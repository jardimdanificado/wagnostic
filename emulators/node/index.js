#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const sdl = require('@kmamal/sdl');
const { WagnosticState } = require('./abi');
const WagnosticAudio = require('./audio');

// ── Gamepad Bitmasks (ABI.md) ───────────────────────────
const GP_UP    = 1 << 0;
const GP_DOWN  = 1 << 1;
const GP_LEFT  = 1 << 2;
const GP_RIGHT = 1 << 3;
const GP_A     = 1 << 4;
const GP_B     = 1 << 5;
const GP_SEL   = 1 << 6;
const GP_START = 1 << 7;

// ── Parse Arguments ──────────────────────────────────────
function parseArgs() {
  const args = process.argv.slice(2);
  let romPath = null;
  let forcedScale = 0;
  let forcedFps = 0;
  let enableAudio = true;

  for (const arg of args) {
    if (arg.startsWith('--scale=')) {
      forcedScale = parseInt(arg.split('=')[1], 10) || 0;
    } else if (arg.startsWith('--fps=')) {
      forcedFps = parseInt(arg.split('=')[1], 10) || 0;
    } else if (arg === '--no-audio') {
      enableAudio = false;
    } else if (!arg.startsWith('-') && !romPath) {
      romPath = arg;
    }
  }

  if (!romPath) {
    console.log('Wagnostic Node.js SDL2 Host');
    console.log('Usage: node index.js <path-to-rom.wasm> [--scale=N] [--fps=N] [--no-audio]');
    process.exit(1);
  }

  return { romPath, forcedScale, forcedFps, enableAudio };
}

// ── Helper to resolve pixel format for SDL2 ───────────────
function getPixelFormatInfo(state) {
  const rBits = state.rBits;
  const rShift = state.rShift;
  const gBits = state.gBits;
  const gShift = state.gShift;
  const bBits = state.bBits;
  const bShift = state.bShift;

  // Standard RGB565 (16-bit)
  if (rBits === 5 && rShift === 11 && gBits === 6 && gShift === 5 && bBits === 5 && bShift === 0) {
    return { format: 'rgb565', bpp: 2, convert: false };
  }
  // Standard RGBA8888 (32-bit)
  if (rBits === 8 && rShift === 0 && gBits === 8 && gShift === 8 && bBits === 8 && bShift === 16) {
    return { format: 'rgba32', bpp: 4, convert: false };
  }
  // Standard BGRA8888 (32-bit)
  if (rBits === 8 && rShift === 16 && gBits === 8 && gShift === 8 && bBits === 8 && bShift === 0) {
    return { format: 'bgra32', bpp: 4, convert: false };
  }

  // Fallback to RGBA32 conversion
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

// ── Main Host Function ───────────────────────────────────
async function main() {
  const { romPath, forcedScale, forcedFps, enableAudio } = parseArgs();

  const absoluteRomPath = path.resolve(process.cwd(), romPath);
  if (!fs.existsSync(absoluteRomPath)) {
    console.error(`Error: ROM file not found: ${absoluteRomPath}`);
    process.exit(1);
  }

  const wasmBytes = fs.readFileSync(absoluteRomPath);

  // WASM Imports / Stubs
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
  let audio = enableAudio ? new WagnosticAudio() : null;

  let currentWidth = 0;
  let currentHeight = 0;
  let currentScale = 1;
  let currentTitle = '';
  let conversionBuffer = null;

  let mouseButtonsMask = 0;
  let gamepadMask = 0;
  let ticks = 0;
  let isRunning = true;

  // Frame timing
  let targetFps = forcedFps || 60;
  let frameIntervalNs = BigInt(Math.floor(1e9 / targetFps));
  let lastFrameTime = process.hrtime.bigint();

  function exitCleanly() {
    if (!isRunning) return;
    isRunning = false;
    if (audio) {
      audio.close();
      audio = null;
    }
    process.exit(0);
  }

  // Input mapping for arrow keys & WASD to Gamepad mask
  function updateGamepadKey(scancode, pressed) {
    if (scancode === 82 || scancode === 26) { // Up / W
      if (pressed) gamepadMask |= GP_UP; else gamepadMask &= ~GP_UP;
    }
    if (scancode === 81 || scancode === 22) { // Down / S
      if (pressed) gamepadMask |= GP_DOWN; else gamepadMask &= ~GP_DOWN;
    }
    if (scancode === 80 || scancode === 4) { // Left / A
      if (pressed) gamepadMask |= GP_LEFT; else gamepadMask &= ~GP_LEFT;
    }
    if (scancode === 79 || scancode === 7) { // Right / D
      if (pressed) gamepadMask |= GP_RIGHT; else gamepadMask &= ~GP_RIGHT;
    }
    if (scancode === 29 || scancode === 13) { // Z / J -> A
      if (pressed) gamepadMask |= GP_A; else gamepadMask &= ~GP_A;
    }
    if (scancode === 27 || scancode === 14) { // X / K -> B
      if (pressed) gamepadMask |= GP_B; else gamepadMask &= ~GP_B;
    }
    if (scancode === 43) { // Tab -> Select
      if (pressed) gamepadMask |= GP_SEL; else gamepadMask &= ~GP_SEL;
    }
    if (scancode === 40) { // Enter -> Start
      if (pressed) gamepadMask |= GP_START; else gamepadMask &= ~GP_START;
    }
  }

  // ── Main Frame Loop ────────────────────────────────────
  function gameLoop() {
    if (!isRunning) return;

    const now = process.hrtime.bigint();
    if (now - lastFrameTime < frameIntervalNs) {
      setImmediate(gameLoop);
      return;
    }
    lastFrameTime = now;

    // Execute ROM update
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

    // Update target FPS if requested by ROM
    const romFps = state.targetFps;
    if (!forcedFps && romFps && romFps !== targetFps) {
      targetFps = romFps;
      frameIntervalNs = BigInt(Math.floor(1e9 / targetFps));
    }

    // Read window parameters
    const width = state.width || 320;
    const height = state.height || 240;
    const scale = forcedScale || state.scale || 1;
    const title = state.title || 'Wagnostic Host (Node.js)';

    // Initialize or Resize Window
    if (!window || currentWidth !== width || currentHeight !== height || currentScale !== scale || currentTitle !== title) {
      if (window && !window.destroyed) {
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

      // Window Event Listeners
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
        if (e.button === 1) mouseButtonsMask |= 1; // Left
        if (e.button === 3) mouseButtonsMask |= 2; // Right
        if (e.button === 2) mouseButtonsMask |= 4; // Middle
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

    // Audio Processing
    if (audio && isRunning) {
      audio.setup(state);
      audio.process(state, memory.buffer);
    }

    // VRAM Rendering
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
        } catch (err) {
          // Ignore render errors if window is destroyed during close
        }
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
