#!/usr/bin/env node

/**
 * Wagnostic Single-File Node.js SDL2 Host (`wagnostic.js`)
 * 
 * Standalone Host written for Node.js.
 * Requires only @kmamal/sdl for native windowing, rendering, and audio.
 */

const fs = require('fs');
const path = require('path');
const sdl = require('@kmamal/sdl');

// ── ABI Offsets & Constants (ABI.md) ────────────────────
const STRUCT_SIZE = 48;

const OFFSETS = {
  width: 0,                // uint32
  height: 4,               // uint32
  r_bits: 8, r_shift: 12,  // uint32
  g_bits: 16, g_shift: 20, // uint32
  b_bits: 24, b_shift: 28, // uint32
  a_bits: 32, a_shift: 36, // uint32
  vram_offset: 40,         // uint32
  dirty_rects: 44,         // uint32 (pointer)
};

// Gamepad Bitmasks
const GP_UP    = 1 << 0;
const GP_DOWN  = 1 << 1;
const GP_LEFT  = 1 << 2;
const GP_RIGHT = 1 << 3;
const GP_A     = 1 << 4;
const GP_B     = 1 << 5;
const GP_SEL   = 1 << 6;
const GP_START = 1 << 7;

// ── WagnosticState Wrapper ──────────────────────────────
class WagnosticState {
  constructor(buffer, statePtr) {
    this.buffer = buffer;
    this.statePtr = statePtr;
    this.view = new DataView(buffer, statePtr, STRUCT_SIZE);
  }

  updateBuffer(buffer) {
    this.buffer = buffer;
    this.view = new DataView(buffer, this.statePtr, STRUCT_SIZE);
  }

  get width() { return this.view.getUint32(OFFSETS.width, true); }
  get height() { return this.view.getUint32(OFFSETS.height, true); }

  get dirtyRectsPtr() { return this.view.getUint32(OFFSETS.dirty_rects, true); }
  get vramOffset() { return this.view.getUint32(OFFSETS.vram_offset, true); }

  get rBits() { return this.view.getUint32(OFFSETS.r_bits, true); }
  get rShift() { return this.view.getUint32(OFFSETS.r_shift, true); }
  get gBits() { return this.view.getUint32(OFFSETS.g_bits, true); }
  get gShift() { return this.view.getUint32(OFFSETS.g_shift, true); }
  get bBits() { return this.view.getUint32(OFFSETS.b_bits, true); }
  get bShift() { return this.view.getUint32(OFFSETS.b_shift, true); }
  get aBits() { return this.view.getUint32(OFFSETS.a_bits, true); }
  get aShift() { return this.view.getUint32(OFFSETS.a_shift, true); }
}

// ── Parse Arguments ──────────────────────────────────────
function parseArgs() {
  const args = process.argv.slice(2);
  let romPath = null;
  let forcedScale = 0;
  let forcedFps = 0;

  for (const arg of args) {
    if (arg.startsWith('--scale=')) {
      forcedScale = parseInt(arg.split('=')[1], 10) || 0;
    } else if (arg.startsWith('--fps=')) {
      forcedFps = parseInt(arg.split('=')[1], 10) || 0;
    } else if (!arg.startsWith('-') && !romPath) {
      romPath = arg;
    }
  }

  if (!romPath) {
    console.log('Wagnostic Single-File Node.js SDL2 Host');
    console.log('Usage: node wagnostic.js <path-to-rom.wasm> [--scale=N] [--fps=N]');
    process.exit(1);
  }

  return { romPath, forcedScale, forcedFps };
}

// ── Helper to resolve pixel format for SDL2 ───────────────
function getVramLength(state, width, height) {
  let maxBit = 0;
  function check(bits, shift) {
    if (bits > 0 && (shift + bits) > maxBit) maxBit = shift + bits;
  }
  check(state.rBits, state.rShift);
  check(state.gBits, state.gShift);
  check(state.bBits, state.bShift);
  check(state.aBits, state.aShift);
  check(state.xBits, state.xShift);

  if (maxBit <= 1 && maxBit > 0) return Math.ceil((width * height) / 8);
  if (maxBit <= 2 && maxBit > 0) return Math.ceil((width * height) / 4);
  if (maxBit <= 4 && maxBit > 0) return Math.ceil((width * height) / 2);
  if (maxBit <= 8 && maxBit > 0) return width * height;
  if (maxBit <= 16 && maxBit > 0) return width * height * 2;
  return width * height * 4;
}

function convertPixelsToRgba32(state, vramRaw, width, height, outBuffer) {
  const rBits = state.rBits, rShift = state.rShift;
  const gBits = state.gBits, gShift = state.gShift;
  const bBits = state.bBits, bShift = state.bShift;
  const aBits = state.aBits, aShift = state.aShift;
  const xBits = state.xBits, xShift = state.xShift;

  let maxBit = 0;
  function check(bits, shift) {
    if (bits > 0 && (shift + bits) > maxBit) maxBit = shift + bits;
  }
  check(rBits, rShift);
  check(gBits, gShift);
  check(bBits, bShift);
  check(aBits, aShift);
  check(xBits, xShift);

  const totalPixels = width * height;
  const out32 = new Uint32Array(outBuffer.buffer, outBuffer.byteOffset, totalPixels);

  if (maxBit <= 1 && maxBit > 0) {
    for (let i = 0; i < totalPixels; i++) {
      const bit = (vramRaw[i >> 3] >> (7 - (i & 7))) & 1;
      const val = bit ? 255 : 0;
      out32[i] = (255 << 24) | (val << 16) | (val << 8) | val;
    }
  } else if (maxBit <= 2 && maxBit > 0) {
    for (let i = 0; i < totalPixels; i++) {
      const bits = (vramRaw[i >> 2] >> ((3 - (i & 3)) * 2)) & 3;
      const val = Math.round(bits * 255 / 3);
      out32[i] = (255 << 24) | (val << 16) | (val << 8) | val;
    }
  } else if (maxBit <= 4 && maxBit > 0) {
    for (let i = 0; i < totalPixels; i++) {
      const bits = (vramRaw[i >> 1] >> ((1 - (i & 1)) * 4)) & 15;
      const val = Math.round(bits * 255 / 15);
      out32[i] = (255 << 24) | (val << 16) | (val << 8) | val;
    }
  } else if (maxBit <= 8 && maxBit > 0) {
    const rMax = (1 << rBits) - 1 || 1;
    const gMax = (1 << gBits) - 1 || 1;
    const bMax = (1 << bBits) - 1 || 1;
    const isRgb = (rBits || gBits || bBits);
    for (let i = 0; i < totalPixels; i++) {
      const p = vramRaw[i];
      if (isRgb) {
        const r = Math.round(((p >> rShift) & rMax) * 255 / rMax);
        const g = Math.round(((p >> gShift) & gMax) * 255 / gMax);
        const b = Math.round(((p >> bShift) & bMax) * 255 / bMax);
        out32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
      } else {
        out32[i] = (255 << 24) | (p << 16) | (p << 8) | p;
      }
    }
  } else if (maxBit <= 16 && maxBit > 0) {
    const rMax = (1 << rBits) - 1 || 1;
    const gMax = (1 << gBits) - 1 || 1;
    const bMax = (1 << bBits) - 1 || 1;
    const uint16 = new Uint16Array(vramRaw.buffer, vramRaw.byteOffset, totalPixels);
    for (let i = 0; i < totalPixels; i++) {
      const p = uint16[i];
      const r = Math.round(((p >> rShift) & rMax) * 255 / rMax);
      const g = Math.round(((p >> gShift) & gMax) * 255 / gMax);
      const b = Math.round(((p >> bShift) & bMax) * 255 / bMax);
      out32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
    }
  } else {
    const rMax = (1 << rBits) - 1 || 1;
    const gMax = (1 << gBits) - 1 || 1;
    const bMax = (1 << bBits) - 1 || 1;
    const aMax = aBits ? ((1 << aBits) - 1) : 255;
    const uint32 = new Uint32Array(vramRaw.buffer, vramRaw.byteOffset, totalPixels);
    for (let i = 0; i < totalPixels; i++) {
      const p = uint32[i];
      const r = Math.round(((p >> rShift) & rMax) * 255 / rMax);
      const g = Math.round(((p >> gShift) & gMax) * 255 / gMax);
      const b = Math.round(((p >> bShift) & bMax) * 255 / bMax);
      const a = aBits ? Math.round(((p >> aShift) & aMax) * 255 / aMax) : 255;
      out32[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
  }
}

// ── Main Host Execution ───────────────────────────────────
async function main() {
  const { romPath, forcedScale, forcedFps } = parseArgs();

  const absoluteRomPath = path.resolve(process.cwd(), romPath);
  if (!fs.existsSync(absoluteRomPath)) {
    console.error(`Error: ROM file not found: ${absoluteRomPath}`);
    process.exit(1);
  }

  const wasmBytes = fs.readFileSync(absoluteRomPath);

  let currentTitle = 'Wagnostic Host (Node.js)';
  let lastTitleWasmPtr = 0;

  function readWasmString(ptr) {
    if (!ptr || !memory) return '';
    const bytes = new Uint8Array(memory.buffer, ptr);
    let len = 0;
    while (len < 1024 && bytes[len] !== 0) len++;
    return new TextDecoder().decode(bytes.subarray(0, len));
  }

  function writeWasmString(ptr, str) {
    if (!ptr || !memory) return;
    const encoder = new TextEncoder();
    const encoded = encoder.encode(str);
    const bytes = new Uint8Array(memory.buffer, ptr, encoded.length + 1);
    bytes.set(encoded);
    bytes[encoded.length] = 0;
  }

  const importObject = {
    env: {
      memory: new WebAssembly.Memory({ initial: 16 }),
      wextension: (namePtr, dataPtr) => {
        const name = readWasmString(namePtr);
        if (name === 'title.set') {
          if (dataPtr) {
            currentTitle = readWasmString(dataPtr);
            lastTitleWasmPtr = dataPtr;
            if (window && !window.destroyed) {
              try { window.setTitle(currentTitle); } catch (e) {}
            }
            return dataPtr;
          }
          return 0;
        }
        if (name === 'title.get') {
          if (dataPtr) {
            writeWasmString(dataPtr, currentTitle);
            return dataPtr;
          }
          return lastTitleWasmPtr;
        }
        if (name === 'std:keyboard') {
          if (dataPtr) {
            keyboardWasmPtr = dataPtr;
          } else if (!keyboardWasmPtr && statePtr) {
            keyboardWasmPtr = statePtr + STRUCT_SIZE;
          }
          return keyboardWasmPtr;
        }
        if (name === 'std:mouse') {
          if (dataPtr) {
            mouseWasmPtr = dataPtr;
          } else if (!mouseWasmPtr && statePtr) {
            mouseWasmPtr = statePtr + STRUCT_SIZE + 256;
          }
          return mouseWasmPtr;
        }
        if (name === 'std:gamepad') {
          if (dataPtr) {
            gamepadWasmPtr = dataPtr;
          } else if (!gamepadWasmPtr && statePtr) {
            gamepadWasmPtr = statePtr + STRUCT_SIZE + 256 + 16;
          }
          return gamepadWasmPtr;
        }
        return 0;
      },
      abort: () => console.error('WASM Aborted'),
    },
    wasi_snapshot_preview1: {
      fd_write: () => 0,
      fd_seek: () => 0,
      fd_close: () => 0,
      proc_exit: (code) => process.exit(code),
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

  if (!exports.wupdate) {
    console.error('Error: ROM does not export "wupdate()" function.');
    process.exit(1);
  }

  const memory = exports.memory || importObject.env.memory;
  let state = null;
  let window = null;

  let currentWidth = 0;
  let currentHeight = 0;
  let currentScale = 1;

  let isRunning = true;
  let statePtr = 0;
  let keyboardWasmPtr = 0;
  let mouseWasmPtr = 0;
  let gamepadWasmPtr = 0;
  let conversionBuffer = null;

  const keyBuffer = new Uint8Array(256);
  let mouseX = 0, mouseY = 0;
  let mouseButtonsMask = 0;
  let mouseWheelDelta = 0;
  let gamepadMask = 0;

  function exitCleanly() {
    isRunning = false;
    if (window && !windowDestroyed(window)) {
      try { window.destroy(); } catch (e) {}
    }
    process.exit(0);
  }

  function windowDestroyed(win) {
    return !win || win.destroyed;
  }

  function getMem() {
    return new DataView(memory.buffer);
  }

  function updateGamepadKey(scancode, isDown) {
    let bit = 0;
    if (scancode === 0x52) bit = GP_UP;    // ArrowUp
    if (scancode === 0x51) bit = GP_DOWN;  // ArrowDown
    if (scancode === 0x50) bit = GP_LEFT;  // ArrowLeft
    if (scancode === 0x4F) bit = GP_RIGHT; // ArrowRight
    if (scancode === 0x1D) bit = GP_A;     // Z
    if (scancode === 0x1B) bit = GP_B;     // X
    if (scancode === 0x28) bit = GP_START; // Enter
    if (scancode === 0xE1 || scancode === 0xE5) bit = GP_SEL; // Shift

    if (bit) {
      if (isDown) gamepadMask |= bit;
      else gamepadMask &= ~bit;
    }
  }

  let lastFrameTime = process.hrtime.bigint();
  let targetFps = forcedFps || 60;
  let frameIntervalNs = BigInt(Math.floor(1e9 / targetFps));

  function gameLoop() {
    if (!isRunning) return;

    const now = process.hrtime.bigint();
    const elapsed = now - lastFrameTime;

    if (elapsed < frameIntervalNs) {
      setImmediate(gameLoop);
      return;
    }
    lastFrameTime = now;

    // Update mapped peripheral memory buffers directly
    if (keyboardWasmPtr && keyboardWasmPtr + 256 <= memory.buffer.byteLength) {
      new Uint8Array(memory.buffer, keyboardWasmPtr, 256).set(keyBuffer);
    }
    if (mouseWasmPtr && mouseWasmPtr + 16 <= memory.buffer.byteLength) {
      const view = new DataView(memory.buffer, mouseWasmPtr, 16);
      view.setInt32(0, mouseX, true);
      view.setInt32(4, mouseY, true);
      view.setUint32(8, mouseButtonsMask, true);
      view.setInt32(12, mouseWheelDelta, true);
    }
    if (gamepadWasmPtr && gamepadWasmPtr + 4 <= memory.buffer.byteLength) {
      new DataView(memory.buffer, gamepadWasmPtr, 4).setUint32(0, gamepadMask, true);
    }

    let ret = 0;
    try {
      ret = exports.wupdate();
    } catch (err) {
      console.error('wupdate() threw an error:', err.message);
      exitCleanly();
      return;
    }

    if (ret === 0) {
      exitCleanly();
      return;
    }

    statePtr = ret;
    state = new WagnosticState(memory.buffer, statePtr);
    mouseWheelDelta = 0;

    const width = state.width || 320;
    const height = state.height || 240;
    const scale = forcedScale || 1;

    if (!window || currentWidth !== width || currentHeight !== height || currentScale !== scale) {
      if (window && !window.destroyed) {
        window.removeAllListeners('close');
        try { window.destroy(); } catch (e) {}
      }

      currentWidth = width;
      currentHeight = height;
      currentScale = scale;

      try {
        window = sdl.video.createWindow({
          title: 'Wagnostic Host (Node.js)',
          width: width * scale,
          height: height * scale,
          resizable: true,
        });
      } catch (err) {
        console.error('Failed to create SDL window:', err.message);
        exitCleanly();
        return;
      }

      window.on('close', () => {
        exitCleanly();
      });

      window.on('keyDown', (e) => {
        const scancode = e.scancode;
        if (scancode >= 0 && scancode < 256) keyBuffer[scancode] = 1;
        updateGamepadKey(scancode, true);
      });

      window.on('keyUp', (e) => {
        const scancode = e.scancode;
        if (scancode >= 0 && scancode < 256) keyBuffer[scancode] = 0;
        updateGamepadKey(scancode, false);
      });

      window.on('mouseMove', (e) => {
        mouseX = Math.floor(e.x / currentScale);
        mouseY = Math.floor(e.y / currentScale);
      });

      window.on('mouseButtonDown', (e) => {
        if (e.button === 1) mouseButtonsMask |= 1;
        if (e.button === 3) mouseButtonsMask |= 2;
        if (e.button === 2) mouseButtonsMask |= 4;
      });

      window.on('mouseButtonUp', (e) => {
        if (e.button === 1) mouseButtonsMask &= ~1;
        if (e.button === 3) mouseButtonsMask &= ~2;
        if (e.button === 2) mouseButtonsMask &= ~4;
      });

      window.on('mouseWheel', (e) => {
        mouseWheelDelta += (e.dy || 0);
      });
    }

    if (window && !window.destroyed && isRunning) {
      const vramOffset = state.vramOffset;
      if (vramOffset > 0) {
        const vramLength = getVramLength(state, width, height);
        const vramPtr = statePtr + vramOffset;
        const vramRaw = new Uint8Array(memory.buffer, vramPtr, vramLength);

        let renderBuffer;
        let renderFormat = 'rgba32';
        let renderStride = width * 4;
        const requiredSize = width * height * 4;

        if (!conversionBuffer || conversionBuffer.length !== requiredSize) {
          conversionBuffer = Buffer.alloc(requiredSize);
        }
        convertPixelsToRgba32(state, vramRaw, width, height, conversionBuffer);
        renderBuffer = conversionBuffer;

        try {
          window.render(width, height, renderStride, renderFormat, renderBuffer);
        } catch (err) {}
      }
    }

    if (isRunning) {
      setImmediate(gameLoop);
    }
  }

  gameLoop();
}

main().catch(err => {
  console.error('Fatal error in Wagnostic Node Host:', err);
  process.exit(0);
});
