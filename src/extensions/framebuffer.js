/**
 * Standard Framebuffer Extension: std:framebuffer / std:surface
 */

const framebufferExtension = {
  name: ['std:framebuffer', 'framebuffer', 'std:surface', 'surface'],

  onRequest(worker, host) {
    let state = worker.extState.get('std:framebuffer');
    if (!state) {
      const fbPtr = worker.alloc(12, 4);
      const defaultFbPtr = worker.alloc(640 * 480 * 4, 4);
      const view = new DataView(worker.memory.buffer, fbPtr, 12);
      view.setUint32(0, 320, true);
      view.setUint32(4, 240, true);
      view.setUint32(8, defaultFbPtr, true);

      state = { fbPtr, defaultFbPtr };
      worker.extState.set('std:framebuffer', state);
    }
    return state.fbPtr;
  }
};

function getFramebuffer(worker) {
  const state = worker.extState.get('std:framebuffer');
  if (!state || !state.fbPtr || !worker.memory) return null;

  if (state.fbPtr + 12 > worker.memory.buffer.byteLength) return null;
  const view = new DataView(worker.memory.buffer, state.fbPtr, 12);
  const width = view.getUint32(0, true) || 320;
  const height = view.getUint32(4, true) || 240;
  const pixelsPtr = view.getUint32(8, true);

  if (!pixelsPtr || width === 0 || height === 0 || pixelsPtr + width * height * 4 > worker.memory.buffer.byteLength) {
    return null;
  }

  const pixels = new Uint32Array(worker.memory.buffer, pixelsPtr, width * height);
  return { width, height, pixelsPtr, pixels };
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { framebufferExtension, getFramebuffer };
}
