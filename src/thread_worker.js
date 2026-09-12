/**
 * Piolho Threaded Worker (Host-Side Controller)
 * 
 * Spawns and manages a Web Worker / WorkerThread running a Piolho WASM module.
 */

const path = require('path');

class ThreadedWorker {
  constructor(id, name, filePath, host, options = {}) {
    this.id = id;
    this.name = name;
    this.filePath = filePath;
    this.host = host;
    this.isThreaded = true;
    this.running = false;
    this.exitCode = 0;
    this.stepCount = 0;
    this.options = options;

    this.worker = null;
    this.extState = new Map();
  }

  async instantiate(wasmBytes) {
    return new Promise((resolve, reject) => {
      const isNode = typeof process !== 'undefined' && process.versions && process.versions.node;
      const runnerPath = path.join(__dirname, 'worker_thread_runner.js');

      if (isNode) {
        const { Worker } = require('worker_threads');
        this.worker = new Worker(runnerPath);
      } else if (typeof Worker !== 'undefined') {
        this.worker = new Worker(runnerPath);
      } else {
        return reject(new Error('Worker threads / WebWorkers are not supported in this runtime.'));
      }

      this.worker.on('message', (msg) => {
        if (!msg) return;
        if (msg.type === 'ready') {
          this.running = true;
          resolve(this);
        } else if (msg.type === 'exit') {
          this.running = false;
          this.exitCode = msg.code || 0;
        } else if (msg.type === 'error') {
          this.running = false;
          console.error(`[ThreadedWorker ${this.name}] Error:`, msg.error || msg.status);
        } else if (msg.type === 'ipc_tell') {
          // Coordinate rendezvous through host IPC engine
          this.host.ipc.tell(
            { name: msg.sender, memory: null, isThreaded: true },
            msg.target,
            msg.payload,
            msg.size,
            msg.timeout,
            this.host.workerMap
          );
        } else if (msg.type === 'ipc_hear') {
          this.host.ipc.hear(
            { name: msg.sender, memory: null, isThreaded: true, threadWorker: this },
            msg.target,
            null,
            msg.size,
            msg.timeout,
            this.host.workerMap
          );
        }
      });

      this.worker.on('error', (err) => {
        console.error(`[ThreadedWorker ${this.name}] Thread Exception:`, err);
        this.running = false;
        reject(err);
      });

      this.worker.on('exit', (code) => {
        this.running = false;
        this.exitCode = code;
      });

      // Send initialization message to worker thread
      this.worker.postMessage({
        type: 'init',
        id: this.id,
        name: this.name,
        wasmBytes,
        sharedBuffer: this.host.arena ? this.host.arena.buffer : null,
        intervalMs: this.host.intervalMs,
        autoRun: this.options.autoRun !== false
      });
    });
  }

  deliverTell(sender, payload) {
    if (this.worker && this.running) {
      this.worker.postMessage({
        type: 'deliver_tell',
        sender,
        payload
      });
    }
  }

  update() {
    if (!this.running || !this.worker) return 1;
    this.stepCount++;
    if (this.options.autoRun === false) {
      this.worker.postMessage({ type: 'step' });
    }
    return 0;
  }

  exit() {
    this.running = false;
    if (this.worker) {
      try { this.worker.postMessage({ type: 'stop' }); } catch (e) {}
      try { this.worker.terminate(); } catch (e) {}
      if (typeof this.worker.unref === 'function') this.worker.unref();
      this.worker = null;
    }
  }
}

module.exports = { ThreadedWorker };
