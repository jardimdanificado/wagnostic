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
const STRUCT_SIZE = 1024;

const OFFSETS = {
  width: 0,                // uint32
  height: 4,               // uint32
  scale: 8,                // uint32
  title: 12,               // char[128]
  dirty_rects: 140,        // uint32 (pointer)
  mouse_x: 144,            // int32
  mouse_y: 148,            // int32
  mouse_buttons: 152,      // uint32 (bitmask: 1=left, 2=right, 4=middle)
  mouse_wheel: 156,        // int32
  keys: 160,               // uint8[256] array
  gamepad_buttons: 416,    // uint32
  ticks: 420,              // uint32
  target_fps: 424,         // uint32
  vram_offset: 428,        // uint32
  r_bits: 432, r_shift: 436, // uint32
  g_bits: 440, g_shift: 444, // uint32
  b_bits: 448, b_shift: 452, // uint32
  a_bits: 456, a_shift: 460, // uint32
  x_bits: 464, x_shift: 468, // uint32
  unique: 472,             // int32
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
    this.uint8 = new Uint8Array(buffer, statePtr, STRUCT_SIZE);
  }

  updateBuffer(buffer) {
    this.buffer = buffer;
    this.view = new DataView(buffer, this.statePtr, STRUCT_SIZE);
    this.uint8 = new Uint8Array(buffer, this.statePtr, STRUCT_SIZE);
  }

  get width() { return this.view.getUint32(OFFSETS.width, true); }
  get height() { return this.view.getUint32(OFFSETS.height, true); }
  get scale() { return this.view.getUint32(OFFSETS.scale, true); }
  
  get title() {
    const rawName = this.uint8.subarray(OFFSETS.title, OFFSETS.title + 128);
    const nullIdx = rawName.indexOf(0);
    const slice = nullIdx >= 0 ? rawName.subarray(0, nullIdx) : rawName;
    return new TextDecoder('utf-8').decode(slice) || 'Untitled';
  }

  get dirtyRectsPtr() { return this.view.getUint32(OFFSETS.dirty_rects, true); }
  get targetFps() { return this.view.getUint32(OFFSETS.target_fps, true) || 60; }
  get vramOffset() { return this.view.getUint32(OFFSETS.vram_offset, true); }

  get rBits() { return this.view.getUint32(OFFSETS.r_bits, true); }
  get rShift() { return this.view.getUint32(OFFSETS.r_shift, true); }
  get gBits() { return this.view.getUint32(OFFSETS.g_bits, true); }
  get gShift() { return this.view.getUint32(OFFSETS.g_shift, true); }
  get bBits() { return this.view.getUint32(OFFSETS.b_bits, true); }
  get bShift() { return this.view.getUint32(OFFSETS.b_shift, true); }
  get aBits() { return this.view.getUint32(OFFSETS.a_bits, true); }
  get aShift() { return this.view.getUint32(OFFSETS.a_shift, true); }

  // Inputs
  setMousePos(x, y) {
    this.view.setInt32(OFFSETS.mouse_x, x, true);
    this.view.setInt32(OFFSETS.mouse_y, y, true);
  }

  setMouseButtons(buttons) {
    this.view.setUint32(OFFSETS.mouse_buttons, buttons, true);
  }

  addMouseWheel(delta) {
    const cur = this.view.getInt32(OFFSETS.mouse_wheel, true);
    this.view.setInt32(OFFSETS.mouse_wheel, cur + delta, true);
  }

  setKeyState(scancode, pressed) {
    if (scancode >= 0 && scancode < 256) {
      this.uint8[OFFSETS.keys + scancode] = pressed ? 1 : 0;
    }
  }

  setGamepadButtons(mask) {
    this.view.setUint32(OFFSETS.gamepad_buttons, mask, true);
  }

  setTicks(ticks) {
    this.view.setUint32(OFFSETS.ticks, ticks, true);
  }
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
function getPixelFormatInfo(state) {
  const rBits = state.rBits, rShift = state.rShift;
  const gBits = state.gBits, gShift = state.gShift;
  const bBits = state.bBits, bShift = state.bShift;
  const aBits = state.aBits, aShift = state.aShift;
  const xBits = state.xBits, xShift = state.xShift;

  const bpp = (rBits + gBits + bBits + aBits + xBits) / 8;
  
  if (rBits === 5 && rShift === 11 && gBits === 6 && gShift === 5 && bBits === 5 && bShift === 0) {
    return { format: 'rgb565', bpp: 2, convert: false };
  }
  if (rBits === 8 && rShift === 0 && gBits === 8 && gShift === 8 && bBits === 8 && bShift === 16) {
    return { format: 'rgba32', bpp: 4, convert: false };
  }
  if (rBits === 8 && rShift === 16 && gBits === 8 && gShift === 8 && bBits === 8 && bShift === 0) {
    return { format: 'bgra32', bpp: 4, convert: false };
  }

  return { format: 'rgba32', bpp: 4, convert: true };
}

function convertPixelsToRgba32(state, vramRaw, width, height, outBuffer) {
  const rBits = state.rBits || 5, rShift = state.rShift || 11;
  const gBits = state.gBits || 6, gShift = state.gShift || 5;
  const bBits = state.bBits || 5, bShift = state.bShift || 0;
  const aBits = state.aBits, aShift = state.aShift;

  const rMax = (1 << rBits) - 1 || 1;
  const gMax = (1 << gBits) - 1 || 1;
  const bMax = (1 << bBits) - 1 || 1;
  const aMax = aBits ? ((1 << aBits) - 1) : 255;

  const totalPixels = width * height;
  const is16 = (rBits + gBits + bBits) <= 16;

  if (is16) {
    const uint16 = new Uint16Array(vramRaw.buffer, vramRaw.byteOffset, totalPixels);
    const out32 = new Uint32Array(outBuffer.buffer, outBuffer.byteOffset, totalPixels);
    for (let i = 0; i < totalPixels; i++) {
      const p = uint16[i];
      const r = Math.round(((p >> rShift) & rMax) * 255 / rMax);
      const g = Math.round(((p >> gShift) & gMax) * 255 / gMax);
      const b = Math.round(((p >> bShift) & bMax) * 255 / bMax);
      out32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
    }
  } else {
    const uint32 = new Uint32Array(vramRaw.buffer, vramRaw.byteOffset, totalPixels);
    const out32 = new Uint32Array(outBuffer.buffer, outBuffer.byteOffset, totalPixels);
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

  const importObject = {
    env: {
      memory: new WebAssembly.Memory({ initial: 16 }),
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
  let currentTitle = '';
  let conversionBuffer = null;

  let mouseButtonsMask = 0;
  let gamepadMask = 0;
  let ticks = 0;
  let isRunning = true;

  let targetFps = forcedFps || 60;
  let frameIntervalNs = BigInt(Math.floor(1e9 / targetFps));
  let lastFrameTime = process.hrtime.bigint();

  function exitCleanly() {
    if (!isRunning) return;
    isRunning = false;
    process.exit(0);
  }

  function updateGamepadKey(scancode, pressed) {
    if (scancode === 82 || scancode === 26) {
      if (pressed) gamepadMask |= GP_UP; else gamepadMask &= ~GP_UP;
    }
    if (scancode === 81 || scancode === 22) {
      if (pressed) gamepadMask |= GP_DOWN; else gamepadMask &= ~GP_DOWN;
    }
    if (scancode === 80 || scancode === 4) {
      if (pressed) gamepadMask |= GP_LEFT; else gamepadMask &= ~GP_LEFT;
    }
    if (scancode === 79 || scancode === 7) {
      if (pressed) gamepadMask |= GP_RIGHT; else gamepadMask &= ~GP_RIGHT;
    }
    if (scancode === 29 || scancode === 13) {
      if (pressed) gamepadMask |= GP_A; else gamepadMask &= ~GP_A;
    }
    if (scancode === 27 || scancode === 14) {
      if (pressed) gamepadMask |= GP_B; else gamepadMask &= ~GP_B;
    }
    if (scancode === 43) {
      if (pressed) gamepadMask |= GP_SEL; else gamepadMask &= ~GP_SEL;
    }
    if (scancode === 40) {
      if (pressed) gamepadMask |= GP_START; else gamepadMask &= ~GP_START;
    }
  }

  function gameLoop() {
    if (!isRunning) return;

    const now = process.hrtime.bigint();
    if (now - lastFrameTime < frameIntervalNs) {
      setImmediate(gameLoop);
      return;
    }
    lastFrameTime = now;

    let statePtr = 0;
    try {
      statePtr = exports.wupdate();
    } catch (e) {
      console.error('Error during wupdate():', e);
      exitCleanly();
      return;
    }

    if (statePtr === 0) {
      exitCleanly();
      return;
    }

    if (!state || state.statePtr !== statePtr || state.buffer !== memory.buffer) {
      state = new WagnosticState(memory.buffer, statePtr);
    }

    ticks++;
    state.setTicks(ticks);

    const romFps = state.targetFps;
    if (!forcedFps && romFps && romFps !== targetFps) {
      targetFps = romFps;
      frameIntervalNs = BigInt(Math.floor(1e9 / targetFps));
    }

    const width = state.width || 320;
    const height = state.height || 240;
    const scale = forcedScale || state.scale || 1;
    const title = state.title || 'Wagnostic Host (Node.js)';

    if (!window || currentWidth !== width || currentHeight !== height || currentScale !== scale || currentTitle !== title) {
      if (window && !window.destroyed && currentWidth === width && currentHeight === height && currentScale === scale) {
        currentTitle = title;
        try { window.setTitle(title); } catch (e) {}
      } else {
        if (window && !window.destroyed) {
          window.removeAllListeners('close');
          try { window.destroy(); } catch (e) {}
        }

        currentWidth = width;
        currentHeight = height;
        currentScale = scale;
        currentTitle = title;

        try {
          window = sdl.video.createWindow({
            title: currentTitle,
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
          if (!state) return;
          const scancode = e.scancode;
          state.setKeyState(scancode, true);
          updateGamepadKey(scancode, true);
          state.setGamepadButtons(gamepadMask);
        });

        window.on('keyUp', (e) => {
          if (!state) return;
          const scancode = e.scancode;
          state.setKeyState(scancode, false);
          updateGamepadKey(scancode, false);
          state.setGamepadButtons(gamepadMask);
        });

        window.on('mouseMove', (e) => {
          if (!state) return;
          const mx = Math.floor(e.x / currentScale);
          const my = Math.floor(e.y / currentScale);
          state.setMousePos(mx, my);
        });

        window.on('mouseButtonDown', (e) => {
          if (!state) return;
          if (e.button === 1) mouseButtonsMask |= 1;
          if (e.button === 3) mouseButtonsMask |= 2;
          if (e.button === 2) mouseButtonsMask |= 4;
          state.setMouseButtons(mouseButtonsMask);
        });

        window.on('mouseButtonUp', (e) => {
          if (!state) return;
          if (e.button === 1) mouseButtonsMask &= ~1;
          if (e.button === 3) mouseButtonsMask &= ~2;
          if (e.button === 2) mouseButtonsMask &= ~4;
          state.setMouseButtons(mouseButtonsMask);
        });

        window.on('mouseWheel', (e) => {
          if (!state) return;
          state.addMouseWheel(e.dy || 0);
        });
      }
    }

    if (window && !window.destroyed && isRunning) {
      const vramOffset = state.vramOffset;
      if (vramOffset > 0) {
        const formatInfo = getPixelFormatInfo(state);
        const vramPtr = statePtr + vramOffset;
        const vramLength = width * height * formatInfo.bpp;
        const vramRaw = new Uint8Array(memory.buffer, vramPtr, vramLength);

        let renderBuffer;
        let renderFormat = formatInfo.format;
        let renderStride = width * formatInfo.bpp;

        if (formatInfo.convert) {
          renderFormat = 'rgba32';
          renderStride = width * 4;
          const requiredSize = width * height * 4;
          if (!conversionBuffer || conversionBuffer.length !== requiredSize) {
            conversionBuffer = Buffer.alloc(requiredSize);
          }
          convertPixelsToRgba32(state, vramRaw, width, height, conversionBuffer);
          renderBuffer = conversionBuffer;
        } else {
          renderBuffer = Buffer.from(vramRaw.buffer, vramRaw.byteOffset, vramRaw.byteLength);
        }

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
