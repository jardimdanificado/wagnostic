/**
 * comm:shm Extension
 * 
 * SharedArrayBuffer & Atomics capability for zero-copy memory communication.
 * Checks environment before registering/dispatching.
 */

const STRUCT_SIZE = 16;

class CommShmExtension {
  constructor() {
    this.name = ['comm:shm', 'shm'];
  }

  isSupported() {
    return typeof SharedArrayBuffer !== 'undefined' && typeof Atomics !== 'undefined';
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;

    if (!worker.extState.has('comm:shm')) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      const view = new DataView(worker.memory.buffer, ptr, STRUCT_SIZE);
      view.setInt32(0, 2, true); // Status: CONNECTED
      worker.extState.set('comm:shm', { ptr });
    }
    return worker.extState.get('comm:shm').ptr;
  }

  onBeforeUpdate(worker, host) {}
  onDestroy(host) {}
}

const commShmExtension = new CommShmExtension();

module.exports = {
  CommShmExtension,
  commShmExtension
};
