/**
 * Piolho Universal WebWorker / WorkerThread Runner (SAB + Atomics)
 * 
 * Executes an isolated WebAssembly instance inside an independent OS thread,
 * with hardware-level lock-free rendezvous IPC via SharedArrayBuffer & Atomics.
 */

const {
  ShmArena,
  MAX_WORKERS,
  DATA_SLOT_SIZE,
  STATUS_IDLE,
  STATUS_WAITING_HEAR,
  STATUS_WAITING_TELL,
  STATUS_DATA_READY,
  STATUS_MATCHED,
  STATUS_CLOSED,
  OFF_STATUS,
  OFF_TARGET_ID,
  OFF_PAYLOAD_SIZE
} = require('./shm_arena');
const { createDefaultRegistry } = require('./extensions');

let parentPort = null;

if (typeof process !== 'undefined' && process.versions && process.versions.node) {
  try {
    const wt = require('worker_threads');
    parentPort = wt.parentPort;
  } catch (e) {}
}

if (!parentPort && typeof self !== 'undefined') {
  parentPort = {
    postMessage: (msg, transfer) => self.postMessage(msg, transfer),
    on: (evt, handler) => {
      if (evt === 'message') {
        self.onmessage = (e) => handler(e.data);
      }
    }
  };
}

let arena = null;
let wasmInstance = null;
let wasmMemory = null;
let workerName = '';
let workerId = 0;
let isRunning = false;
let arenaOffset = 0;
let extState = new Map();
let activeExtensions = [];
let intervalMs = 1;
let timerId = null;
let extensions = null;

const workerAdapter = {
  get name() { return workerName; },
  get id() { return workerId; },
  get memory() { return wasmMemory; },
  get extState() { return extState; },
  get activeExtensions() { return activeExtensions; },
  alloc(size, align = 4) {
    if (arenaOffset === 0) {
      const blen = wasmMemory ? wasmMemory.buffer.byteLength : 65536;
      if (blen >= 2097152) arenaOffset = blen - 1400000;
      else if (blen >= 1048576) arenaOffset = blen - 400000;
      else arenaOffset = 0x8000;
    }
    if (align > 1) {
      arenaOffset = (arenaOffset + align - 1) & ~(align - 1);
    }
    const ptr = arenaOffset;
    arenaOffset += size;
    return ptr;
  },
  readString(ptr) {
    if (!ptr || !wasmMemory) return '';
    const bytes = new Uint8Array(wasmMemory.buffer, ptr);
    let len = 0;
    while (len < 256 && bytes[len] !== 0) len++;
    return new TextDecoder().decode(bytes.subarray(0, len));
  }
};

const hostAdapter = {
  get extensions() { return extensions; },
  get workers() { return [workerAdapter]; },
  get workerMap() { return new Map([[workerName, workerAdapter]]); },
  peers: {
    registerRemote: () => {},
    registerLocal: () => {},
    has: () => false,
    get: () => null
  },
  ipc: {
    receiveRemoteTell: (s, t, p) => {}
  }
};

function readString(ptr) {
  return workerAdapter.readString(ptr);
}

function threadHear(targetName, dataPtr, size, timeoutMs) {
  const isAny = !targetName || targetName.length === 0;
  const targetId = isAny ? -1 : arena.findWorkerIdByName(targetName);

  if (!isAny) {
    if (targetName === workerName) return -3; // ERROR_PARAM
    if (targetId === -1) return -2; // ERROR_TARGET
  }

  const mySlotOff = arena.getSlotOffset(workerId);
  const myIntIdx = mySlotOff >> 2;
  const myStatusIdx = myIntIdx + (OFF_STATUS >> 2);

  arena.int32[myIntIdx + (OFF_TARGET_ID >> 2)] = targetId;
  arena.int32[myIntIdx + (OFF_PAYLOAD_SIZE >> 2)] = size;

  // Check if someone was already waiting in WAITING_TELL for us
  let matchedSenderId = -1;
  for (let sid = 0; sid < MAX_WORKERS; sid++) {
    if (sid === workerId) continue;
    const sOff = arena.getSlotOffset(sid);
    const sIntIdx = sOff >> 2;
    const sStatus = Atomics.load(arena.int32, sIntIdx + (OFF_STATUS >> 2));
    if (sStatus === STATUS_WAITING_TELL) {
      const sTarget = arena.int32[sIntIdx + (OFF_TARGET_ID >> 2)];
      if (sTarget === -1 || sTarget === workerId) {
        if (isAny || sid === targetId) {
          matchedSenderId = sid;
          break;
        }
      }
    }
  }

  if (matchedSenderId !== -1) {
    const sOff = arena.getSlotOffset(matchedSenderId);
    const sIntIdx = sOff >> 2;
    const sPayloadSize = arena.int32[sIntIdx + (OFF_PAYLOAD_SIZE >> 2)];
    const copyLen = Math.min(size, sPayloadSize);

    if (copyLen > 0 && wasmMemory) {
      const sDataOff = arena.getDataOffset(matchedSenderId);
      new Uint8Array(wasmMemory.buffer, dataPtr, copyLen).set(
        arena.uint8.subarray(sDataOff, sDataOff + copyLen)
      );
    }

    // Wake up sender
    Atomics.store(arena.int32, sIntIdx + (OFF_STATUS >> 2), STATUS_MATCHED);
    Atomics.notify(arena.int32, sIntIdx + (OFF_STATUS >> 2), 1);

    Atomics.store(arena.int32, myStatusIdx, STATUS_IDLE);
    Atomics.add(arena.int32, 4, 1);
    return 0; // OK
  }

  // No active tell found
  if (timeoutMs === 0) {
    Atomics.store(arena.int32, myStatusIdx, STATUS_IDLE);
    return 2; // TIMEOUT
  }

  // Wait on Atomics
  Atomics.store(arena.int32, myStatusIdx, STATUS_WAITING_HEAR);
  const waitTimeout = timeoutMs < 0 ? undefined : timeoutMs;
  Atomics.wait(arena.int32, myStatusIdx, STATUS_WAITING_HEAR, waitTimeout);

  const finalStatus = Atomics.load(arena.int32, myStatusIdx);
  if (finalStatus === STATUS_DATA_READY) {
    const myDataOff = arena.getDataOffset(workerId);
    const inSize = arena.int32[myIntIdx + (OFF_PAYLOAD_SIZE >> 2)];
    const copyLen = Math.min(size, inSize);

    if (copyLen > 0 && wasmMemory) {
      new Uint8Array(wasmMemory.buffer, dataPtr, copyLen).set(
        arena.uint8.subarray(myDataOff, myDataOff + copyLen)
      );
    }
    Atomics.store(arena.int32, myStatusIdx, STATUS_IDLE);
    Atomics.add(arena.int32, 4, 1);
    return 0; // OK
  }

  Atomics.store(arena.int32, myStatusIdx, STATUS_IDLE);
  return 2; // TIMEOUT
}

function threadTell(targetName, dataPtr, size, timeoutMs) {
  const isAny = !targetName || targetName.length === 0;
  const targetId = isAny ? -1 : arena.findWorkerIdByName(targetName);

  if (!isAny) {
    if (targetName === workerName) return -3; // ERROR_PARAM
    if (targetId === -1) return -2; // ERROR_TARGET
  }

  const mySlotOff = arena.getSlotOffset(workerId);
  const myIntIdx = mySlotOff >> 2;
  const myStatusIdx = myIntIdx + (OFF_STATUS >> 2);

  // Check if target is WAITING_HEAR
  let matchedReceiverId = -1;
  if (!isAny && targetId !== -1) {
    const tOff = arena.getSlotOffset(targetId);
    const tIntIdx = tOff >> 2;
    const tStatus = Atomics.load(arena.int32, tIntIdx + (OFF_STATUS >> 2));
    if (tStatus === STATUS_WAITING_HEAR) {
      const tExpected = arena.int32[tIntIdx + (OFF_TARGET_ID >> 2)];
      if (tExpected === -1 || tExpected === workerId) {
        matchedReceiverId = targetId;
      }
    }
  } else {
    for (let tid = 0; tid < MAX_WORKERS; tid++) {
      if (tid === workerId) continue;
      const tOff = arena.getSlotOffset(tid);
      const tIntIdx = tOff >> 2;
      const tStatus = Atomics.load(arena.int32, tIntIdx + (OFF_STATUS >> 2));
      if (tStatus === STATUS_WAITING_HEAR) {
        const tExpected = arena.int32[tIntIdx + (OFF_TARGET_ID >> 2)];
        if (tExpected === -1 || tExpected === workerId) {
          matchedReceiverId = tid;
          break;
        }
      }
    }
  }

  if (matchedReceiverId !== -1) {
    const tOff = arena.getSlotOffset(matchedReceiverId);
    const tIntIdx = tOff >> 2;
    const tDataOff = arena.getDataOffset(matchedReceiverId);
    const copyLen = Math.min(size, DATA_SLOT_SIZE);

    if (copyLen > 0 && wasmMemory) {
      arena.uint8.set(
        new Uint8Array(wasmMemory.buffer, dataPtr, copyLen),
        tDataOff
      );
    }
    arena.int32[tIntIdx + (OFF_PAYLOAD_SIZE >> 2)] = copyLen;

    // Wake up receiver
    Atomics.store(arena.int32, tIntIdx + (OFF_STATUS >> 2), STATUS_DATA_READY);
    Atomics.notify(arena.int32, tIntIdx + (OFF_STATUS >> 2), 1);
    Atomics.add(arena.int32, 4, 1);
    return 0; // OK
  }

  // Zero-queue: timeout == 0 returns TIMEOUT
  if (timeoutMs === 0) {
    return 2; // TIMEOUT
  }

  // Write payload to our data slot and wait
  const myDataOff = arena.getDataOffset(workerId);
  const copyLen = Math.min(size, DATA_SLOT_SIZE);
  if (copyLen > 0 && wasmMemory) {
    arena.uint8.set(new Uint8Array(wasmMemory.buffer, dataPtr, copyLen), myDataOff);
  }
  arena.int32[myIntIdx + (OFF_TARGET_ID >> 2)] = targetId;
  arena.int32[myIntIdx + (OFF_PAYLOAD_SIZE >> 2)] = copyLen;

  Atomics.store(arena.int32, myStatusIdx, STATUS_WAITING_TELL);
  const waitTimeout = timeoutMs < 0 ? undefined : timeoutMs;
  Atomics.wait(arena.int32, myStatusIdx, STATUS_WAITING_TELL, waitTimeout);

  const finalStatus = Atomics.load(arena.int32, myStatusIdx);
  Atomics.store(arena.int32, myStatusIdx, STATUS_IDLE);

  if (finalStatus === STATUS_MATCHED) {
    return 0; // OK
  }
  return 2; // TIMEOUT
}

function getImportObject() {
  return {
    env: {
      memory: new WebAssembly.Memory({ initial: 16 }),

      ask: (namePtr) => {
        const name = readString(namePtr);
        if (!name) return 0;
        if (extensions) {
          return extensions.dispatch(workerAdapter, hostAdapter, name);
        }
        return 0;
      },

      tell: (targetPtr, dataPtr, size, timeout) => {
        const target = readString(targetPtr);
        return threadTell(target, dataPtr, size, timeout);
      },

      hear: (targetPtr, dataPtr, size, timeout) => {
        const target = readString(targetPtr);
        return threadHear(target, dataPtr, size, timeout);
      },

      quit: (code) => {
        isRunning = false;
        parentPort.postMessage({ type: 'exit', code });
      },

      use: (namePtr) => {
        const name = readString(namePtr);
        if (extensions) return extensions.dispatch(workerAdapter, hostAdapter, name);
        return 0;
      },
      wextension: (namePtr) => {
        const name = readString(namePtr);
        if (extensions) return extensions.dispatch(workerAdapter, hostAdapter, name);
        return 0;
      },
      wtell: (t, d, s, to) => threadTell(readString(t), d, s, to),
      whear: (t, d, s, to) => threadHear(readString(t), d, s, to),
      wexit: (c) => { isRunning = false; }
    },
    wasi_snapshot_preview1: {
      proc_exit: (code) => {
        isRunning = false;
        parentPort.postMessage({ type: 'exit', code });
      }
    }
  };
}

async function initWorker(data) {
  workerName = data.name;
  workerId = data.id;
  intervalMs = data.intervalMs !== undefined ? data.intervalMs : 1;
  arena = new ShmArena(data.sharedBuffer);
  arena.registerWorker(workerId, workerName);

  extensions = createDefaultRegistry();

  const importObj = getImportObject();
  const wasmModule = await WebAssembly.instantiate(data.wasmBytes, importObj);
  wasmInstance = wasmModule.instance;
  wasmMemory = wasmInstance.exports.memory || importObj.env.memory;
  isRunning = true;

  parentPort.postMessage({ type: 'ready', name: workerName });

  if (data.autoRun !== false) {
    startLoop();
  }
}

function step() {
  if (!isRunning || !wasmInstance) return;
  const updateFn = wasmInstance.exports.update || wasmInstance.exports.step || wasmInstance.exports.wupdate;
  if (typeof updateFn === 'function') {
    try {
      if (extensions) extensions.onBeforeUpdate(workerAdapter, hostAdapter);
      const status = updateFn();
      if (extensions) extensions.onAfterUpdate(workerAdapter, hostAdapter);

      if (status === 1) { // EXIT
        isRunning = false;
        if (timerId) clearInterval(timerId);
        arena.closeWorker(workerId);
        parentPort.postMessage({ type: 'exit', code: 0 });
      } else if (status < 0) {
        isRunning = false;
        if (timerId) clearInterval(timerId);
        arena.closeWorker(workerId);
        parentPort.postMessage({ type: 'error', status });
      }
    } catch (e) {
      isRunning = false;
      if (timerId) clearInterval(timerId);
      arena.closeWorker(workerId);
      parentPort.postMessage({ type: 'error', error: e.message });
    }
  }
}

function startLoop() {
  const tick = () => {
    if (!isRunning) return;
    // Pump a batch of steps to eliminate event loop overhead
    const batch = intervalMs === 0 ? 50 : 1;
    for (let i = 0; i < batch && isRunning; i++) {
      step();
    }
    if (isRunning) {
      if (intervalMs === 0) {
        if (typeof setImmediate !== 'undefined') setImmediate(tick);
        else setTimeout(tick, 0);
      } else {
        timerId = setTimeout(tick, intervalMs);
        if (timerId && typeof timerId.unref === 'function') timerId.unref();
      }
    }
  };
  tick();
}

parentPort.on('message', async (msg) => {
  if (!msg) return;
  if (msg.type === 'init') {
    await initWorker(msg);
  } else if (msg.type === 'step') {
    step();
  } else if (msg.type === 'stop') {
    isRunning = false;
    if (timerId) clearInterval(timerId);
    if (arena) arena.closeWorker(workerId);
    if (typeof process !== 'undefined' && process.exit) process.exit(0);
  }
});
