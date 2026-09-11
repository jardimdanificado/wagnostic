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
    this.frameCount = 0;

    // Extension state storage & active extension trackers
    this.extState = new Map();
    this.activeExtensions = [];
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
    return {
      env: {
        memory: new WebAssembly.Memory({ initial: 16 }),

        wextension: (namePtr) => {
          const extName = this.readString(namePtr);
          return this.host.extensions.dispatch(this, this.host, extName);
        },

        wask: (targetPtr, dataPtr, size, timeout) => {
          const target = this.readString(targetPtr);
          return this.host.ipc.ask(this, target, dataPtr, size, timeout, this.host.workerMap);
        },

        wtell: (targetPtr, dataPtr, size, timeout) => {
          const target = this.readString(targetPtr);
          return this.host.ipc.tell(this, target, dataPtr, size, timeout, this.host.workerMap);
        },

        wexit: (code) => {
          this.running = false;
          this.exitCode = code;
        }
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

    if (typeof this.instance.exports.winit === 'function') {
      try {
        const initRes = this.instance.exports.winit();
        if (initRes < 0) {
          console.error(`[Worker ${this.name}] winit() returned error code ${initRes}`);
          this.running = false;
          this.exitCode = initRes;
        }
      } catch (err) {
        console.error(`[Worker ${this.name}] winit() exception:`, err.message);
        this.running = false;
        this.exitCode = -1;
      }
    }
  }

  update() {
    if (!this.running || !this.instance) return 1;
    if (typeof this.instance.exports.wupdate !== 'function') return 1;

    try {
      this.host.extensions.onBeforeUpdate(this, this.host);
      const status = this.instance.exports.wupdate();
      this.frameCount++;
      this.host.extensions.onAfterUpdate(this, this.host);
      return status;
    } catch (err) {
      console.error(`[Worker ${this.name}] wupdate() exception:`, err.message);
      this.running = false;
      this.exitCode = -1;
      return -1;
    }
  }

  exit() {
    if (this.instance && typeof this.instance.exports.wexit === 'function') {
      try { this.instance.exports.wexit(); } catch (e) {}
    }
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { WWorker };
}
