/**
 * comm:serial Extension
 * 
 * Serial port communication (Web Serial API / serialport).
 * Checks environment before registering/dispatching.
 */

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 144;

class CommSerialExtension {
  constructor() {
    this.name = ['comm:serial', 'serial'];
  }

  isSupported() {
    return (
      (typeof navigator !== 'undefined' && 'serial' in navigator) ||
      (typeof require !== 'undefined' && (function() { try { return !!require('serialport'); } catch (e) { return false; } })())
    );
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;

    if (!worker.extState.has('comm:serial')) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set('comm:serial', {
        ptr,
        port: null,
        status: COMM_STATUS_IDLE
      });
    }
    return worker.extState.get('comm:serial').ptr;
  }

  onBeforeUpdate(worker, host) {}
  onDestroy(host) {}
}

const commSerialExtension = new CommSerialExtension();

module.exports = {
  CommSerialExtension,
  commSerialExtension
};
