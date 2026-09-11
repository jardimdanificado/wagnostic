#!/usr/bin/env node
/**
 * Wagnostic 2.0 — Bare JavaScript Runner (Minimal Reference Host)
 *
 * This is a minimal, zero-dependency WebAssembly host.
 * It demonstrates the core Wagnostic contract:
 *   - Host provides `env.wextension(namePtr, version)`
 *   - Guest exports `wupdate()`
 *
 * Usage:
 *   node bare_runner.js <path-to-rom.wasm> [max_frames]
 */

const fs = require('fs');
const path = require('path');

async function run() {
  const romPath = process.argv[2];
  const maxFrames = parseInt(process.argv[3], 10) || 60;

  if (!romPath) {
    console.log('Usage: node bare_runner.js <path-to-rom.wasm> [max_frames]');
    process.exit(1);
  }

  const wasmBytes = fs.readFileSync(path.resolve(process.cwd(), romPath));

  let memory = null;
  let arenaOffset = 0x8000;

  // Simple host allocator for shared extension structures
  function hostAlloc(size, align = 4) {
    if (align > 1) arenaOffset = (arenaOffset + align - 1) & ~(align - 1);
    const ptr = arenaOffset;
    arenaOffset += size;
    return ptr;
  }

  // Helper to read C-string from WASM memory
  function readString(ptr) {
    if (!ptr || !memory) return '';
    const bytes = new Uint8Array(memory.buffer, ptr);
    let len = 0;
    while (len < 256 && bytes[len] !== 0) len++;
    return new TextDecoder().decode(bytes.subarray(0, len));
  }

  // ── Extension Registry (Extend this freely!) ──────────────
  let loggerPtr = 0;
  let loggerBufPtr = 0;

  const customExtensions = {
    'logger': () => {
      if (!loggerPtr) {
        loggerPtr = hostAlloc(12, 4);
        loggerBufPtr = hostAlloc(1024, 4);
        const view = new DataView(memory.buffer, loggerPtr, 12);
        view.setUint32(0, loggerBufPtr, true); // buffer
        view.setUint32(4, 1024, true);        // capacity
        view.setUint32(8, 0, true);           // length
      }
      return loggerPtr;
    },
  };

  const importObject = {
    env: {
      memory: new WebAssembly.Memory({ initial: 16 }),
      wextension: (namePtr) => {
        const name = readString(namePtr);
        console.log(`[Host] ROM requested extension: "${name}"`);

        if (customExtensions[name]) {
          return customExtensions[name]();
        }

        // Return 0 (NULL) if extension is unknown/unsupported
        return 0;
      },
      wask: () => -2,  // WIPC_TIMEOUT
      wtell: () => -2, // WIPC_TIMEOUT
      wexit: (code) => {
        console.log(`[Host] ROM called wexit(${code})`);
        process.exit(code);
      },
    },
    wasi_snapshot_preview1: {
      fd_write: () => 0,
      fd_seek: () => 0,
      fd_close: () => 0,
      proc_exit: (code) => process.exit(code),
    },
  };

  const { instance } = await WebAssembly.instantiate(wasmBytes, importObject);
  memory = instance.exports.memory || importObject.env.memory;

  if (typeof instance.exports.winit === 'function') {
    instance.exports.winit();
  }

  if (typeof instance.exports.wupdate !== 'function') {
    console.error('Error: WASM module does not export "wupdate()"');
    process.exit(1);
  }

  console.log(`[Host] Starting execution loop (${maxFrames} frames)...`);

  let frame = 0;
  while (frame < maxFrames) {
    const status = instance.exports.wupdate();

    // Check if guest wrote anything to logger extension
    if (loggerPtr) {
      const view = new DataView(memory.buffer, loggerPtr, 12);
      const len = view.getUint32(8, true);
      if (len > 0) {
        const textBytes = new Uint8Array(memory.buffer, loggerBufPtr, len);
        const msg = new TextDecoder().decode(textBytes);
        console.log(`[Guest Log] ${msg}`);
        view.setUint32(8, 0, true); // Flush
      }
    }

    if (status === 1) { // WUPDATE_EXIT
      console.log(`[Host] ROM requested clean exit at frame ${frame}.`);
      break;
    }
    if (status < 0) { // WUPDATE_ERROR
      console.error(`[Host] ROM returned error code ${status} at frame ${frame}.`);
      process.exit(1);
    }

    frame++;
  }

  console.log(`[Host] Finished successfully after ${frame} frame(s).`);
}

run().catch((err) => {
  console.error('Fatal error:', err);
  process.exit(1);
});
