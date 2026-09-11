/**
 * Standard Clock Extension: std:clock
 */

const clockExtension = {
  name: ['std:clock', 'clock'],

  onRequest(worker, host) {
    let state = worker.extState.get('std:clock');
    if (!state) {
      const clockPtr = worker.alloc(24, 8);
      const view = new DataView(worker.memory.buffer, clockPtr, 24);
      view.setBigUint64(0, 0n, true);
      view.setBigUint64(8, 1000n, true);
      view.setFloat32(16, 1.0 / host.targetFps, true);

      state = { clockPtr };
      worker.extState.set('std:clock', state);
    }
    return state.clockPtr;
  },

  onBeforeUpdate(worker, host) {
    const state = worker.extState.get('std:clock');
    if (!state || !state.clockPtr || !worker.memory) return;

    if (state.clockPtr + 24 <= worker.memory.buffer.byteLength) {
      const view = new DataView(worker.memory.buffer, state.clockPtr, 24);
      const now = Date.now();
      const elapsedMs = now - host.startTime;
      const deltaSec = (now - host.lastTime) / 1000.0;

      view.setBigUint64(0, BigInt(elapsedMs), true);
      view.setFloat32(16, deltaSec > 0 ? deltaSec : 1.0 / host.targetFps, true);
    }
  }
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { clockExtension };
}
