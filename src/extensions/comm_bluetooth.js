/**
 * comm:bluetooth Extension
 * 
 * Bluetooth Low Energy communication (Web Bluetooth API).
 * Checks environment before registering/dispatching.
 */

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 144;

class CommBluetoothExtension {
  constructor() {
    this.name = ['comm:bluetooth', 'bluetooth'];
  }

  isSupported() {
    return typeof navigator !== 'undefined' && 'bluetooth' in navigator;
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;

    if (!worker.extState.has('comm:bluetooth')) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set('comm:bluetooth', {
        ptr,
        device: null,
        status: COMM_STATUS_IDLE
      });
    }
    return worker.extState.get('comm:bluetooth').ptr;
  }

  onBeforeUpdate(worker, host) {}
  onDestroy(host) {}
}

const commBluetoothExtension = new CommBluetoothExtension();

module.exports = {
  CommBluetoothExtension,
  commBluetoothExtension
};
