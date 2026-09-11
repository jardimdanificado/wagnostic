#!/usr/bin/env node
/**
 * Wagnostic 2.0 — Zero-Dependency ANSI Terminal Runner
 *
 * Renders the 32-bit RGBA8888 framebuffer directly into any modern terminal
 * using 24-bit ANSI TrueColor and Unicode half-block characters (▀).
 *
 * Usage:
 *   node examples/terminal_runner.js <path/to/rom.wasm> [-n <frames>] [-fps <target_fps>]
 */

const fs = require('fs');
const path = require('path');
const process = require('process');

const args = process.argv.slice(2);
let wasmFile = null;
let maxFrames = 0;
let targetFps = 30;

for (let i = 0; i < args.length; i++) {
  if (args[i] === '-n' || args[i] === '--frames') {
    maxFrames = parseInt(args[++i], 10) || 0;
  } else if (args[i] === '-fps') {
    targetFps = parseInt(args[++i], 10) || 30;
  } else if (!wasmFile && !args[i].startsWith('-')) {
    wasmFile = args[i];
  }
}

if (!wasmFile) {
  console.log('Usage: node examples/terminal_runner.js <rom.wasm> [-n <frames>] [-fps <fps>]');
  process.exit(1);
}

const wasmBytes = fs.readFileSync(path.resolve(process.cwd(), wasmFile));

async function run() {
  let memory = null;
  let arenaOffset = 0;

  function hostAlloc(size, align = 4) {
    if (arenaOffset === 0) {
      arenaOffset = (memory.buffer.byteLength > 1048576) ? 0x20000 : 0x8000;
    }
    if (align > 1) {
      arenaOffset = (arenaOffset + align - 1) & ~(align - 1);
    }
    const ptr = arenaOffset;
    arenaOffset += size;
    return ptr;
  }

  function readString(ptr) {
    if (!ptr || !memory) return '';
    const bytes = new Uint8Array(memory.buffer, ptr);
    let len = 0;
    while (len < 256 && bytes[len] !== 0) len++;
    return new TextDecoder().decode(bytes.subarray(0, len));
  }

  let fbPtr = 0;
  let defaultFbPtr = 0;
  let clockPtr = 0;
  let keyboardPtr = 0;
  let mousePtr = 0;
  let gamepadPtr = 0;
  let loggerPtr = 0;
  let loggerBufPtr = 0;

  const importObject = {
    env: {
      memory: new WebAssembly.Memory({ initial: 16 }),
      wextension: (namePtr) => {
        const name = readString(namePtr);

        // 1. Framebuffer
        if (name === 'framebuffer' || name === 'surface' || name === 'std:framebuffer' || name === 'std:surface') {
          if (!fbPtr) {
            fbPtr = hostAlloc(12, 4);
            defaultFbPtr = hostAlloc(640 * 480 * 4, 4);
            const view = new DataView(memory.buffer, fbPtr, 12);
            view.setUint32(0, 320, true);             // width = 320
            view.setUint32(4, 240, true);             // height = 240
            view.setUint32(8, defaultFbPtr, true);    // pixels
          }
          return fbPtr;
        }

        // 2. Clock
        if (name === 'clock' || name === 'std:clock') {
          if (!clockPtr) {
            clockPtr = hostAlloc(24, 8);
            const view = new DataView(memory.buffer, clockPtr, 24);
            view.setBigUint64(0, 0n, true);
            view.setBigUint64(8, 1000n, true);
            view.setFloat32(16, 1.0 / targetFps, true);
          }
          return clockPtr;
        }

        // 3. Keyboard
        if (name === 'keyboard' || name === 'std:keyboard') {
          if (!keyboardPtr) {
            keyboardPtr = hostAlloc(256, 4);
          }
          return keyboardPtr;
        }

        // 4. Mouse
        if (name === 'mouse' || name === 'std:mouse') {
          if (!mousePtr) {
            mousePtr = hostAlloc(20, 4);
          }
          return mousePtr;
        }

        // 5. Gamepad
        if (name === 'gamepad' || name === 'std:gamepad') {
          if (!gamepadPtr) {
            gamepadPtr = hostAlloc(20, 4);
          }
          return gamepadPtr;
        }

        // 6. Logger
        if (name === 'logger' || name === 'std:logger') {
          if (!loggerPtr) {
            loggerPtr = hostAlloc(20, 4);
            loggerBufPtr = hostAlloc(1024, 4);
            const view = new DataView(memory.buffer, loggerPtr, 20);
            view.setUint32(0, 1, true);
            view.setUint32(4, 20, true);
            view.setUint32(8, loggerBufPtr, true);
            view.setUint32(12, 1024, true);
            view.setUint32(16, 0, true);
          }
          return loggerPtr;
        }

        return 0;
      },
      wask: () => -2,
      wtell: () => -2,
      wexit: (code) => {
        cleanup();
        console.log(`\n[Host] ROM called wexit(${code})`);
        process.exit(code);
      },
      abort: () => console.error('WASM Aborted'),
    },
    wasi_snapshot_preview1: {
      fd_write: () => 0,
      fd_seek: () => 0,
      fd_close: () => 0,
      proc_exit: (code) => {
        cleanup();
        process.exit(code);
      },
    }
  };

  let wasmModule;
  try {
    wasmModule = await WebAssembly.instantiate(wasmBytes, importObject);
  } catch (err) {
    try {
      wasmModule = await WebAssembly.instantiate(wasmBytes, {});
    } catch (e2) {
      console.error('Failed to instantiate WebAssembly module:', err.message);
      process.exit(1);
    }
  }

  const instance = wasmModule.instance;
  const exports = instance.exports;

  if (typeof exports.winit === 'function') {
    exports.winit();
  }

  if (typeof exports.wupdate !== 'function') {
    console.error('Error: ROM does not export wupdate()');
    process.exit(1);
  }

  memory = exports.memory || importObject.env.memory;

  // Terminal Setup
  function cleanup() {
    process.stdout.write('\x1b[?25h\x1b[0m\n');
    if (process.stdin.isTTY && process.stdin.setRawMode) {
      process.stdin.setRawMode(false);
    }
  }

  process.on('SIGINT', () => { cleanup(); process.exit(0); });
  process.on('exit', cleanup);

  process.stdout.write('\x1b[?25l\x1b[2J');

  if (process.stdin.isTTY && process.stdin.setRawMode) {
    process.stdin.setRawMode(true);
    process.stdin.resume();
    process.stdin.on('data', (key) => {
      if (key[0] === 3 || key[0] === 27 || key[0] === 113) {
        cleanup();
        process.exit(0);
      }
    });
  }

  function renderTerminal(frameNum) {
    if (!fbPtr) return;

    const fbView = new DataView(memory.buffer, fbPtr, 12);
    const fbW = fbView.getUint32(0, true);
    const fbH = fbView.getUint32(4, true);
    const pixelsPtr = fbView.getUint32(8, true);

    if (!pixelsPtr || fbW === 0 || fbH === 0) return;

    const termCols = process.stdout.columns || 80;
    const termRows = (process.stdout.rows ? process.stdout.rows - 2 : 24);

    const termW = Math.min(termCols, fbW);
    const termH = Math.min(termRows * 2, fbH);

    const pixels = new Uint32Array(memory.buffer, pixelsPtr, fbW * fbH);

    let out = '\x1b[H';

    for (let ty = 0; ty < termH; ty += 2) {
      for (let tx = 0; tx < termW; tx++) {
        const srcX = Math.floor((tx * fbW) / termW);
        const srcY1 = Math.floor((ty * fbH) / termH);
        const srcY2 = Math.min(fbH - 1, Math.floor(((ty + 1) * fbH) / termH));

        const pxTop = pixels[srcY1 * fbW + srcX];
        const pxBot = (ty + 1 < termH) ? pixels[srcY2 * fbW + srcX] : pxTop;

        const r1 = pxTop & 0xFF, g1 = (pxTop >> 8) & 0xFF, b1 = (pxTop >> 16) & 0xFF;
        const r2 = pxBot & 0xFF, g2 = (pxBot >> 8) & 0xFF, b2 = (pxBot >> 16) & 0xFF;

        out += `\x1b[38;2;${r1};${g1};${b1}m\x1b[48;2;${r2};${g2};${b2}m▀`;
      }
      out += '\x1b[0m\n';
    }

    out += `\x1b[0m\x1b[90m [Wagnostic] Frame ${frameNum} | ${fbW}x${fbH} -> ${termW}x${termH} (Press 'q' or ESC to exit)\x1b[0m`;
    process.stdout.write(out);
  }

  let frame = 0;
  let lastTime = Date.now();
  const startTime = Date.now();

  function step() {
    frame++;
    const now = Date.now();
    const deltaSec = (now - lastTime) / 1000.0;
    lastTime = now;

    if (clockPtr) {
      const view = new DataView(memory.buffer, clockPtr, 32);
      view.setBigUint64(8, BigInt(now - startTime), true);
      view.setFloat32(24, deltaSec, true);
    }

    let status = 0;
    try {
      status = exports.wupdate();
    } catch (err) {
      cleanup();
      console.error(`\nwupdate() error at frame ${frame}:`, err.message);
      process.exit(1);
    }

    if (loggerPtr) {
      const lView = new DataView(memory.buffer, loggerPtr, 20);
      const len = lView.getUint32(16, true);
      if (len > 0) {
        const msgBytes = new Uint8Array(memory.buffer, loggerBufPtr, len);
        const msg = new TextDecoder().decode(msgBytes);
        process.stdout.write(`\n\x1b[36m[Guest Log]\x1b[0m ${msg}\n`);
        lView.setUint32(16, 0, true);
      }
    }

    renderTerminal(frame);

    if (status === 1) {
      cleanup();
      console.log(`\n[Host] ROM requested clean exit at frame ${frame}.`);
      process.exit(0);
    } else if (status < 0) {
      cleanup();
      console.error(`\n[Host] Error: wupdate() returned code ${status}`);
      process.exit(status);
    }

    if (maxFrames > 0 && frame >= maxFrames) {
      cleanup();
      console.log(`\n[Host] Completed ${maxFrames} frames.`);
      process.exit(0);
    }
  }

  const intervalMs = Math.max(1, Math.floor(1000 / targetFps));
  setInterval(step, intervalMs);
}

run().catch((err) => {
  console.error('Fatal error:', err);
  process.exit(1);
});
