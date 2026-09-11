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
    this.hearWaiters = []; // { worker, caller, target, dataPtr, size }
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
    const isAny = !targetName || targetName.length === 0;
    if (!isAny) {
      if (targetName === callerWorker.name) return ERROR_PARAM;
      if (!workerMap.has(targetName)) return ERROR_TARGET;
    }

    if (timeout === 0) return TIMEOUT;

    // Update existing waiter or register new listener waiter
    const existingIdx = this.hearWaiters.findIndex(w => w.worker === callerWorker);
    const waiter = {
      worker: callerWorker,
      caller: callerWorker.name,
      target: isAny ? '' : targetName,
      dataPtr,
      size
    };

    if (existingIdx !== -1) {
      this.hearWaiters[existingIdx] = waiter;
    } else {
      this.hearWaiters.push(waiter);
    }

    return OK;
  }

  tell(callerWorker, targetName, dataPtr, size, timeout, workerMap) {
    const isAny = !targetName || targetName.length === 0;
    if (!isAny) {
      if (targetName === callerWorker.name) return ERROR_PARAM;
      if (!workerMap.has(targetName)) return ERROR_TARGET;

      const targetPeer = workerMap.get(targetName);

      // If target is a RemotePeer, transmit over the wire
      if (targetPeer && targetPeer.isRemote) {
        const dataCopy = new Uint8Array(size);
        if (size > 0 && callerWorker.memory) {
          dataCopy.set(new Uint8Array(callerWorker.memory.buffer, dataPtr, size));
        }
        this.logMessage(callerWorker.name, targetName, size, dataCopy, 'tell');
        targetPeer.sendTell(callerWorker.name, dataCopy);
        return OK;
      }
    }

    // Rendezvous: Find matching HEAR waiting for this TELL
    const matchIdx = this.hearWaiters.findIndex(
      w => w.worker !== callerWorker &&
           (!w.target || w.target === callerWorker.name) &&
           (isAny || w.caller === targetName)
    );

    if (matchIdx !== -1) {
      const match = this.hearWaiters.splice(matchIdx, 1)[0];
      if (size > match.size) return ERROR_SIZE;
      let dataCopy = null;
      if (size > 0 && callerWorker.memory && match.worker.memory) {
        dataCopy = new Uint8Array(size);
        const src = new Uint8Array(callerWorker.memory.buffer, dataPtr, size);
        dataCopy.set(src);
        new Uint8Array(match.worker.memory.buffer, match.dataPtr, size).set(src);
      }
      match.worker.lastIpcSender = callerWorker.name;
      this.logMessage(callerWorker.name, match.caller, size, dataCopy, 'rendezvous');
      return OK;
    }

    // No receiver at the rendezvous point -> TIMEOUT (zero queue)
    return TIMEOUT;
  }

  /**
   * Called when a TELL arrives from a remote connection over the wire.
   */
  receiveRemoteTell(senderName, targetName, payloadBytes) {
    const dataCopy = payloadBytes instanceof Uint8Array ? payloadBytes : new Uint8Array(payloadBytes);

    this.logMessage(senderName, targetName || '*', dataCopy.length, dataCopy, 'recv');

    // Deliver directly to matching HEAR waiter if present
    const matchIdx = this.hearWaiters.findIndex(
      w => (!w.target || w.target === senderName) &&
           (!targetName || w.caller === targetName)
    );

    if (matchIdx !== -1) {
      const match = this.hearWaiters.splice(matchIdx, 1)[0];
      if (match.worker) {
        match.worker.lastIpcSender = senderName;
        if (match.worker.memory) {
          const copyLen = Math.min(match.size, dataCopy.length);
          if (copyLen > 0) {
            new Uint8Array(match.worker.memory.buffer, match.dataPtr, copyLen).set(dataCopy.subarray(0, copyLen));
          }
        }
      }
    }
  }

  clear() {
    this.hearWaiters.length = 0;
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
