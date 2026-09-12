/**
 * Wagnostic 2.0 — Bare Minimal Embeddable WebAssembly Runner (~60 lines)
 * 
 * Single-file, zero-dependency ES6 module.
 * Only handles WASM loading, memory arena allocation, and wupdate() loop.
 * Passes all `wextension(name)` calls directly to `onExtension(name, host)`.
 */

export class Wagnostic {
  /**
   * @param {Function|Object} onExtensionOrOptions - Extension handler function or options object
   * @param {Object} [extraOptions] - Additional options when passing function as first arg
   */
  constructor(onExtensionOrOptions = {}, extraOptions = {}) {
    let options = {};
    if (typeof onExtensionOrOptions === 'function') {
      options = { onExtension: onExtensionOrOptions, ...extraOptions };
    } else if (onExtensionOrOptions && typeof onExtensionOrOptions === 'object') {
      options = onExtensionOrOptions;
    }

    this.onExtension = options.onExtension || (() => 0);
    this.onExit = options.onExit || null;
    this.memory = null;
    this.instance = null;
    this.exports = null;
    this.arenaOffset = 0;
  }

  /**
   * Allocates aligned memory chunk in guest WebAssembly linear memory.
   */
  hostAlloc(size, align = 4) {
    if (!this.memory) throw new Error('Memory not initialized');
    if (this.arenaOffset === 0) {
      if (this.exports && this.exports.__heap_base !== undefined) {
        this.arenaOffset = typeof this.exports.__heap_base === 'object' ? Number(this.exports.__heap_base.value) : Number(this.exports.__heap_base);
      } else {
        this.arenaOffset = (this.memory.buffer.byteLength > 1048576) ? 0x20000 : 0x8000;
      }
    }
    if (align > 1) this.arenaOffset = (this.arenaOffset + align - 1) & ~(align - 1);
    const ptr = this.arenaOffset;
    this.arenaOffset += size;
    if (this.arenaOffset > this.memory.buffer.byteLength) {
      this.memory.grow(Math.ceil((this.arenaOffset - this.memory.buffer.byteLength) / 65536));
    }
    return ptr;
  }

  /**
   * Reads null-terminated C-string from WASM memory.
   */
  readString(ptr) {
    if (!ptr || !this.memory) return '';
    const bytes = new Uint8Array(this.memory.buffer, ptr);
    let len = 0;
    while (len < 1024 && (ptr + len) < this.memory.buffer.byteLength && bytes[len] !== 0) len++;
    return new TextDecoder().decode(bytes.subarray(0, len));
  }

  /**
   * Instantiates the WebAssembly module.
   */
  async init(wasmInput, extraImports = {}) {
    let wasmBytes = wasmInput;
    if (typeof Response !== 'undefined' && wasmInput instanceof Response) {
      wasmBytes = await wasmInput.arrayBuffer();
    } else if (typeof wasmInput === 'string') {
      if (typeof process !== 'undefined' && process.versions && process.versions.node) {
        const fs = await import('fs');
        wasmBytes = fs.readFileSync(wasmInput);
      } else if (typeof fetch === 'function') {
        const res = await fetch(wasmInput);
        if (!res.ok) throw new Error(`Failed to load WASM from '${wasmInput}': HTTP ${res.status}`);
        wasmBytes = await res.arrayBuffer();
      }
    }

    const defaultMemory = new WebAssembly.Memory({ initial: 16 });
    const importObject = {
      env: {
        memory: defaultMemory,
        wextension: (namePtr) => {
          const name = this.readString(namePtr);
          return this.onExtension(name, this);
        },
        abort: () => console.error('[Wagnostic] Guest WebAssembly module aborted.'),
        ...(extraImports.env || {})
      },
      wasi_snapshot_preview1: {
        fd_write: () => 0,
        fd_seek: () => 0,
        fd_close: () => 0,
        proc_exit: (code) => { if (this.onExit) this.onExit(code); },
        ...(extraImports.wasi_snapshot_preview1 || {})
      },
      ...extraImports
    };

    const module = await WebAssembly.instantiate(wasmBytes, importObject);
    this.instance = module.instance || module;
    this.exports = this.instance.exports;

    if (typeof this.exports.wupdate !== 'function') {
      throw new Error("ROM module does not export 'wupdate()' function.");
    }

    this.memory = this.exports.memory || importObject.env.memory;
    this.arenaOffset = 0;
    return this;
  }

  /**
   * Executes a single frame (`wupdate()`).
   * Returns: 0 = OK, 1 = EXIT, < 0 = ERROR
   */
  step() {
    if (!this.exports || !this.exports.wupdate) throw new Error('Not initialized');
    return this.exports.wupdate();
  }
}
