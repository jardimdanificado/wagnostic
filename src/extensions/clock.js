/**
 * Clock Extension: clock
 */

const clockExtension = {
  name: 'clock',

  onRequest(worker, host) {
    let state = worker.extState.get('clock');
    if (!state) {
      const clockPtr = worker.alloc(24, 8);
      const view = new DataView(worker.memory.buffer, clockPtr, 24);
      const defaultDelta = host.intervalMs ? host.intervalMs / 1000.0 : (1.0 / (host.targetFps || 30));
      view.setBigUint64(0, 0n, true);
      view.setBigUint64(8, 1000n, true);
      view.setFloat32(16, defaultDelta, true);

      state = { clockPtr };
      worker.extState.set('clock', state);
    }
    return state.clockPtr;
  },

  onBeforeUpdate(worker, host) {
    const state = worker.extState.get('clock');
    if (!state || !state.clockPtr || !worker.memory) return;

    if (state.clockPtr + 24 <= worker.memory.buffer.byteLength) {
      const view = new DataView(worker.memory.buffer, state.clockPtr, 24);
      const now = Date.now();
      const elapsedMs = now - host.startTime;
      const deltaSec = (now - host.lastTime) / 1000.0;
      const fallbackDelta = host.intervalMs ? host.intervalMs / 1000.0 : 0.016;

      view.setBigUint64(0, BigInt(elapsedMs), true);
      view.setFloat32(16, deltaSec > 0 ? deltaSec : fallbackDelta, true);
    }
  }
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { clockExtension };
}
