/**
 * Piolho Synchronous & Distributed Rendezvous IPC Engine
 * 
 * Implements wask() and wtell() rendezvous matching across local WASM workers
 * and discovered remote peers (TCP, Pipe, WS, UDP).
 */

const WIPC_OK = 1;
const WIPC_TIMEOUT = 0;
const WIPC_ERROR = -1;
const WIPC_TARGET = -2;
const WIPC_PARAM = -3;
const WIPC_SIZE = -4;

class IpcEngine {
  constructor() {
    this.pendingOps = []; // { worker, caller, target, dataPtr, size, opType ('ASK'|'TELL'), data: Uint8Array }
  }

  ask(callerWorker, targetName, dataPtr, size, timeout, workerMap) {
    const isAny = !targetName;
    if (!isAny) {
      if (targetName === callerWorker.name) return WIPC_PARAM;
      if (!workerMap.has(targetName)) return WIPC_TARGET;
    }

    // Check if there is already a matching TELL waiting for this ASK
    const matchIdx = this.pendingOps.findIndex(
      op => op.opType === 'TELL' && (isAny || op.caller === targetName) && (!op.target || op.target === callerWorker.name)
    );

    if (matchIdx !== -1) {
      const match = this.pendingOps.splice(matchIdx, 1)[0];
      if (match.size > size) return WIPC_SIZE;
      callerWorker.lastIpcSender = match.caller;
      if (match.size > 0 && match.data) {
        new Uint8Array(callerWorker.memory.buffer, dataPtr, match.size).set(match.data);
      }
      return WIPC_OK;
    }

    if (timeout === 0) return WIPC_TIMEOUT;

    // Register waiter
    this.pendingOps.push({
      worker: callerWorker,
      caller: callerWorker.name,
      target: isAny ? '' : targetName,
      dataPtr,
      size,
      opType: 'ASK'
    });
    return WIPC_OK;
  }

  tell(callerWorker, targetName, dataPtr, size, timeout, workerMap) {
    if (!targetName || targetName === callerWorker.name) return WIPC_PARAM;
    if (!workerMap.has(targetName)) return WIPC_TARGET;

    const targetPeer = workerMap.get(targetName);

    // If target is a RemotePeer, transmit over the wire
    if (targetPeer && targetPeer.isRemote) {
      const dataCopy = new Uint8Array(size);
      if (size > 0) {
        dataCopy.set(new Uint8Array(callerWorker.memory.buffer, dataPtr, size));
      }
      targetPeer.sendTell(callerWorker.name, dataCopy);
      return WIPC_OK;
    }

    // Check if there is already a matching ASK waiting for this TELL
    const matchIdx = this.pendingOps.findIndex(
      op => op.opType === 'ASK' && (!op.target || op.target === callerWorker.name) && op.caller === targetName
    );

    if (matchIdx !== -1) {
      const match = this.pendingOps.splice(matchIdx, 1)[0];
      if (size > match.size) return WIPC_SIZE;
      if (size > 0) {
        const src = new Uint8Array(callerWorker.memory.buffer, dataPtr, size);
        new Uint8Array(match.worker.memory.buffer, match.dataPtr, size).set(src);
      }
      return WIPC_OK;
    }

    if (timeout === 0) return WIPC_TIMEOUT;

    // Copy data immediately so local stack variable on caller side isn't clobbered
    const dataCopy = new Uint8Array(size);
    if (size > 0) {
      dataCopy.set(new Uint8Array(callerWorker.memory.buffer, dataPtr, size));
    }

    this.pendingOps.push({
      worker: callerWorker,
      caller: callerWorker.name,
      target: targetName,
      dataPtr,
      size,
      data: dataCopy,
      opType: 'TELL'
    });
    return WIPC_OK;
  }

  /**
   * Called when a TELL arrives from a remote connection over the wire.
   */
  receiveRemoteTell(senderName, targetName, payloadBytes) {
    const dataCopy = payloadBytes instanceof Uint8Array ? payloadBytes : new Uint8Array(payloadBytes);

    // Check if there is an ASK waiting for this message
    const matchIdx = this.pendingOps.findIndex(
      op => op.opType === 'ASK' && (!op.target || op.target === senderName) && (!targetName || op.caller === targetName)
    );

    if (matchIdx !== -1) {
      const match = this.pendingOps.splice(matchIdx, 1)[0];
      if (match.worker) {
        match.worker.lastIpcSender = senderName;
        if (match.worker.memory) {
          const copyLen = Math.min(match.size, dataCopy.length);
          if (copyLen > 0) {
            new Uint8Array(match.worker.memory.buffer, match.dataPtr, copyLen).set(dataCopy.subarray(0, copyLen));
          }
        }
      }
      return;
    }

    // Otherwise store as pending TELL
    this.pendingOps.push({
      worker: null,
      caller: senderName,
      target: targetName || '',
      dataPtr: 0,
      size: dataCopy.length,
      data: dataCopy,
      opType: 'TELL'
    });
  }

  clear() {
    this.pendingOps.length = 0;
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    IpcEngine,
    WIPC_OK,
    WIPC_TIMEOUT,
    WIPC_ERROR,
    WIPC_TARGET,
    WIPC_PARAM,
    WIPC_SIZE
  };
}
