/**
 * Standard Mouse Extension: std:mouse
 */

const mouseExtension = {
  name: ['std:mouse', 'mouse'],

  onRequest(worker, host) {
    let state = worker.extState.get('std:mouse');
    if (!state) {
      const mousePtr = worker.alloc(20, 4);
      new Uint8Array(worker.memory.buffer, mousePtr, 20).fill(0);
      state = { mousePtr };
      worker.extState.set('std:mouse', state);
    }
    return state.mousePtr;
  },

  onBeforeUpdate(worker, host) {
    const state = worker.extState.get('std:mouse');
    if (!state || !state.mousePtr || !worker.memory) return;

    if (state.mousePtr + 20 <= worker.memory.buffer.byteLength) {
      const view = new DataView(worker.memory.buffer, state.mousePtr, 20);
      view.setInt32(0, host.mouseX || 0, true);
      view.setInt32(4, host.mouseY || 0, true);
      view.setUint32(8, host.mouseButtons || 0, true);
      view.setInt32(12, host.mouseWheelX || 0, true);
      view.setInt32(16, host.mouseWheelY || 0, true);
    }
  }
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { mouseExtension };
}
