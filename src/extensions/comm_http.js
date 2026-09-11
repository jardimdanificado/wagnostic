/**
 * comm:http Extension
 * 
 * HTTP streaming / fetch communication capability.
 * Checks environment before registering/dispatching.
 */

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 144;

class CommHttpExtension {
  constructor() {
    this.name = ['comm:http', 'http'];
  }

  isSupported() {
    return (
      typeof fetch !== 'undefined' ||
      typeof globalThis.fetch !== 'undefined' ||
      (typeof require !== 'undefined' && (function() { try { return !!require('http'); } catch (e) { return false; } })())
    );
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;

    if (!worker.extState.has('comm:http')) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set('comm:http', {
        ptr,
        status: COMM_STATUS_IDLE
      });
    }
    return worker.extState.get('comm:http').ptr;
  }

  onBeforeUpdate(worker, host) {}
  onDestroy(host) {}
}

const commHttpExtension = new CommHttpExtension();

module.exports = {
  CommHttpExtension,
  commHttpExtension
};
