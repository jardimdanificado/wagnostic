/**
 * Piolho Worker Instance
 * 
 * Encapsulates a WebAssembly instance, its linear memory arena, and core capability imports.
 */

class WWorker {
  constructor(id, name, filePath, host) {
    this.id = id;
    this.name = name;
    this.filePath = filePath;
    this.host = host;

    this.memory = null;
    this.instance = null;
    this.module = null;
    this.arenaOffset = 0;
    this.running = true;
    this.exitCode = 0;
    this.stepCount = 0;

    // Extension state storage & active extension trackers
    this.extState = new Map();
    this.activeExtensions = [];
  }

  get frameCount() {
    return this.stepCount;
  }
  set frameCount(v) {
    this.stepCount = v;
  }

  alloc(size, align = 4) {
    if (this.arenaOffset === 0) {
      const blen = this.memory ? this.memory.buffer.byteLength : 65536;
      if (blen >= 2097152) {
        this.arenaOffset = blen - 1400000;
      } else if (blen >= 1048576) {
        this.arenaOffset = blen - 400000;
      } else {
        this.arenaOffset = 0x8000;
      }
    }
    if (align > 1) {
      this.arenaOffset = (this.arenaOffset + align - 1) & ~(align - 1);
    }
    const ptr = this.arenaOffset;
    this.arenaOffset += size;
    return ptr;
  }

  readString(ptr) {
    if (!ptr || !this.memory) return '';
    const bytes = new Uint8Array(this.memory.buffer, ptr);
    let len = 0;
    while (len < 256 && bytes[len] !== 0) len++;
    return new TextDecoder().decode(bytes.subarray(0, len));
  }

  getImportObject() {
    const handleUse = (namePtr) => {
      const extName = this.readString(namePtr);
      return this.host.extensions.dispatch(this, this.host, extName);
    };

    const handleHear = (targetPtr, dataPtr, size, timeout) => {
      const target = this.readString(targetPtr);
      return this.host.ipc.ask(this, target, dataPtr, size, timeout, this.host.workerMap);
    };

    const handleTell = (targetPtr, dataPtr, size, timeout) => {
      const target = this.readString(targetPtr);
      return this.host.ipc.tell(this, target, dataPtr, size, timeout, this.host.workerMap);
    };

    const handleQuit = (code) => {
      this.running = false;
      this.exitCode = code;
    };

    return {
      env: {
        memory: new WebAssembly.Memory({ initial: 16 }),

        use: handleUse,
        hear: handleHear,
        tell: handleTell,
        quit: handleQuit,

        // Backwards compatibility aliases
        wextension: handleUse,
        wask: handleHear,
        wtell: handleTell,
        wexit: handleQuit
      },
      wasi_snapshot_preview1: {
        proc_exit: (code) => {
          this.running = false;
          this.exitCode = code;
        }
      }
    };
  }

  async instantiate(wasmBytes) {
    const importObj = this.getImportObject();
    const wasmModule = await WebAssembly.instantiate(wasmBytes, importObj);
    this.instance = wasmModule.instance;
    this.module = wasmModule.module;
    this.memory = this.instance.exports.memory || importObj.env.memory;
  }

  update() {
    if (!this.running || !this.instance) return 1;
    const updateFn = this.instance.exports.update || this.instance.exports.step || this.instance.exports.wupdate;
    if (typeof updateFn !== 'function') return 1;

    try {
      this.host.extensions.onBeforeUpdate(this, this.host);
      const status = updateFn();
      this.stepCount++;
      this.host.extensions.onAfterUpdate(this, this.host);
      return status;
    } catch (err) {
      console.error(`[Worker ${this.name}] update() exception:`, err.message);
      this.running = false;
      this.exitCode = -1;
      return -1;
    }
  }

  exit() {
    this.running = false;
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { WWorker, Worker: WWorker };
}
