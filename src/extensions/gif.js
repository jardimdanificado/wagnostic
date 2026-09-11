/**
 * Standard GIF Extension: std:gif
 */

const { ENV } = require('../env');
const { MinimalGifEncoder } = require('../gif');
const { getFramebuffer } = require('./framebuffer');

const gifExtension = {
  name: ['std:gif', 'gif'],

  onRequest(worker, host) {
    let state = worker.extState.get('std:gif');
    if (!state) {
      const gifPtr = worker.alloc(20, 4);
      const view = new DataView(worker.memory.buffer, gifPtr, 20);
      view.setUint32(0, (host.gifPath || host.maxFrames > 0) ? 1 : 0, true);
      view.setUint32(4, 0, true);
      view.setUint32(8, host.maxFrames, true);
      view.setUint32(12, 2, true);
      view.setUint32(16, 0, true);

      state = { gifPtr };
      worker.extState.set('std:gif', state);
    }
    return state.gifPtr;
  },

  onFrameComplete(host) {
    if (!host.gifPath) return;

    // Find primary worker with active framebuffer
    const primary = host.workers.find(w => w.extState.has('std:framebuffer'));
    if (!primary) return;

    const fb = getFramebuffer(primary);
    if (!fb) return;

    if (!host.gifEncoder) {
      host.gifEncoder = new MinimalGifEncoder(fb.width, fb.height, Math.round(100 / host.targetFps));
    }
    if (host.gifEncoder) {
      host.gifEncoder.addFrame(fb.pixels);
    }
  },

  onDestroy(host) {
    if (host.gifPath && host.gifEncoder && host.gifEncoder.frames.length > 0) {
      try {
        const gifData = host.gifEncoder.save();
        ENV.writeFile(host.gifPath, gifData);
        console.log(`[GIF] Saved ${host.gifEncoder.frames.length} frames to ${host.gifPath}`);
      } catch (err) {
        console.error('Failed to save GIF:', err.message);
      }
    }
  }
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { gifExtension };
}
