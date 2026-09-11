/**
 * Wagnostic Host Runner
 * 
 * Coordinates workers, frame loops, clocks, input polling, logger, and GIF export.
 */

const { ENV } = require('./env');
const { extractFromTar } = require('./tar');
const { MinimalGifEncoder } = require('./gif');
const { ExtensionRegistry, defaultRegistry } = require('./extensions');
const { IpcEngine } = require('./ipc');
const { WWorker } = require('./worker');

class WagnosticHost {
  constructor(options = {}) {
    this.maxFrames = (options.maxFrames !== undefined) ? options.maxFrames : 1;
    this.targetFps = options.targetFps || 30;
    this.gifPath = options.gifPath || null;
    this.extensions = options.extensions || defaultRegistry;

    this.workers = [];
    this.workerMap = new Map();
    this.ipc = new IpcEngine();
    this.gifEncoder = null;
    this.isRunning = false;
    this.startTime = 0;
    this.lastTime = 0;
    this.frameCount = 0;

    // Shared input state
    this.keyState = new Uint8Array(256);
    this.mouseX = 0;
    this.mouseY = 0;
    this.mouseButtons = 0;
    this.gamepadMask = 0;
    this.gamepadAxes = new Int16Array(8);
  }

  async loadRom(spec) {
    let filePath = spec;
    let name = '';

    if (spec.includes(':')) {
      const parts = spec.split(':');
      filePath = parts[0];
      name = parts[1];
    } else {
      name = filePath.split('/').pop().replace(/\.wasm$|\.tar$/, '');
    }

    let rawBytes;
    try {
      rawBytes = ENV.readFile(filePath);
    } catch (e) {
      throw new Error(`Failed to open ROM file: ${filePath}`);
    }

    let wasmBytes = rawBytes;
    const extractedWasm = extractFromTar(rawBytes, 'main.wasm');
    if (extractedWasm) wasmBytes = extractedWasm;

    const workerId = this.workers.length + 1;
    const worker = new WWorker(workerId, name, filePath, this);

    this.workers.push(worker);
    this.workerMap.set(name, worker);

    await worker.instantiate(wasmBytes);
    return worker;
  }

  captureFrame() {
    if (!this.gifPath) return;
    const primary = this.workers.find(w => w.fbPtr && w.fbPtr + 12 <= w.memory.buffer.byteLength);
    if (!primary) return;

    const fbView = new DataView(primary.memory.buffer, primary.fbPtr, 12);
    const fbW = fbView.getUint32(0, true) || 320;
    const fbH = fbView.getUint32(4, true) || 240;
    const pixelsPtr = fbView.getUint32(8, true);

    if (!pixelsPtr || fbW === 0 || fbH === 0 || pixelsPtr + fbW * fbH * 4 > primary.memory.buffer.byteLength) return;

    const pixels = new Uint32Array(primary.memory.buffer, pixelsPtr, fbW * fbH);

    if (!this.gifEncoder) {
      this.gifEncoder = new MinimalGifEncoder(fbW, fbH, Math.round(100 / this.targetFps));
    }
    if (this.gifEncoder) {
      this.gifEncoder.addFrame(pixels);
    }
  }

  cleanup() {
    if (!this.isRunning) return;
    this.isRunning = false;

    for (const w of this.workers) {
      w.exit();
    }

    if (this.gifPath) {
      if (this.gifEncoder && this.gifEncoder.frames.length > 0) {
        try {
          const gifData = this.gifEncoder.save();
          ENV.writeFile(this.gifPath, gifData);
          console.log(`[GIF] Saved ${this.gifEncoder.frames.length} frames to ${this.gifPath}`);
        } catch (err) {
          console.error('Failed to save GIF:', err.message);
        }
      } else {
        console.log(`[GIF] No active framebuffer in loaded ROMs; skipping ${this.gifPath}`);
      }
    }
  }

  step() {
    if (!this.isRunning) return false;
    this.frameCount++;
    const now = Date.now();
    const deltaSec = (now - this.lastTime) / 1000.0;
    this.lastTime = now;

    const primary = this.workers[0];
    let anyRunning = false;

    for (const w of this.workers) {
      if (!w.running) continue;

      // Update clock extension
      if (w.clockPtr && w.clockPtr + 24 <= w.memory.buffer.byteLength) {
        const view = new DataView(w.memory.buffer, w.clockPtr, 24);
        view.setBigUint64(0, BigInt(now - this.startTime), true);
        view.setFloat32(16, deltaSec, true);
      }

      // Update primary input extensions
      if (w === primary) {
        if (w.keyboardPtr && w.keyboardPtr + 256 <= w.memory.buffer.byteLength) {
          new Uint8Array(w.memory.buffer, w.keyboardPtr, 256).set(this.keyState);
        }
        if (w.mousePtr && w.mousePtr + 20 <= w.memory.buffer.byteLength) {
          const view = new DataView(w.memory.buffer, w.mousePtr, 20);
          view.setInt32(0, this.mouseX, true);
          view.setInt32(4, this.mouseY, true);
          view.setUint32(8, this.mouseButtons, true);
        }
        if (w.gamepadPtr && w.gamepadPtr + 20 <= w.memory.buffer.byteLength) {
          const view = new DataView(w.memory.buffer, w.gamepadPtr, 20);
          view.setUint32(0, this.gamepadMask, true);
          new Int16Array(w.memory.buffer, w.gamepadPtr + 4, 8).set(this.gamepadAxes);
        }
      }

      // Flush logger extension
      if (w.loggerPtr && w.loggerPtr + 12 <= w.memory.buffer.byteLength) {
        const view = new DataView(w.memory.buffer, w.loggerPtr, 12);
        const len = view.getUint32(8, true);
        if (len > 0 && w.loggerBufPtr && w.loggerBufPtr + len <= w.memory.buffer.byteLength) {
          const textBytes = new Uint8Array(w.memory.buffer, w.loggerBufPtr, len);
          const str = new TextDecoder().decode(textBytes);
          console.log(`[${w.name} Log] ${str}`);
          view.setUint32(8, 0, true);
        }
      }

      const status = w.update();
      if (status === 1) { // WUPDATE_EXIT
        w.running = false;
      } else if (status < 0) { // WUPDATE_ERROR
        console.error(`[Worker ${w.name}] wupdate() returned error code ${status}`);
        this.cleanup();
        return false;
      } else {
        anyRunning = true;
      }
    }

    if (!anyRunning) {
      this.cleanup();
      return false;
    }

    this.captureFrame();

    if (this.maxFrames > 0 && this.frameCount >= this.maxFrames) {
      this.cleanup();
      return false;
    }

    return true;
  }

  async run() {
    this.isRunning = true;
    this.startTime = Date.now();
    this.lastTime = this.startTime;

    ENV.onSignal('SIGINT', () => { this.cleanup(); ENV.exit(0); });

    return new Promise((resolve) => {
      const loop = () => {
        const shouldContinue = this.step();
        if (!shouldContinue) {
          resolve(0);
          return;
        }

        if (this.maxFrames > 0) {
          if (typeof setImmediate !== 'undefined') setImmediate(loop);
          else setTimeout(loop, 0);
        } else {
          setTimeout(loop, 1000 / this.targetFps);
        }
      };

      loop();
    });
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { WagnosticHost };
}
