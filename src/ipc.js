/**
 * Piolho Synchronous & Distributed Rendezvous IPC Engine
 * 
 * Implements hear() and tell() rendezvous matching across local WASM workers
 * and discovered remote peers (TCP, Pipe, WS, UDP).
 */

const OK              = 0;
const DONE            = 1;
const TIMEOUT         = 2;
const ERROR           = -1;
const ERROR_TARGET    = -2;
const ERROR_PARAM     = -3;
const ERROR_SIZE      = -4;
const ERROR_CLOSED    = -5;
const ERROR_STATE     = -6;

class IpcEngine {
  constructor() {
    this.pendingOps = []; // { worker, caller, target, dataPtr, size, opType ('HEAR'|'TELL'), data: Uint8Array }
    this.onMessage = null; // ({ sender, target, size, data, op }) => void
  }

  logMessage(sender, target, size, data, op) {
    if (typeof this.onMessage === 'function') {
      this.onMessage({ sender, target, size, data, op });
    }
  }

  ask(callerWorker, targetName, dataPtr, size, timeout, workerMap) {
    return this.hear(callerWorker, targetName, dataPtr, size, timeout, workerMap);
  }

  hear(callerWorker, targetName, dataPtr, size, timeout, workerMap) {
    const isAny = !targetName;
    if (!isAny) {
      if (targetName === callerWorker.name) return ERROR_PARAM;
      if (!workerMap.has(targetName)) return ERROR_TARGET;
    }

    // Check if there is already a matching TELL waiting for this HEAR
    const matchIdx = this.pendingOps.findIndex(
      op => op.opType === 'TELL' &&
            (isAny || op.caller === targetName) &&
            (!op.target || op.target === callerWorker.name)
    );

    if (matchIdx !== -1) {
      const match = this.pendingOps.splice(matchIdx, 1)[0];
      if (match.size > size) return ERROR_SIZE;
      callerWorker.lastIpcSender = match.caller;
      if (match.size > 0 && match.data) {
        new Uint8Array(callerWorker.memory.buffer, dataPtr, match.size).set(match.data);
      }
      this.logMessage(match.caller, callerWorker.name, match.size, match.data, 'hear');
      return OK;
    }

    if (timeout === 0) return TIMEOUT;

    // Register waiter
    this.pendingOps.push({
      worker: callerWorker,
      caller: callerWorker.name,
      target: isAny ? '' : targetName,
      dataPtr,
      size,
      opType: 'HEAR'
    });
    return OK;
  }

  tell(callerWorker, targetName, dataPtr, size, timeout, workerMap) {
    if (!targetName || targetName === callerWorker.name) return ERROR_PARAM;
    if (!workerMap.has(targetName)) return ERROR_TARGET;

    const targetPeer = workerMap.get(targetName);

    // If target is a RemotePeer, transmit over the wire
    if (targetPeer && targetPeer.isRemote) {
      const dataCopy = new Uint8Array(size);
      if (size > 0) {
        dataCopy.set(new Uint8Array(callerWorker.memory.buffer, dataPtr, size));
      }
      this.logMessage(callerWorker.name, targetName, size, dataCopy, 'tell');
      targetPeer.sendTell(callerWorker.name, dataCopy);
      return OK;
    }

    // Check if there is already a matching HEAR waiting for this TELL
    const matchIdx = this.pendingOps.findIndex(
      op => op.opType === 'HEAR' &&
            (!op.target || op.target === callerWorker.name) &&
            op.caller === targetName
    );

    if (matchIdx !== -1) {
      const match = this.pendingOps.splice(matchIdx, 1)[0];
      if (size > match.size) return ERROR_SIZE;
      let dataCopy = null;
      if (size > 0) {
        dataCopy = new Uint8Array(size);
        const src = new Uint8Array(callerWorker.memory.buffer, dataPtr, size);
        dataCopy.set(src);
        new Uint8Array(match.worker.memory.buffer, match.dataPtr, size).set(src);
      }
      match.worker.lastIpcSender = callerWorker.name;
      this.logMessage(callerWorker.name, match.caller, size, dataCopy, 'tell');
      return OK;
    }

    if (timeout === 0) return TIMEOUT;

    // Copy data immediately so local stack variable on caller side isn't clobbered
    const dataCopy = new Uint8Array(size);
    if (size > 0) {
      dataCopy.set(new Uint8Array(callerWorker.memory.buffer, dataPtr, size));
    }

    this.logMessage(callerWorker.name, targetName, size, dataCopy, 'tell');

    this.pendingOps.push({
      worker: callerWorker,
      caller: callerWorker.name,
      target: targetName,
      dataPtr,
      size,
      data: dataCopy,
      opType: 'TELL'
    });
    return OK;
  }

  /**
   * Called when a TELL arrives from a remote connection over the wire.
   */
  receiveRemoteTell(senderName, targetName, payloadBytes) {
    const dataCopy = payloadBytes instanceof Uint8Array ? payloadBytes : new Uint8Array(payloadBytes);

    this.logMessage(senderName, targetName || '*', dataCopy.length, dataCopy, 'recv');

    // Check if there is a HEAR waiting for this message
    const matchIdx = this.pendingOps.findIndex(
      op => op.opType === 'HEAR' &&
            (!op.target || op.target === senderName) &&
            (!targetName || op.caller === targetName)
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
    OK,
    DONE,
    EXIT: DONE,
    TIMEOUT,
    ERROR,
    ERROR_TARGET,
    ERROR_PARAM,
    ERROR_SIZE,
    ERROR_CLOSED,
    ERROR_STATE,
    // Aliases
    ERROR_SHUTDOWN: ERROR_CLOSED,
    ERR: ERROR,
    ERR_TARGET: ERROR_TARGET,
    ERR_PARAM: ERROR_PARAM,
    ERR_SIZE: ERROR_SIZE,
    ERR_CLOSED: ERROR_CLOSED,
    WIPC_OK: OK,
    WIPC_TIMEOUT: TIMEOUT,
    WIPC_ERROR: ERROR,
    WIPC_TARGET: ERROR_TARGET,
    WIPC_PARAM: ERROR_PARAM,
    WIPC_SIZE: ERROR_SIZE
  };
}
