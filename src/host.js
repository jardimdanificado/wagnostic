/**
 * Piolho Host Runner
 * 
 * Coordinates workers, frame loops, and extension lifecycle hooks.
 */

const { ENV } = require('./env');
const { extractFromTar } = require('./tar');
const { defaultRegistry } = require('./extensions');
const { IpcEngine } = require('./ipc');
const { WWorker } = require('./worker');

class PiolhoHost {
  constructor(options = {}) {
    this.maxFrames = (options.maxFrames !== undefined) ? options.maxFrames : 1;
    this.targetFps = options.targetFps || 30;
    this.gifPath = options.gifPath || null;
    this.extensions = options.extensions || defaultRegistry;

    this.workers = [];
    this.workerMap = new Map();
    this.ipc = new IpcEngine();
    this.isRunning = false;
    this.startTime = 0;
    this.lastTime = 0;
    this.frameCount = 0;

    // Generic host input state for input extensions
    this.keyState = new Uint8Array(256);
    this.mouseX = 0;
    this.mouseY = 0;
    this.mouseButtons = 0;
    this.mouseWheelX = 0;
    this.mouseWheelY = 0;
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

  cleanup() {
    if (!this.isRunning) return;
    this.isRunning = false;

    for (const w of this.workers) {
      w.exit();
    }

    this.extensions.onDestroy(this);
  }

  step() {
    if (!this.isRunning) return false;
    this.frameCount++;
    const now = Date.now();
    this.lastTime = now;

    let anyRunning = false;

    for (const w of this.workers) {
      if (!w.running) continue;

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

    this.extensions.onFrameComplete(this);

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
  module.exports = { PiolhoHost };
}
