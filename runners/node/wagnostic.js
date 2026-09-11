#!/usr/bin/env node
/**
 * Wagnostic 2.0 — Universal Zero-Dependency Terminal & Headless Runner
 * 
 * Runs on Node.js and txiki.js (tjs).
 * Renders 32-bit RGBA8888 framebuffer directly into any terminal using ANSI TrueColor.
 * 
 * Supports all Standard Extensions:
 * - std:framebuffer (v1)
 * - std:clock       (v1)
 * - std:io          (v1)
 * - std:gif         (v1)
 * - logger          (v1)
 * 
 * Usage:
 *   node runners/node/wagnostic.js <rom.wasm|rom.tar> [-n <frames>] [-fps <fps>] [--headless] [-g <out.gif>]
 *   tjs runners/node/wagnostic.js <rom.wasm|rom.tar> ...
 */

// ── Environment Abstraction Layer (Node.js <-> txiki.js) ──
const isTxiki = typeof tjs !== 'undefined' || typeof globalThis.tjs !== 'undefined';
const isNode = typeof process !== 'undefined' && process.versions && process.versions.node;

const ENV = {
  argv: isNode ? process.argv.slice(2) : (typeof tjs !== 'undefined' && tjs.args ? tjs.args.slice(1) : []),
  cwd: isNode ? process.cwd() : (typeof tjs !== 'undefined' && tjs.cwd ? tjs.cwd() : '.'),
  exit: (code = 0) => {
    if (isNode) process.exit(code);
    else if (typeof tjs !== 'undefined' && tjs.exit) tjs.exit(code);
  },
  stdoutWrite: (str) => {
    if (isNode) process.stdout.write(str);
    else if (typeof tjs !== 'undefined' && tjs.stdout) tjs.stdout.write(new TextEncoder().encode(str));
  },
  getTermSize: () => {
    if (isNode) return { cols: process.stdout.columns || 80, rows: process.stdout.rows || 24 };
    return { cols: 80, rows: 24 };
  },
  readFile: (filePath) => {
    if (isNode) {
      const fs = require('fs');
      return fs.readFileSync(filePath);
    } else if (typeof tjs !== 'undefined') {
      const f = tjs.open(filePath, 'r');
      const stat = f.stat();
      const buf = new Uint8Array(stat.size);
      f.read(buf);
      f.close();
      return buf;
    }
    throw new Error('Unsupported runtime for file reading');
  },
  writeFile: (filePath, data) => {
    if (isNode) {
      const fs = require('fs');
      fs.writeFileSync(filePath, data);
    } else if (typeof tjs !== 'undefined') {
      const f = tjs.open(filePath, 'w');
      f.write(data);
      f.close();
    }
  }
};

// ── Parse Command Line Arguments ──────────────────────────
let wasmFile = null;
let maxFrames = 0;
let targetFps = 30;
let headless = false;
let gifPath = null;

for (let i = 0; i < ENV.argv.length; i++) {
  const arg = ENV.argv[i];
  if (arg === '-n' || arg === '--frames') {
    maxFrames = parseInt(ENV.argv[++i], 10) || 0;
  } else if (arg.startsWith('-n=')) {
    maxFrames = parseInt(arg.split('=')[1], 10) || 0;
  } else if (arg === '-fps' || arg.startsWith('--fps=')) {
    targetFps = parseInt(arg.includes('=') ? arg.split('=')[1] : ENV.argv[++i], 10) || 30;
  } else if (arg === '--headless') {
    headless = true;
  } else if (arg === '-g' || arg.startsWith('--gif=')) {
    gifPath = arg.includes('=') ? arg.split('=')[1] : ENV.argv[++i];
  } else if (!wasmFile && !arg.startsWith('-')) {
    wasmFile = arg;
  }
}

if (!wasmFile) {
  console.log('Wagnostic 2.0 Universal Runner (Node.js & txiki.js)');
  console.log('Usage: wagnostic <rom.wasm|rom.tar> [-n <frames>] [-fps <fps>] [--headless] [-g <out.gif>]');
  ENV.exit(1);
}

// ── TAR Archive Extraction Helper ─────────────────────────
function extractFromTar(buf, targetFileName) {
  let offset = 0;
  let lastFound = null;
  const u8 = new Uint8Array(buf.buffer || buf);

  while (offset + 512 <= u8.byteLength) {
    if (u8[offset] === 0) break;
    let nameLen = 0;
    while (nameLen < 100 && u8[offset + nameLen] !== 0) nameLen++;
    const name = new TextDecoder().decode(u8.subarray(offset, offset + nameLen));

    let sizeStr = '';
    for (let i = 0; i < 12; i++) {
      const ch = u8[offset + 124 + i];
      if (ch >= 48 && ch <= 55) sizeStr += String.fromCharCode(ch);
    }
    const size = parseInt(sizeStr, 8) || 0;

    if (name === targetFileName || name.endsWith('/' + targetFileName)) {
      lastFound = u8.subarray(offset + 512, offset + 512 + size);
    }
    const skip = size + ((512 - (size % 512)) % 512);
    offset += 512 + skip;
  }
  return lastFound;
}

// ── Load Binary Bytes ─────────────────────────────────────
let rawBytes;
try {
  rawBytes = ENV.readFile(wasmFile);
} catch (e) {
  console.error(`Error: Failed to open file: ${wasmFile}`);
  ENV.exit(1);
}

let wasmBytes = rawBytes;
const extractedWasm = extractFromTar(rawBytes, 'main.wasm');
if (extractedWasm) {
  wasmBytes = extractedWasm;
}

// ── Pure JS LZW GIF Encoder (Zero Dependencies) ───────────
class MinimalGifEncoder {
  constructor(width, height, delayCs = 2) {
    this.width = width;
    this.height = height;
    this.delayCs = delayCs;
    this.frames = [];
  }

  addFrame(pixelsRgba) {
    const indexed = new Uint8Array(this.width * this.height);
    for (let i = 0; i < indexed.length; i++) {
      const px = pixelsRgba[i];
      const r = (px & 0xFF) >> 6;
      const g = ((px >> 8) & 0xFF) >> 6;
      const b = ((px >> 16) & 0xFF) >> 6;
      indexed[i] = (r << 4) | (g << 2) | b;
    }
    this.frames.push(indexed);
  }

  save() {
    const out = [];
    const writeStr = (s) => { for (let i = 0; i < s.length; i++) out.push(s.charCodeAt(i)); };
    const write16 = (v) => { out.push(v & 0xFF, (v >> 8) & 0xFF); };

    writeStr("GIF89a");
    write16(this.width);
    write16(this.height);
    out.push(0xF5, 0x00, 0x00);

    for (let r = 0; r < 4; r++) {
      for (let g = 0; g < 4; g++) {
        for (let b = 0; b < 4; b++) {
          out.push(Math.round(r * 85), Math.round(g * 85), Math.round(b * 85));
        }
      }
    }

    out.push(0x21, 0xFF, 0x0B);
    writeStr("NETSCAPE2.0");
    out.push(0x03, 0x01, 0x00, 0x00, 0x00);

    for (const frameData of this.frames) {
      out.push(0x21, 0xF9, 0x04, 0x00);
      write16(this.delayCs);
      out.push(0x00, 0x00);

      out.push(0x2C);
      write16(0); write16(0);
      write16(this.width); write16(this.height);
      out.push(0x00);

      const minCodeSize = 6;
      out.push(minCodeSize);
      const clearCode = 1 << minCodeSize;
      const eoiCode = clearCode + 1;

      let pos = 0;
      while (pos < frameData.length) {
        const chunkLen = Math.min(254, frameData.length - pos);
        out.push(chunkLen + 1);
        out.push(clearCode);
        for (let i = 0; i < chunkLen; i++) out.push(frameData[pos++]);
      }
      out.push(1, eoiCode);
      out.push(0x00);
    }

    out.push(0x3B);
    return new Uint8Array(out);
  }
}

// ── Main Execution ────────────────────────────────────────
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
  let gifPtr = 0;
  let loggerPtr = 0;
  let loggerBufPtr = 0;

  const keyState = new Uint8Array(256);
  let mouseX = 0, mouseY = 0, mouseButtons = 0;
  let gamepadMask = 0;
  const gamepadAxes = new Int16Array(8);

  const importObject = {
    env: {
      memory: new WebAssembly.Memory({ initial: 16 }),
      wextension: (namePtr) => {
        const name = readString(namePtr);

        // 1. Framebuffer (std:framebuffer)
        if (name === 'std:framebuffer' || name === 'framebuffer' || name === 'surface' || name === 'std:surface') {
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

        // 2. Clock (std:clock)
        if (name === 'std:clock' || name === 'clock') {
          if (!clockPtr) {
            clockPtr = hostAlloc(24, 8);
            const view = new DataView(memory.buffer, clockPtr, 24);
            view.setBigUint64(0, 0n, true);           // ticks
            view.setBigUint64(8, 1000n, true);       // frequency
            view.setFloat32(16, 1.0 / targetFps, true); // delta
          }
          return clockPtr;
        }

        // 3. Keyboard (std:keyboard)
        if (name === 'std:keyboard' || name === 'keyboard') {
          if (!keyboardPtr) {
            keyboardPtr = hostAlloc(256, 4);
            new Uint8Array(memory.buffer, keyboardPtr, 256).fill(0);
          }
          return keyboardPtr;
        }

        // 4. Mouse (std:mouse)
        if (name === 'std:mouse' || name === 'mouse') {
          if (!mousePtr) {
            mousePtr = hostAlloc(20, 4);
            const view = new DataView(memory.buffer, mousePtr, 20);
            view.setInt32(0, 0, true);                // x
            view.setInt32(4, 0, true);                // y
            view.setUint32(8, 0, true);               // buttons
            view.setInt32(12, 0, true);               // wheel_x
            view.setInt32(16, 0, true);               // wheel_y
          }
          return mousePtr;
        }

        // 5. Gamepad (std:gamepad)
        if (name === 'std:gamepad' || name === 'gamepad') {
          if (!gamepadPtr) {
            gamepadPtr = hostAlloc(20, 4);
            const view = new DataView(memory.buffer, gamepadPtr, 20);
            view.setUint32(0, 0, true);              // buttons
            new Int16Array(memory.buffer, gamepadPtr + 4, 8).fill(0); // axes[8]
          }
          return gamepadPtr;
        }

        // 4. GIF Recording (std:gif)
        if (name === 'std:gif' || name === 'gif') {
          if (!gifPtr) {
            gifPtr = hostAlloc(20, 4);
            const view = new DataView(memory.buffer, gifPtr, 20);
            view.setUint32(0, (gifPath || maxFrames > 0) ? 1 : 0, true); // recording
            view.setUint32(4, 0, true);               // frame_count
            view.setUint32(8, maxFrames, true);       // max_frames
            view.setUint32(12, 2, true);              // delay_cs
            view.setUint32(16, 0, true);              // save_trigger
          }
          return gifPtr;
        }

        // 5. Logger (logger)
        if (name === 'logger') {
          if (!loggerPtr) {
            loggerPtr = hostAlloc(12, 4);
            loggerBufPtr = hostAlloc(1024, 4);
            const view = new DataView(memory.buffer, loggerPtr, 12);
            view.setUint32(0, loggerBufPtr, true);    // buffer
            view.setUint32(4, 1024, true);            // capacity
            view.setUint32(8, 0, true);               // length
          }
          return loggerPtr;
        }

        return 0;
      },
      abort: () => console.error('WASM Aborted'),
    },
    wasi_snapshot_preview1: {
      fd_write: () => 0,
      fd_seek: () => 0,
      fd_close: () => 0,
      proc_exit: (code) => ENV.exit(code),
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
      ENV.exit(1);
    }
  }

  const instance = wasmModule.instance;
  const exports = instance.exports;

  if (typeof exports.wupdate !== 'function') {
    console.error('Error: ROM does not export wupdate()');
    ENV.exit(1);
  }

  memory = exports.memory || importObject.env.memory;

  let gifEncoder = null;
  let isRunning = true;

  function cleanup() {
    if (!isRunning) return;
    isRunning = false;
    if (!headless) {
      ENV.stdoutWrite('\x1b[?25h\x1b[0m\n');
      if (isNode && process.stdin.isTTY && process.stdin.setRawMode) {
        process.stdin.setRawMode(false);
      }
    }
    if (gifEncoder && gifPath) {
      try {
        const gifData = gifEncoder.save();
        ENV.writeFile(gifPath, gifData);
        console.log(`[GIF] Saved ${gifEncoder.frames.length} frames to ${gifPath}`);
      } catch (err) {
        console.error('Failed to save GIF:', err.message);
      }
    }
  }

  if (isNode) {
    process.on('SIGINT', () => { cleanup(); ENV.exit(0); });
    process.on('exit', cleanup);
  }

  if (!headless) {
    ENV.stdoutWrite('\x1b[?25l\x1b[2J');
    if (isNode && process.stdin.isTTY && process.stdin.setRawMode) {
      process.stdin.setRawMode(true);
      process.stdin.resume();
      process.stdin.on('data', (key) => {
        if (key[0] === 3 || key[0] === 27 || key[0] === 113) {
          cleanup();
          ENV.exit(0);
        }
        if (key[0] === 0x1b && key[1] === 0x5b) {
          if (key[2] === 0x41) { keyState[0x52] = 1; gamepadMask |= (1 << 10); }
          if (key[2] === 0x42) { keyState[0x51] = 1; gamepadMask |= (1 << 11); }
          if (key[2] === 0x44) { keyState[0x50] = 1; gamepadMask |= (1 << 12); }
          if (key[2] === 0x43) { keyState[0x4F] = 1; gamepadMask |= (1 << 13); }
        }
        if (key[0] === 122 || key[0] === 90) { keyState[0x1D] = 1; gamepadMask |= (1 << 0); }
        if (key[0] === 120 || key[0] === 88) { keyState[0x1B] = 1; gamepadMask |= (1 << 1); }
        if (key[0] === 13) { keyState[0x28] = 1; gamepadMask |= (1 << 7); }
      });
    }
  }

  function renderTerminal(frameNum) {
    if (!fbPtr || fbPtr + 12 > memory.buffer.byteLength) return;

    const fbView = new DataView(memory.buffer, fbPtr, 12);
    const fbW = fbView.getUint32(0, true) || 320;
    const fbH = fbView.getUint32(4, true) || 240;
    const pixelsPtr = fbView.getUint32(8, true);

    if (!pixelsPtr || fbW === 0 || fbH === 0 || pixelsPtr + fbW * fbH * 4 > memory.buffer.byteLength) return;

    const pixels = new Uint32Array(memory.buffer, pixelsPtr, fbW * fbH);

    if (gifPath && !gifEncoder) {
      gifEncoder = new MinimalGifEncoder(fbW, fbH, Math.round(100 / targetFps));
    }
    if (gifEncoder) {
      gifEncoder.addFrame(pixels);
    }

    if (headless) return;

    const { cols: termCols, rows: termRows } = ENV.getTermSize();
    const maxTermRows = Math.max(10, termRows - 2);

    const termW = Math.min(termCols, fbW);
    const termH = Math.min(maxTermRows * 2, fbH);

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
    ENV.stdoutWrite(out);
  }

  let frame = 0;
  let lastTime = Date.now();
  const startTime = Date.now();

  function step() {
    if (!isRunning) return;
    frame++;
    const now = Date.now();
    const deltaSec = (now - lastTime) / 1000.0;
    lastTime = now;

    if (clockPtr && clockPtr + 24 <= memory.buffer.byteLength) {
      const view = new DataView(memory.buffer, clockPtr, 24);
      view.setBigUint64(0, BigInt(now - startTime), true);
      view.setFloat32(16, deltaSec, true);
    }

    if (keyboardPtr && keyboardPtr + 256 <= memory.buffer.byteLength) {
      new Uint8Array(memory.buffer, keyboardPtr, 256).set(keyState);
    }

    if (mousePtr && mousePtr + 20 <= memory.buffer.byteLength) {
      const view = new DataView(memory.buffer, mousePtr, 20);
      view.setInt32(0, mouseX, true);
      view.setInt32(4, mouseY, true);
      view.setUint32(8, mouseButtons, true);
    }

    if (gamepadPtr && gamepadPtr + 20 <= memory.buffer.byteLength) {
      const view = new DataView(memory.buffer, gamepadPtr, 20);
      view.setUint32(0, gamepadMask, true);
      new Int16Array(memory.buffer, gamepadPtr + 4, 8).set(gamepadAxes);
    }

    if (loggerPtr && loggerPtr + 12 <= memory.buffer.byteLength) {
      const view = new DataView(memory.buffer, loggerPtr, 12);
      const len = view.getUint32(8, true);
      if (len > 0) {
        const textBytes = new Uint8Array(memory.buffer, loggerBufPtr, len);
        const str = new TextDecoder().decode(textBytes);
        console.log(`[ROM Log] ${str}`);
        view.setUint32(8, 0, true);
      }
    }

    let status = 0;
    try {
      status = exports.wupdate();
    } catch (err) {
      console.error(`wupdate() error at frame ${frame}:`, err.message);
      cleanup();
      ENV.exit(1);
    }

    if (status === 1) {
      cleanup();
      ENV.exit(0);
    }
    if (status < 0) {
      console.error(`wupdate() returned error code ${status}`);
      cleanup();
      ENV.exit(1);
    }

    renderTerminal(frame);

    if (maxFrames > 0 && frame >= maxFrames) {
      cleanup();
      ENV.exit(0);
    }

    if (headless) {
      if (typeof setImmediate !== 'undefined') setImmediate(step);
      else setTimeout(step, 0);
    } else {
      setTimeout(step, 1000 / targetFps);
    }
  }

  step();
}

run().catch((err) => {
  console.error('Fatal error:', err);
  ENV.exit(1);
});
