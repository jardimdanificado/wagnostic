#!/usr/bin/env node
/**
 * Wagnostic 2.0 — Universal Zero-Dependency Terminal & Headless Runner
 * 
 * Runs on Node.js and txiki.js (tjs).
 * Renders 32-bit RGBA8888 framebuffer directly into any terminal using ANSI TrueColor.
 * 
 * Supports:
 * - Multi-ROM Workers & Synchronous Rendezvous IPC (wask, wtell)
 * - Standard Extensions: std:framebuffer, std:clock, std:keyboard, std:mouse, std:gamepad, std:gif, logger
 * 
 * Usage:
 *   node runners/node/wagnostic.js <rom1.wasm[:name1]> [rom2.wasm[:name2] ...] [-n <frames>] [-fps <fps>] [--headless] [-g <out.gif>]
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
const romSpecs = [];
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
  } else if (!arg.startsWith('-')) {
    romSpecs.push(arg);
  }
}

if (romSpecs.length === 0) {
  console.log('Wagnostic 2.0 Universal Runner (Node.js & txiki.js)');
  console.log('Usage: wagnostic <rom1.wasm[:name1]> [rom2.wasm[:name2] ...] [-n <frames>] [-fps <fps>] [--headless] [-g <out.gif>]');
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
      const r = (px & 0xFF) >> 5;
      const g = ((px >> 8) & 0xFF) >> 5;
      const b = ((px >> 16) & 0xFF) >> 6;
      indexed[i] = (r << 5) | (g << 2) | b;
    }
    this.frames.push(indexed);
  }

  save() {
    const out = [];
    const pushStr = (str) => {
      for (let i = 0; i < str.length; i++) out.push(str.charCodeAt(i));
    };
    const push16 = (v) => { out.push(v & 0xFF, (v >> 8) & 0xFF); };

    pushStr('GIF89a');
    push16(this.width);
    push16(this.height);
    out.push(0xF7, 0, 0);

    for (let i = 0; i < 256; i++) {
      const r = (i >> 5) & 7;
      const g = (i >> 2) & 7;
      const b = i & 3;
      out.push(
        Math.round((r / 7) * 255),
        Math.round((g / 7) * 255),
        Math.round((b / 3) * 255)
      );
    }

    out.push(0x21, 0xFF, 0x0B);
    pushStr('NETSCAPE2.0');
    out.push(0x03, 0x01);
    push16(0);
    out.push(0x00);

    for (const frame of this.frames) {
      out.push(0x21, 0xF9, 0x04, 0x00);
      push16(this.delayCs);
      out.push(0x00, 0x00);

      out.push(0x2C);
      push16(0); push16(0);
      push16(this.width); push16(this.height);
      out.push(0x00);

      const minCodeSize = 8;
      out.push(minCodeSize);

      const clearCode = 1 << minCodeSize;
      const eoiCode = clearCode + 1;
      let nextCode = eoiCode + 1;
      let curCodeSize = minCodeSize + 1;

      let codeBuf = 0;
      let codeBits = 0;
      const packet = [];

      const writeBits = (val, bits) => {
        codeBuf |= (val << codeBits);
        codeBits += bits;
        while (codeBits >= 8) {
          packet.push(codeBuf & 0xFF);
          codeBuf >>= 8;
          codeBits -= 8;
          if (packet.length === 254) {
            out.push(packet.length, ...packet);
            packet.length = 0;
          }
        }
      };

      const codeTable = new Map();
      const resetTable = () => {
        codeTable.clear();
        for (let i = 0; i < clearCode; i++) codeTable.set(String.fromCharCode(i), i);
        nextCode = eoiCode + 1;
        curCodeSize = minCodeSize + 1;
      };

      resetTable();
      writeBits(clearCode, curCodeSize);

      let curStr = '';
      for (let i = 0; i < frame.length; i++) {
        const k = String.fromCharCode(frame[i]);
        const testStr = curStr + k;
        if (codeTable.has(testStr)) {
          curStr = testStr;
        } else {
          writeBits(codeTable.get(curStr), curCodeSize);
          if (nextCode < 4096) {
            codeTable.set(testStr, nextCode++);
            if (nextCode > (1 << curCodeSize) && curCodeSize < 12) {
              curCodeSize++;
            }
          } else {
            writeBits(clearCode, curCodeSize);
            resetTable();
          }
          curStr = k;
        }
      }

      if (curStr.length > 0) {
        writeBits(codeTable.get(curStr), curCodeSize);
      }
      writeBits(eoiCode, curCodeSize);

      if (codeBits > 0) {
        packet.push(codeBuf & 0xFF);
      }
      if (packet.length > 0) {
        out.push(packet.length, ...packet);
      }
      out.push(0x00);
    }

    out.push(0x3B);
    return new Uint8Array(out);
  }
}

// ── Main Multi-Worker Runner ──────────────────────────────
async function run() {
  const workers = [];
  const pendingIpcOps = []; // { sender, target, dataPtr, size, opType ('ASK'|'TELL') }

  const keyState = new Uint8Array(256);
  let mouseX = 0, mouseY = 0, mouseButtons = 0;
  let gamepadMask = 0;
  const gamepadAxes = new Int16Array(8);

  for (let i = 0; i < romSpecs.length; i++) {
    const spec = romSpecs[i];
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
      console.error(`Error: Failed to open file: ${filePath}`);
      ENV.exit(1);
    }

    let wasmBytes = rawBytes;
    const extractedWasm = extractFromTar(rawBytes, 'main.wasm');
    if (extractedWasm) wasmBytes = extractedWasm;

    const worker = {
      id: i + 1,
      name,
      filePath,
      memory: null,
      instance: null,
      arenaOffset: 0,
      running: true,
      exitCode: 0,
      fbPtr: 0,
      defaultFbPtr: 0,
      clockPtr: 0,
      keyboardPtr: 0,
      mousePtr: 0,
      gamepadPtr: 0,
      gifPtr: 0,
      loggerPtr: 0,
      loggerBufPtr: 0,
    };

    function hostAlloc(size, align = 4) {
      if (worker.arenaOffset === 0) {
        const blen = worker.memory.buffer.byteLength;
        if (blen >= 2097152) {
          worker.arenaOffset = blen - 1400000;
        } else if (blen >= 1048576) {
          worker.arenaOffset = blen - 400000;
        } else {
          worker.arenaOffset = 0x8000;
        }
      }
      if (align > 1) {
        worker.arenaOffset = (worker.arenaOffset + align - 1) & ~(align - 1);
      }
      const ptr = worker.arenaOffset;
      worker.arenaOffset += size;
      return ptr;
    }

    function readString(ptr) {
      if (!ptr || !worker.memory) return '';
      const bytes = new Uint8Array(worker.memory.buffer, ptr);
      let len = 0;
      while (len < 256 && bytes[len] !== 0) len++;
      return new TextDecoder().decode(bytes.subarray(0, len));
    }

    const importObject = {
      env: {
        memory: new WebAssembly.Memory({ initial: 16 }),
        wextension: (namePtr) => {
          const extName = readString(namePtr);

          if (extName === 'std:framebuffer' || extName === 'framebuffer' || extName === 'surface' || extName === 'std:surface') {
            if (!worker.fbPtr) {
              worker.fbPtr = hostAlloc(12, 4);
              worker.defaultFbPtr = hostAlloc(640 * 480 * 4, 4);
              const view = new DataView(worker.memory.buffer, worker.fbPtr, 12);
              view.setUint32(0, 320, true);
              view.setUint32(4, 240, true);
              view.setUint32(8, worker.defaultFbPtr, true);
            }
            return worker.fbPtr;
          }

          if (extName === 'std:clock' || extName === 'clock') {
            if (!worker.clockPtr) {
              worker.clockPtr = hostAlloc(24, 8);
              const view = new DataView(worker.memory.buffer, worker.clockPtr, 24);
              view.setBigUint64(0, 0n, true);
              view.setBigUint64(8, 1000n, true);
              view.setFloat32(16, 1.0 / targetFps, true);
            }
            return worker.clockPtr;
          }

          if (extName === 'std:keyboard' || extName === 'keyboard') {
            if (!worker.keyboardPtr) {
              worker.keyboardPtr = hostAlloc(256, 4);
              new Uint8Array(worker.memory.buffer, worker.keyboardPtr, 256).fill(0);
            }
            return worker.keyboardPtr;
          }

          if (extName === 'std:mouse' || extName === 'mouse') {
            if (!worker.mousePtr) {
              worker.mousePtr = hostAlloc(20, 4);
            }
            return worker.mousePtr;
          }

          if (extName === 'std:gamepad' || extName === 'gamepad') {
            if (!worker.gamepadPtr) {
              worker.gamepadPtr = hostAlloc(20, 4);
            }
            return worker.gamepadPtr;
          }

          if (extName === 'std:gif' || extName === 'gif') {
            if (!worker.gifPtr) {
              worker.gifPtr = hostAlloc(20, 4);
              const view = new DataView(worker.memory.buffer, worker.gifPtr, 20);
              view.setUint32(0, (gifPath || maxFrames > 0) ? 1 : 0, true);
              view.setUint32(4, 0, true);
              view.setUint32(8, maxFrames, true);
              view.setUint32(12, 2, true);
              view.setUint32(16, 0, true);
            }
            return worker.gifPtr;
          }

          if (extName === 'logger') {
            if (!worker.loggerPtr) {
              worker.loggerPtr = hostAlloc(12, 4);
              worker.loggerBufPtr = hostAlloc(1024, 4);
              const view = new DataView(worker.memory.buffer, worker.loggerPtr, 12);
              view.setUint32(0, worker.loggerBufPtr, true);
              view.setUint32(4, 1024, true);
              view.setUint32(8, 0, true);
            }
            return worker.loggerPtr;
          }

          return 0;
        },

        wask: (targetPtr, dataPtr, size, timeout) => {
          const target = readString(targetPtr);
          if (target === worker.name) return -3; // WIPC_PARAM

          const targetExists = romSpecs.some(s => (s.includes(':') ? s.split(':')[1] : s.split('/').pop().replace(/\.wasm$|\.tar$/, '')) === target);
          if (!targetExists) return -2; // WIPC_TARGET

          const matchIdx = pendingIpcOps.findIndex(
            op => op.opType === 'TELL' && op.caller === target && op.target === worker.name
          );

          if (matchIdx !== -1) {
            const match = pendingIpcOps.splice(matchIdx, 1)[0];
            if (match.size > size) return -4; // WIPC_SIZE
            if (match.size > 0) {
              const src = new Uint8Array(match.data);
              new Uint8Array(worker.memory.buffer, dataPtr, match.size).set(src);
            }
            return 1; // WIPC_OK
          }

          if (timeout === 0) return 0; // WIPC_TIMEOUT

          // Register waiter
          pendingIpcOps.push({
            worker,
            caller: worker.name,
            target,
            dataPtr,
            size,
            opType: 'ASK'
          });
          return 1; // WIPC_OK (cooperative)
        },

        wtell: (targetPtr, dataPtr, size, timeout) => {
          const target = readString(targetPtr);
          if (target === worker.name) return -3; // WIPC_PARAM

          const targetExists = romSpecs.some(s => (s.includes(':') ? s.split(':')[1] : s.split('/').pop().replace(/\.wasm$|\.tar$/, '')) === target);
          if (!targetExists) return -2; // WIPC_TARGET

          const matchIdx = pendingIpcOps.findIndex(
            op => op.opType === 'ASK' && op.caller === target && op.target === worker.name
          );

          if (matchIdx !== -1) {
            const match = pendingIpcOps.splice(matchIdx, 1)[0];
            if (size > match.size) return -4; // WIPC_SIZE
            if (size > 0) {
              const src = new Uint8Array(worker.memory.buffer, dataPtr, size);
              new Uint8Array(match.worker.memory.buffer, match.dataPtr, size).set(src);
            }
            return 1; // WIPC_OK
          }

          if (timeout === 0) return 0; // WIPC_TIMEOUT

          // Copy data immediately so local stack variable on caller side isn't clobbered
          const dataCopy = new Uint8Array(size);
          if (size > 0) {
            dataCopy.set(new Uint8Array(worker.memory.buffer, dataPtr, size));
          }

          // Register offer
          pendingIpcOps.push({
            worker,
            caller: worker.name,
            target,
            data: dataCopy,
            size,
            opType: 'TELL'
          });
          return 1; // WIPC_OK (cooperative)
        },

        abort: () => console.error(`Worker ${name} WASM Aborted`),
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
      console.error(`Failed to instantiate WASM module for ${name}:`, err.message);
      ENV.exit(1);
    }

    worker.instance = wasmModule.instance;
    worker.memory = worker.instance.exports.memory || importObject.env.memory;
    workers.push(worker);

    if (typeof worker.instance.exports.winit === 'function') {
      try { worker.instance.exports.winit(); } catch (e) {}
    }
  }

  const primary = workers[0];
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
    for (const w of workers) {
      if (w.instance && typeof w.instance.exports.wexit === 'function') {
        try { w.instance.exports.wexit(); } catch (e) {}
      }
    }
    if (gifPath) {
      if (gifEncoder && gifEncoder.frames.length > 0) {
        try {
          const gifData = gifEncoder.save();
          ENV.writeFile(gifPath, gifData);
          console.log(`[GIF] Saved ${gifEncoder.frames.length} frames to ${gifPath}`);
        } catch (err) {
          console.error('Failed to save GIF:', err.message);
        }
      } else {
        console.log(`[GIF] No active framebuffer in loaded ROMs; skipping ${gifPath}`);
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
    const fbWorker = workers.find(w => w.fbPtr && w.fbPtr + 12 <= w.memory.buffer.byteLength);
    if (!fbWorker) return;

    const fbView = new DataView(fbWorker.memory.buffer, fbWorker.fbPtr, 12);
    const fbW = fbView.getUint32(0, true) || 320;
    const fbH = fbView.getUint32(4, true) || 240;
    const pixelsPtr = fbView.getUint32(8, true);

    if (!pixelsPtr || fbW === 0 || fbH === 0 || pixelsPtr + fbW * fbH * 4 > fbWorker.memory.buffer.byteLength) return;

    const pixels = new Uint32Array(fbWorker.memory.buffer, pixelsPtr, fbW * fbH);

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

    let anyRunning = false;

    for (const w of workers) {
      if (!w.running) continue;

      if (w.clockPtr && w.clockPtr + 24 <= w.memory.buffer.byteLength) {
        const view = new DataView(w.memory.buffer, w.clockPtr, 24);
        view.setBigUint64(0, BigInt(now - startTime), true);
        view.setFloat32(16, deltaSec, true);
      }

      if (w === primary) {
        if (w.keyboardPtr && w.keyboardPtr + 256 <= w.memory.buffer.byteLength) {
          new Uint8Array(w.memory.buffer, w.keyboardPtr, 256).set(keyState);
        }

        if (w.mousePtr && w.mousePtr + 20 <= w.memory.buffer.byteLength) {
          const view = new DataView(w.memory.buffer, w.mousePtr, 20);
          view.setInt32(0, mouseX, true);
          view.setInt32(4, mouseY, true);
          view.setUint32(8, mouseButtons, true);
        }

        if (w.gamepadPtr && w.gamepadPtr + 20 <= w.memory.buffer.byteLength) {
          const view = new DataView(w.memory.buffer, w.gamepadPtr, 20);
          view.setUint32(0, gamepadMask, true);
          new Int16Array(w.memory.buffer, w.gamepadPtr + 4, 8).set(gamepadAxes);
        }
      }

      if (w.loggerPtr && w.loggerPtr + 12 <= w.memory.buffer.byteLength) {
        const view = new DataView(w.memory.buffer, w.loggerPtr, 12);
        const len = view.getUint32(8, true);
        if (len > 0) {
          const textBytes = new Uint8Array(w.memory.buffer, w.loggerBufPtr, len);
          const str = new TextDecoder().decode(textBytes);
          console.log(`[${w.name} Log] ${str}`);
          view.setUint32(8, 0, true);
        }
      }

      let status = 0;
      try {
        if (typeof w.instance.exports.wupdate === 'function') {
          status = w.instance.exports.wupdate();
        }
      } catch (err) {
        console.error(`[Worker ${w.name}] wupdate() error at frame ${frame}:`, err.message);
        cleanup();
        ENV.exit(1);
      }

      if (status === 1) { // WUPDATE_EXIT
        w.running = false;
      } else if (status < 0) { // WUPDATE_ERROR
        console.error(`[Worker ${w.name}] wupdate() returned error code ${status}`);
        cleanup();
        ENV.exit(1);
      } else {
        anyRunning = true;
      }
    }

    if (!anyRunning) {
      cleanup();
      ENV.exit(0);
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
