/**
 * Standard Keyboard Extension: std:keyboard
 */

const keyboardExtension = {
  name: ['std:keyboard', 'keyboard'],

  onRequest(worker, host) {
    let state = worker.extState.get('std:keyboard');
    if (!state) {
      const keyboardPtr = worker.alloc(256, 4);
      new Uint8Array(worker.memory.buffer, keyboardPtr, 256).fill(0);
      state = { keyboardPtr };
      worker.extState.set('std:keyboard', state);
    }
    return state.keyboardPtr;
  },

  onBeforeUpdate(worker, host) {
    const state = worker.extState.get('std:keyboard');
    if (!state || !state.keyboardPtr || !worker.memory) return;

    if (state.keyboardPtr + 256 <= worker.memory.buffer.byteLength) {
      const target = new Uint8Array(worker.memory.buffer, state.keyboardPtr, 256);
      if (host.keyState) {
        target.set(host.keyState);
      }
    }
  }
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { keyboardExtension };
}
