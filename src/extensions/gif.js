/**
 * Standard GIF Extension: std:gif
 */

const { ENV } = require('../env');
const { MinimalGifEncoder } = require('../gif');
const { getFramebuffer } = require('./framebuffer');

function createGifExtension(options = {}) {
  const config = typeof options === 'string' ? { path: options } : options;
  const outputPath = config.path || 'output.gif';
  const fps = config.fps || 30;
  const maxFrames = config.maxFrames || config.maxSteps || 0;
  const delay = config.delay || Math.round(100 / fps);

  let encoder = null;
  let capturedFrames = 0;

  return {
    name: ['std:gif', 'gif'],

    onRequest(worker, host) {
      let state = worker.extState.get('std:gif');
      if (!state) {
        const gifPtr = worker.alloc(20, 4);
        const view = new DataView(worker.memory.buffer, gifPtr, 20);
        view.setUint32(0, 1, true); // active
        view.setUint32(4, 0, true);
        view.setUint32(8, maxFrames, true);
        view.setUint32(12, 2, true);
        view.setUint32(16, 0, true);

        state = { gifPtr };
        worker.extState.set('std:gif', state);
      }
      return state.gifPtr;
    },

    onPostStep(host) {
      if (maxFrames > 0 && capturedFrames >= maxFrames) return;

      const primary = host.workers.find(w => w.extState.has('std:framebuffer'));
      if (!primary) return;

      const fb = getFramebuffer(primary);
      if (!fb) return;

      if (!encoder) {
        encoder = new MinimalGifEncoder(fb.width, fb.height, delay);
      }
      encoder.addFrame(fb.pixels);
      capturedFrames++;
    },

    onDestroy(host) {
      if (encoder && encoder.frames.length > 0) {
        try {
          const gifData = encoder.save();
          ENV.writeFile(outputPath, gifData);
          console.log(`[GIF] Saved ${encoder.frames.length} frames to ${outputPath}`);
        } catch (err) {
          console.error('Failed to save GIF:', err.message);
        }
      }
    }
  };
}

const gifExtension = createGifExtension();

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { gifExtension, createGifExtension };
}
