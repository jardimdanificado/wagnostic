/**
 * Piolho Host Runner
 * 
 * Minimalist WebAssembly Multi-Worker Host & Rendezvous IPC Coordinator.
 */

const { ENV } = require('./env');
const { extractFromTar } = require('./tar');
const { ExtensionRegistry } = require('./extensions/registry');
const { IpcEngine } = require('./ipc');
const { PeerRegistry } = require('./peer_registry');
const { WWorker } = require('./worker');

class Piolho {
  constructor(options = {}) {
    this.intervalMs = options.intervalMs || (options.tickRate ? 1000 / options.tickRate : (options.fps ? 1000 / options.fps : 1000 / 30));
    this.extensions = options.extensions || new ExtensionRegistry();

    if (options.extDirs) {
      const dirs = Array.isArray(options.extDirs) ? options.extDirs : [options.extDirs];
      for (const d of dirs) this.extensions.addSearchPath(d);
    }

    this.workers = [];
    this.workerMap = new Map();
    this.peers = new PeerRegistry(this);
    this.ipc = new IpcEngine();
    this.isRunning = false;
    this.startTime = 0;
    this.lastTime = 0;
    this.stepCount = 0;
  }

  get frameCount() {
    return this.stepCount;
  }
  set frameCount(val) {
    this.stepCount = val;
  }

  addExtDir(dirPath) {
    this.extensions.addSearchPath(dirPath);
    return this;
  }

  use(ext) {
    if (typeof ext === 'string') {
      const resolved = this.extensions.resolve(ext);
      if (!resolved) {
        throw new Error(`Extension '${ext}' not found in registered extensions or search paths`);
      }
    } else if (ext instanceof ExtensionRegistry) {
      for (const e of ext.activeList) {
        this.extensions.register(e);
      }
    } else if (ext) {
      this.extensions.register(ext);
    }
    return this;
  }

  async loadRom(filePath, customName) {
    let name = customName;
    if (!name) {
      if (filePath.includes(':')) {
        const parts = filePath.split(':');
        filePath = parts[0];
        name = parts[1];
      } else {
        name = filePath.split('/').pop().replace(/\.wasm$|\.tar$/, '');
      }
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
    this.peers.registerLocal(worker);

    await worker.instantiate(wasmBytes);
    return worker;
  }

  cleanup() {
    if (!this.isRunning) return;
    this.isRunning = false;

    for (const w of this.workers) {
      w.exit();
    }

    this.peers.clear();
    this.extensions.onDestroy(this);
  }

  step() {
    if (!this.isRunning) return false;
    this.stepCount++;
    const now = Date.now();
    this.lastTime = now;

    let anyRunning = false;

    for (const w of this.workers) {
      if (!w.running) continue;

      const status = w.update();
      if (status === 1) { // UPDATE_EXIT
        w.running = false;
      } else if (status < 0) { // UPDATE_ERROR
        console.error(`[Worker ${w.name}] update() returned error code ${status}`);
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

    this.extensions.onPostStep(this);
    return true;
  }

  async run(options = {}) {
    const maxSteps = typeof options === 'number'
      ? options
      : (options.steps || options.ticks || options.maxSteps || options.frames || options.maxFrames || 0);
    this.isRunning = true;
    this.startTime = Date.now();
    this.lastTime = this.startTime;

    ENV.onSignal('SIGINT', () => { this.cleanup(); ENV.exit(0); });

    return new Promise((resolve) => {
      const loop = () => {
        const shouldContinue = this.step();
        if (!shouldContinue || (maxSteps > 0 && this.stepCount >= maxSteps)) {
          this.cleanup();
          resolve(0);
          return;
        }

        if (maxSteps > 0) {
          if (typeof setImmediate !== 'undefined') setImmediate(loop);
          else setTimeout(loop, 0);
        } else {
          setTimeout(loop, this.intervalMs);
        }
      };

      loop();
    });
  }
}

module.exports = { Piolho };
