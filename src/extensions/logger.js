/**
 * Standard Logger Extension: logger
 */

const loggerExtension = {
  name: 'logger',

  onRequest(worker) {
    let state = worker.extState.get('logger');
    if (!state) {
      const loggerPtr = worker.alloc(12, 4);
      const loggerBufPtr = worker.alloc(1024, 4);
      const view = new DataView(worker.memory.buffer, loggerPtr, 12);
      view.setUint32(0, loggerBufPtr, true);
      view.setUint32(4, 1024, true);
      view.setUint32(8, 0, true);

      state = { loggerPtr, loggerBufPtr };
      worker.extState.set('logger', state);
    }
    return state.loggerPtr;
  },

  onAfterUpdate(worker) {
    const state = worker.extState.get('logger');
    if (!state || !state.loggerPtr || !worker.memory) return;

    if (state.loggerPtr + 12 <= worker.memory.buffer.byteLength) {
      const view = new DataView(worker.memory.buffer, state.loggerPtr, 12);
      const len = view.getUint32(8, true);
      if (len > 0 && state.loggerBufPtr && state.loggerBufPtr + len <= worker.memory.buffer.byteLength) {
        const textBytes = new Uint8Array(worker.memory.buffer, state.loggerBufPtr, len);
        const str = new TextDecoder().decode(textBytes);
        console.log(`[${worker.name} Log] ${str}`);
        view.setUint32(8, 0, true); // Reset length after flushing
      }
    }
  }
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { loggerExtension };
}
