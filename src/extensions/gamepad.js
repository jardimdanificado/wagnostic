/**
 * Standard Gamepad Extension: std:gamepad
 */

const gamepadExtension = {
  name: ['std:gamepad', 'gamepad'],

  onRequest(worker, host) {
    let state = worker.extState.get('std:gamepad');
    if (!state) {
      const gamepadPtr = worker.alloc(20, 4);
      new Uint8Array(worker.memory.buffer, gamepadPtr, 20).fill(0);
      state = { gamepadPtr };
      worker.extState.set('std:gamepad', state);
    }
    return state.gamepadPtr;
  },

  onBeforeUpdate(worker, host) {
    const state = worker.extState.get('std:gamepad');
    if (!state || !state.gamepadPtr || !worker.memory) return;

    if (state.gamepadPtr + 20 <= worker.memory.buffer.byteLength) {
      const view = new DataView(worker.memory.buffer, state.gamepadPtr, 20);
      view.setUint32(0, host.gamepadMask || 0, true);
      if (host.gamepadAxes) {
        new Int16Array(worker.memory.buffer, state.gamepadPtr + 4, 8).set(host.gamepadAxes);
      }
    }
  }
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { gamepadExtension };
}
