// Wagnostic Web Runner - runs WASM ROMs in the browser
// Uses WebAssembly API + Canvas 2D
(function () {
  'use strict';

  // ── Constants ──────────────────────────────────────────────────────────
  const DEFAULT_WIDTH  = 320;
  const DEFAULT_HEIGHT = 240;
  const DEFAULT_BPP    = 32;
  const DEFAULT_SCALE  = 1;
  const MAX_DIRTY_RECTS = 32;
  const RECT_STRIDE     = 16; // 4 x i32 per rect (x, y, w, h)
  const TITLE_MAX       = 128;
  const KEYS_COUNT      = 256;

  // Gamepad button bitmasks
  const GP_UP    = 1 << 0;
  const GP_DOWN  = 1 << 1;
  const GP_LEFT  = 1 << 2;
  const GP_RIGHT = 1 << 3;
  const GP_A     = 1 << 4;
  const GP_B     = 1 << 5;
  const GP_SEL   = 1 << 6;
  const GP_START = 1 << 7;

  // KeyboardEvent.code -> USB HID scancode mapping
  const HID_SCANCODES = {
    'KeyA': 0x04, 'KeyB': 0x05, 'KeyC': 0x06, 'KeyD': 0x07,
    'KeyE': 0x08, 'KeyF': 0x09, 'KeyG': 0x0A, 'KeyH': 0x0B,
    'KeyI': 0x0C, 'KeyJ': 0x0D, 'KeyK': 0x0E, 'KeyL': 0x0F,
    'KeyM': 0x10, 'KeyN': 0x11, 'KeyO': 0x12, 'KeyP': 0x13,
    'KeyQ': 0x14, 'KeyR': 0x15, 'KeyS': 0x16, 'KeyT': 0x17,
    'KeyU': 0x18, 'KeyV': 0x19, 'KeyW': 0x1A, 'KeyX': 0x1B,
    'KeyY': 0x1C, 'KeyZ': 0x1D,
    'Digit1': 0x1E, 'Digit2': 0x1F, 'Digit3': 0x20, 'Digit4': 0x21,
    'Digit5': 0x22, 'Digit6': 0x23, 'Digit7': 0x24, 'Digit8': 0x25,
    'Digit9': 0x26, 'Digit0': 0x27,
    'Enter': 0x28, 'Escape': 0x29, 'Backspace': 0x2A, 'Tab': 0x2B,
    'Space': 0x2C, 'Minus': 0x2D, 'Equal': 0x2E, 'BracketLeft': 0x2F,
    'BracketRight': 0x30, 'Backslash': 0x31, 'Semicolon': 0x33,
    'Quote': 0x34, 'Backquote': 0x35, 'Comma': 0x36, 'Period': 0x37,
    'Slash': 0x38, 'CapsLock': 0x39,
    'F1': 0x3A, 'F2': 0x3B, 'F3': 0x3C, 'F4': 0x3D,
    'F5': 0x3E, 'F6': 0x3F, 'F7': 0x40, 'F8': 0x41,
    'F9': 0x42, 'F10': 0x43, 'F11': 0x44, 'F12': 0x45,
    'PrintScreen': 0x46, 'ScrollLock': 0x47, 'Pause': 0x48,
    'Insert': 0x49, 'Home': 0x4A, 'PageUp': 0x4B,
    'Delete': 0x4C, 'End': 0x4D, 'PageDown': 0x4E,
    'ArrowRight': 0x4F, 'ArrowLeft': 0x50, 'ArrowDown': 0x51, 'ArrowUp': 0x52,
    'NumLock': 0x53, 'NumpadDivide': 0x54, 'NumpadMultiply': 0x55,
    'NumpadSubtract': 0x56, 'NumpadAdd': 0x57,
    'NumpadEnter': 0x58, 'Numpad1': 0x59, 'Numpad2': 0x5A,
    'Numpad3': 0x5B, 'Numpad4': 0x5C, 'Numpad5': 0x5D,
    'Numpad6': 0x5E, 'Numpad7': 0x5F, 'Numpad8': 0x60,
    'Numpad9': 0x61, 'Numpad0': 0x62, 'NumpadDecimal': 0x63,
    'ShiftLeft': 0xE1, 'ShiftRight': 0xE5,
    'ControlLeft': 0xE0, 'ControlRight': 0xE4,
    'AltLeft': 0xE2, 'AltRight': 0xE6, 'MetaLeft': 0xE3, 'MetaRight': 0xE7
  };

  // Gamepad button name -> bit
  const GP_BTN_MAP = {
    'up': GP_UP, 'down': GP_DOWN, 'left': GP_LEFT, 'right': GP_RIGHT,
    'a': GP_A, 'b': GP_B, 'select': GP_SEL, 'start': GP_START
  };

  // ── DOM Elements ───────────────────────────────────────────────────────
  const fileInput   = document.getElementById('wasmInput');
  const canvas      = document.getElementById('gameCanvas');
  const ctx         = canvas.getContext('2d', { alpha: false, desynchronized: true });
  const emptyState  = document.getElementById('emptyState');

  // State
  let wasmInstance = null;
  let wasmMemory   = null;
  let wasmExports  = null;
  let running      = false;
  let statePtr     = 0;
  let tarBuffer    = null;

  // Screen config tracking
  let prevWidth  = 0;
  let prevHeight = 0;
  let prevBpp    = 0;
  let prevScale  = 0;

  // Input state
  let mouseX       = 0;
  let mouseY       = 0;
  let mouseButtons = 0;
  let mouseWheel   = 0;
  let keysDown     = new Uint8Array(KEYS_COUNT);
  let gamepadBtns  = 0;

  // Audio state
  let audioCtx      = null;
  let audioProcessor = null;
  let audioEnabled   = false;
  let audioFrac      = 0;

  // Pre-allocated render buffers
  let imageData = null;

  // Pixel lookup tables (LUTs) for fast format conversion


  // Timing
  let startTime = performance.now();

  // ── Helpers ────────────────────────────────────────────────────────────

  function getMem() {
    return new DataView(wasmMemory.buffer);
  }

  function readGlobals() {
    if (!statePtr) return {};
    const mem = getMem();
    const ptr = statePtr;

    let s = {
      w:               mem.getUint32(ptr + 0, true),
      h:               mem.getUint32(ptr + 4, true),
      bpp:             mem.getUint32(ptr + 8, true),
      scale:           mem.getUint32(ptr + 12, true),
      dirtyCount:      mem.getUint32(ptr + 144, true),
      dirtyRects:      ptr + 148,
      mouseX:          mem.getInt32(ptr + 660, true),
      mouseY:          mem.getInt32(ptr + 664, true),
      mouseButtons:    mem.getUint32(ptr + 668, true),
      mouseWheel:      mem.getInt32(ptr + 672, true),
      gamepadButtons:  mem.getUint32(ptr + 932, true),
      ticks:           mem.getUint32(ptr + 936, true),
      targetFps:       mem.getUint32(ptr + 940, true),
      audioSize:       mem.getUint32(ptr + 944, true),
      audioSampleRate: mem.getUint32(ptr + 948, true),
      audioBpp:        mem.getUint32(ptr + 952, true),
      audioChannels:   mem.getUint32(ptr + 956, true),
      audioWrite:      mem.getUint32(ptr + 960, true),
      audioRead:       mem.getUint32(ptr + 964, true),
      audioUnderrun:   mem.getUint32(ptr + 968, true),
      audioOverrun:    mem.getUint32(ptr + 972, true),
      vramOffset:      mem.getUint32(ptr + 976, true),
      audioBuffer:     mem.getUint32(ptr + 980, true),
      rBits:           mem.getUint32(ptr + 984, true),
      rShift:          mem.getUint32(ptr + 988, true),
      gBits:           mem.getUint32(ptr + 992, true),
      gShift:          mem.getUint32(ptr + 996, true),
      bBits:           mem.getUint32(ptr + 1000, true),
      bShift:          mem.getUint32(ptr + 1004, true),
      aBits:           mem.getUint32(ptr + 1008, true),
      aShift:          mem.getUint32(ptr + 1012, true),
    };
    if (!s.bpp) s.bpp = 32;
    if (!s.rBits && !s.gBits && !s.bBits && !s.aBits) {
        if (s.bpp === 32) {
            s.aBits = 8; s.aShift = 24; s.bBits = 8; s.bShift = 16; s.gBits = 8; s.gShift = 8; s.rBits = 8; s.rShift = 0;
        } else if (s.bpp === 24) {
            s.bBits = 8; s.bShift = 16; s.gBits = 8; s.gShift = 8; s.rBits = 8; s.rShift = 0;
        } else if (s.bpp === 16) {
            s.rBits = 5; s.rShift = 11; s.gBits = 6; s.gShift = 5; s.bBits = 5; s.bShift = 0;
        } else if (s.bpp === 8) {
            s.rBits = 3; s.rShift = 5; s.gBits = 3; s.gShift = 2; s.bBits = 2; s.bShift = 0;
        } else if (s.bpp === 4 || s.bpp === 2 || s.bpp === 1) {
            s.aBits = s.bpp; s.aShift = 0;
        }
    }
    return s;
  }

  function readTitle() {
    if (!statePtr) return '(untitled)';
    const u8 = new Uint8Array(wasmMemory.buffer, statePtr + 16, TITLE_MAX);
    let end = 0;
    while (end < TITLE_MAX && u8[end] !== 0) end++;
    return new TextDecoder().decode(u8.subarray(0, end));
  }

  // ── Canvas / Screen ────────────────────────────────────────────────────

  function resizeCanvas(w, h, scale) {
    // Defensive: a bad or absent ROM could give us zero/NaN dimensions,
    // which would make createImageData throw "Out of memory". Clamp to
    // sane bounds before doing anything.
    if (!Number.isFinite(w) || w <= 0)   w = DEFAULT_WIDTH;
    if (!Number.isFinite(h) || h <= 0)   h = DEFAULT_HEIGHT;
    if (!Number.isFinite(scale) || scale <= 0) scale = 1;
    if (w > 8192) w = 8192;
    if (h > 8192) h = 8192;
    canvas.width  = w;
    canvas.height = h;
    canvas.style.width  = (w * scale) + 'px';
    canvas.style.height = (h * scale) + 'px';
    canvas.style.imageRendering = 'pixelated';
    imageData = ctx.createImageData(w, h);
    prevWidth  = w;
    prevHeight = h;
    prevScale  = scale;
  }

  function unpackPixelsToImageData(w, h, bpp, vramPtr, pixels, u32, cx, cy, cw, ch, rb, rs, gb, gs, bb, bs, ab, ash, isGrayscale) {
    const mem = getMem();
    for (let row = 0; row < ch; row++) {
      const dstOff = row * cw;
      for (let col = 0; col < cw; col++) {
        const idx = (cy + row) * w + (cx + col);
        let px = 0;
        if (bpp === 64) px = mem.getBigUint64(vramPtr + idx * 8, true);
        else if (bpp === 32) px = mem.getUint32(vramPtr + idx * 4, true);
        else if (bpp === 24) {
          px = mem.getUint8(vramPtr + idx * 3) | (mem.getUint8(vramPtr + idx * 3 + 1) << 8) | (mem.getUint8(vramPtr + idx * 3 + 2) << 16);
        }
        else if (bpp === 16) px = mem.getUint16(vramPtr + idx * 2, true);
        else if (bpp === 8) px = mem.getUint8(vramPtr + idx);
        else if (bpp === 4) {
          const byte = mem.getUint8(vramPtr + Math.floor(idx / 2));
          px = (idx % 2 === 0) ? (byte >> 4) : (byte & 0x0F);
        }
        else if (bpp === 2) {
          const byte = mem.getUint8(vramPtr + Math.floor(idx / 4));
          px = (byte >> (6 - (idx % 4) * 2)) & 0x03;
        }
        else if (bpp === 1) {
          const byte = mem.getUint8(vramPtr + Math.floor(idx / 8));
          px = (byte >> (7 - (idx % 8))) & 1;
        }
        
        let val = (bpp === 64) ? BigInt(px) : Number(px);

        let r = 0, g = 0, b = 0, a = 255;
        if (isGrayscale) {
            let lum = 0;
            if (ab > 0) {
                if (bpp === 64) lum = Number((val >> BigInt(ash)) & ((1n << BigInt(ab)) - 1n));
                else lum = (val >> ash) & ((1 << ab) - 1);
                lum = (lum * 255 / ((1 << ab) - 1)) | 0;
            } else {
                lum = val ? 255 : 0;
            }
            r = g = b = lum;
            a = 255;
        } else {
            if (bpp === 64) {
                if (rb) r = Number((val >> BigInt(rs)) & ((1n << BigInt(rb)) - 1n)) * 255 / ((1 << rb) - 1) | 0;
                if (gb) g = Number((val >> BigInt(gs)) & ((1n << BigInt(gb)) - 1n)) * 255 / ((1 << gb) - 1) | 0;
                if (bb) b = Number((val >> BigInt(bs)) & ((1n << BigInt(bb)) - 1n)) * 255 / ((1 << bb) - 1) | 0;
                if (ab) a = Number((val >> BigInt(ash)) & ((1n << BigInt(ab)) - 1n)) * 255 / ((1 << ab) - 1) | 0;
            } else {
                if (rb) r = ((val >> rs) & ((1 << rb) - 1)) * 255 / ((1 << rb) - 1) | 0;
                if (gb) g = ((val >> gs) & ((1 << gb) - 1)) * 255 / ((1 << gb) - 1) | 0;
                if (bb) b = ((val >> bs) & ((1 << bb) - 1)) * 255 / ((1 << bb) - 1) | 0;
                if (ab) a = ((val >> ash) & ((1 << ab) - 1)) * 255 / ((1 << ab) - 1) | 0;
            }
        }
        u32[dstOff + col] = (a << 24) | (b << 16) | (g << 8) | r;
      }
    }
  }

  function renderFullFrame(state, w, h, bpp, vramPtr) {
    const isGrayscale = (state.rBits === 0 && state.gBits === 0 && state.bBits === 0 && state.aBits > 0) || 
                        (state.rBits === 0 && state.gBits === 0 && state.bBits === 0 && state.aBits === 0 && bpp < 8);
    let ab = state.aBits > 0 ? state.aBits : (bpp < 8 ? bpp : 0);
    const u32 = new Uint32Array(imageData.data.buffer);
    unpackPixelsToImageData(w, h, bpp, vramPtr, imageData.data, u32, 0, 0, w, h, state.rBits, state.rShift, state.gBits, state.gShift, state.bBits, state.bShift, ab, state.aShift, isGrayscale);
    ctx.putImageData(imageData, 0, 0);
  }

  function renderDirtyRects(state, w, h, bpp, vramPtr, dirtyCount, dirtyRectsPtr) {
    const isGrayscale = (state.rBits === 0 && state.gBits === 0 && state.bBits === 0 && state.aBits > 0) || 
                        (state.rBits === 0 && state.gBits === 0 && state.bBits === 0 && state.aBits === 0 && bpp < 8);
    let ab = state.aBits > 0 ? state.aBits : (bpp < 8 ? bpp : 0);
    const bppBytes = bpp >> 3;
    const dataView = new DataView(wasmMemory.buffer, dirtyRectsPtr, MAX_DIRTY_RECTS * RECT_STRIDE);
    const isFullScreen = dirtyCount === 1 &&
      dataView.getInt32(0, true) === 0 &&
      dataView.getInt32(4, true) === 0 &&
      dataView.getInt32(8, true) === w &&
      dataView.getInt32(12, true) === h;

    if (isFullScreen) {
      renderFullFrame(state, w, h, bpp, vramPtr);
      return;
    }

    for (let r = 0; r < dirtyCount && r < MAX_DIRTY_RECTS; r++) {
      const off = r * RECT_STRIDE;
      const rx = dataView.getInt32(off, true);
      const ry = dataView.getInt32(off + 4, true);
      const rw = dataView.getInt32(off + 8, true);
      const rh = dataView.getInt32(off + 12, true);

      let cx = rx, cy = ry, cw = rw, ch = rh;
      if (cx < 0) { cw += cx; cx = 0; }
      if (cy < 0) { ch += cy; cy = 0; }
      if (cx + cw > w) cw = w - cx;
      if (cy + ch > h) ch = h - cy;
      if (cw <= 0 || ch <= 0) continue;

      const rectData = ctx.createImageData(cw, ch);
      const pixels = rectData.data;
      const u32 = new Uint32Array(pixels.buffer);

      unpackPixelsToImageData(w, h, bpp, vramPtr, pixels, u32, cx, cy, cw, ch, state.rBits, state.rShift, state.gBits, state.gShift, state.bBits, state.bShift, ab, state.aShift, isGrayscale);
      ctx.putImageData(rectData, cx, cy);
    }
  }


  // ── Input ──────────────────────────────────────────────────────────────

  function writeInputToGlobals() {
    if (!statePtr) return;
    const mem = getMem();
    const ptr = statePtr;
    mem.setInt32(ptr + 660, mouseX, true);
    mem.setInt32(ptr + 664, mouseY, true);
    mem.setUint32(ptr + 668, mouseButtons, true);
    mem.setInt32(ptr + 672, mouseWheel, true);
    
    const keysMem = new Uint8Array(wasmMemory.buffer, ptr + 676, KEYS_COUNT);
    keysMem.set(keysDown);

    mem.setUint32(ptr + 932, gamepadBtns, true);
    mem.setUint32(ptr + 936, (performance.now() - startTime) | 0, true);
  }

  function resetInput() {
    if (!statePtr) return;
    mouseWheel = 0;
    getMem().setInt32(statePtr + 672, 0, true);
  }

  // ── Keyboard ───────────────────────────────────────────────────────────

  function onKeyDown(e) {
    resumeAudio();
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    const hid = HID_SCANCODES[e.code];
    if (hid !== undefined) {
      keysDown[hid] = 1;
      e.preventDefault();
    }
    // Map arrow keys to gamepad
    if (e.code === 'ArrowUp')    gamepadBtns |= GP_UP;
    if (e.code === 'ArrowDown')  gamepadBtns |= GP_DOWN;
    if (e.code === 'ArrowLeft')  gamepadBtns |= GP_LEFT;
    if (e.code === 'ArrowRight') gamepadBtns |= GP_RIGHT;
    if (e.code === 'KeyZ')       gamepadBtns |= GP_A;
    if (e.code === 'KeyX')       gamepadBtns |= GP_B;
    if (e.code === 'Enter')      gamepadBtns |= GP_START;
    if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') gamepadBtns |= GP_SEL;
  }

  function onKeyUp(e) {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    const hid = HID_SCANCODES[e.code];
    if (hid !== undefined) {
      keysDown[hid] = 0;
      e.preventDefault();
    }
    if (e.code === 'ArrowUp')    gamepadBtns &= ~GP_UP;
    if (e.code === 'ArrowDown')  gamepadBtns &= ~GP_DOWN;
    if (e.code === 'ArrowLeft')  gamepadBtns &= ~GP_LEFT;
    if (e.code === 'ArrowRight') gamepadBtns &= ~GP_RIGHT;
    if (e.code === 'KeyZ')       gamepadBtns &= ~GP_A;
    if (e.code === 'KeyX')       gamepadBtns &= ~GP_B;
    if (e.code === 'Enter')      gamepadBtns &= ~GP_START;
    if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') gamepadBtns &= ~GP_SEL;
  }

  // ── Mouse ──────────────────────────────────────────────────────────────

  function onMouseMove(e) {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    mouseX = ((e.clientX - rect.left) * scaleX) | 0;
    mouseY = ((e.clientY - rect.top) * scaleY) | 0;
  }

  function onMouseDown(e) {
    resumeAudio();
    if (e.button === 0) mouseButtons |= 1; // left
    if (e.button === 2) mouseButtons |= 2; // right
    e.preventDefault();
  }

  function onMouseUp(e) {
    if (e.button === 0) mouseButtons &= ~1;
    if (e.button === 2) mouseButtons &= ~2;
    e.preventDefault();
  }

  function onWheel(e) {
    // SDL: wheel.y > 0 = scroll UP. Browser: deltaY > 0 = scroll DOWN.
    // Invert to match SDL convention. Pass through raw magnitude.
    mouseWheel -= Math.sign(e.deltaY);
    e.preventDefault();
  }

  function onContextMenu(e) { e.preventDefault(); }

  // ── Gamepad Buttons ────────────────────────────────────────────────────

  function setupGamepadButtons() {
    const buttons = document.querySelectorAll('[data-btn]');
    buttons.forEach(function (btn) {
      const name = btn.getAttribute('data-btn');
      const bit = GP_BTN_MAP[name];
      if (bit === undefined) return;

      btn.addEventListener('mousedown', function (e) {
        resumeAudio();
        gamepadBtns |= bit;
        e.preventDefault();
      });
      btn.addEventListener('mouseup', function (e) {
        gamepadBtns &= ~bit;
        e.preventDefault();
      });
      btn.addEventListener('mouseleave', function () {
        gamepadBtns &= ~bit;
      });
      // Touch support
      btn.addEventListener('touchstart', function (e) {
        resumeAudio();
        gamepadBtns |= bit;
        e.preventDefault();
      });
      btn.addEventListener('touchend', function (e) {
        gamepadBtns &= ~bit;
        e.preventDefault();
      });
    });
  }

  // ── Audio ──────────────────────────────────────────────────────────────

  function resumeAudio() {
    if (audioCtx && audioCtx.state === 'suspended') {
      audioCtx.resume().catch(function () {});
    }
  }

  function initAudio(sampleRate, channels) {
    if (audioProcessor) {
      audioProcessor.disconnect();
      audioProcessor = null;
    }
    if (audioCtx && sampleRate && audioCtx.sampleRate !== sampleRate) {
      try { audioCtx.close(); } catch (e) {}
      audioCtx = null;
    }
    if (!audioCtx) {
      const AudioCtx = window.AudioContext || window.webkitAudioContext;
      try {
        if (sampleRate > 0) {
          audioCtx = new AudioCtx({ sampleRate: sampleRate });
        } else {
          audioCtx = new AudioCtx();
        }
      } catch (e) {
        audioCtx = new AudioCtx();
      }
    }
    if (audioCtx.state === 'suspended') {
      audioCtx.resume().catch(function () {});
    }

    // Use ScriptProcessorNode for broad compatibility
    const bufferSize = 2048;
    audioProcessor = audioCtx.createScriptProcessor(bufferSize, 0, channels || 1);
    audioEnabled = true;
    audioFrac = 0;

    audioProcessor.onaudioprocess = function (e) {
      if (!wasmInstance || !audioEnabled) return;

      const g = readGlobals();
      if (!statePtr || g.audioSize === 0 || g.audioBpp === 0 || !g.audioBuffer) return;

      const writePtr   = g.audioWrite;
      let   readPtr    = g.audioRead;
      const bufPtr     = statePtr + g.audioBuffer;
      const size       = g.audioSize;
      const bpp        = g.audioBpp;
      const channels   = g.audioChannels || 1;
      const sampleRate = g.audioSampleRate || audioCtx.sampleRate;
      const frameSize  = bpp * channels;

      let available = writePtr - readPtr;
      if (available < 0) available += size;

      const outputChannels = e.outputBuffer.numberOfChannels;
      const framesPerBuffer = e.outputBuffer.length;
      const outputs = [];
      for (let ch = 0; ch < outputChannels; ch++) {
        outputs.push(e.outputBuffer.getChannelData(ch));
      }

      const step = (sampleRate > 0 && audioCtx.sampleRate > 0) ? (sampleRate / audioCtx.sampleRate) : 1.0;
      const memView = getMem();
      const u8View = new Uint8Array(wasmMemory.buffer);
      let underrun = false;

      for (let i = 0; i < framesPerBuffer; i++) {
        if (available < frameSize) {
          underrun = true;
          for (let ch = 0; ch < outputChannels; ch++) {
            outputs[ch][i] = 0;
          }
          continue;
        }

        for (let ch = 0; ch < outputChannels; ch++) {
          let sample = 0;
          if (ch < channels) {
            const sampleByteOff = bufPtr + ((readPtr + ch * bpp) % size);
            if (bpp === 1) {
              sample = (u8View[sampleByteOff] - 128) / 128.0;
            } else if (bpp === 2) {
              sample = memView.getInt16(sampleByteOff, true) / 32768.0;
            } else if (bpp === 4) {
              sample = memView.getFloat32(sampleByteOff, true);
            }
          } else if (channels === 1 && ch === 1) {
            sample = outputs[0][i];
          }
          outputs[ch][i] = sample;
        }

        audioFrac += step;
        while (audioFrac >= 1.0) {
          if (available >= frameSize) {
            readPtr = (readPtr + frameSize) % size;
            available -= frameSize;
          } else {
            underrun = true;
          }
          audioFrac -= 1.0;
        }
      }

      if (statePtr) {
        memView.setUint32(statePtr + 964, readPtr, true);
        if (underrun) {
          const uOff = statePtr + 968;
          memView.setUint32(uOff, memView.getUint32(uOff, true) + 1, true);
        }
      }
    };

    audioProcessor.connect(audioCtx.destination);
  }

  function stopAudio() {
    if (audioProcessor) {
      audioProcessor.disconnect();
      audioProcessor = null;
    }
    audioEnabled = false;
  }

  // ── Main Loop ──────────────────────────────────────────────────────────

  function frame() {
    if (!running) return;

    // 1. Write input to globals
    writeInputToGlobals();

    // 2. Call wupdate(); exit if 0
    let ret;
    try {
      ret = wasmExports.wupdate();
    } catch (err) {
      console.error('wupdate() threw:', err);
      running = false;
      return;
    }
    if (ret === 0) {
      running = false;
      console.log('ROM exited (wupdate returned 0)');
      return;
    }
    statePtr = ret;

    // 3. Read globals and auto-detect config changes
    const g = readGlobals();
    const w = g.w || DEFAULT_WIDTH;
    const h = g.h || DEFAULT_HEIGHT;
    const bpp = g.bpp || DEFAULT_BPP;
    const scale = g.scale || DEFAULT_SCALE;

    if (w !== prevWidth || h !== prevHeight || bpp !== prevBpp || scale !== prevScale) {
      resizeCanvas(w, h, scale);
    }
    // Initialize audio if audio globals changed or were enabled after startup
    if (g.audioSampleRate > 0 && g.audioBpp > 0 && g.audioChannels > 0) {
      if (!audioEnabled) {
        initAudio(g.audioSampleRate, g.audioChannels);
      }
    }

    // 4. Render dirty rects
    if (g.dirtyCount > 0) {
      renderDirtyRects(g, w, h, bpp, statePtr + g.vramOffset, g.dirtyCount, g.dirtyRects);
      // Reset dirty count
      getMem().setUint32(statePtr + 144, 0, true);
    }
    


    // 6. Reset mouse wheel
    resetInput();
    
    // Check if ROM wants a specific framerate
    var target = g.targetFps;
    if (target > 0) {
      setTimeout(frame, 1000 / target);
    } else {
      requestAnimationFrame(frame);
    }
  }

  // ── WASM Loading ───────────────────────────────────────────────────────

  // Simple TAR file extraction
  function extractFromTar(buf, filename) {
    const dv = new DataView(buf);
    let offset = 0;
    let lastFound = null;
    while (offset + 512 <= buf.byteLength) {
      if (dv.getUint8(offset) === 0) break; // end of tar
      let name = '';
      for (let i = 0; i < 100; i++) {
        let b = dv.getUint8(offset + i);
        if (b === 0) break;
        name += String.fromCharCode(b);
      }
      let sizeStr = '';
      for (let i = 124; i < 135; i++) {
        let b = dv.getUint8(offset + i);
        if (b >= 48 && b <= 55) { // '0' to '7'
           sizeStr += String.fromCharCode(b);
        }
      }
      let size = parseInt(sizeStr || '0', 8);
      if (name === filename) {
        lastFound = new Uint8Array(buf, offset + 512, size);
      }
      let skip = size + ((512 - (size % 512)) % 512);
      offset += 512 + skip;
    }
    return lastFound;
  }

  function loadRomFromBuffer(buf) {
    let wasmBuffer = buf;
    tarBuffer = null;
    
    // Check if TAR by trying to extract main.wasm
    const mainWasm = extractFromTar(buf, 'main.wasm');
    if (mainWasm) {
      tarBuffer = buf;
      wasmBuffer = mainWasm.buffer.slice(mainWasm.byteOffset, mainWasm.byteOffset + mainWasm.byteLength);
    }

    const wasmImports = {};

    WebAssembly.instantiate(wasmBuffer, wasmImports).then(function (result) {
      wasmInstance = result.instance;
      wasmExports  = wasmInstance.exports;
      wasmMemory   = wasmExports.memory || wasmExports.linear_memory;

      if (!wasmMemory) {
        console.error('ROM does not export memory');
        return;
      }

      if (typeof wasmExports.wupdate !== 'function') {
        console.error('ROM does not export wupdate() function');
        return;
      }
      
      // Get initial state pointer
      statePtr = wasmExports.wupdate();
      if (!statePtr) {
        console.error('Initial wupdate() returned 0');
        return;
      }

      const g = readGlobals();
      resizeCanvas(g.w || DEFAULT_WIDTH, g.h || DEFAULT_HEIGHT, g.scale || DEFAULT_SCALE);

      if (emptyState) emptyState.style.display = 'none';
      canvas.style.display = 'block';

      if (g.audioSampleRate > 0 && g.audioBpp > 0 && g.audioChannels > 0) {
        initAudio(g.audioSampleRate, g.audioChannels);
      }

      if (audioCtx && audioCtx.state === 'suspended') {
        audioCtx.resume().catch(function () {});
      }

      console.log('ROM loaded. Title:', readTitle());
      
      running = true;
      var nextFrame = function () { requestAnimationFrame(frame); };
      var target = g.targetFps;
      if (target > 0) {
        var delay = 1000 / target;
        nextFrame = function () { setTimeout(frame, delay); };
      }
      nextFrame();
    }).catch(function (err) {
      console.error('Failed to load ROM:', err);
    });
  }

  function loadRom(wasmUrl) {
    running = false;
    stopAudio();
    startTime = performance.now();

    keysDown.fill(0);
    gamepadBtns = 0;
    mouseButtons = 0;
    mouseWheel = 0;

    fetch(wasmUrl).then(function (r) { return r.arrayBuffer(); })
      .then(loadRomFromBuffer).catch(function (err) {
        console.error('Fetch failed:', err);
      });
  }

  // ── Event Listeners ────────────────────────────────────────────────────

  // File input
  fileInput.addEventListener('change', function (e) {
    const file = e.target.files[0];
    if (!file) return;
    
    const reader = new FileReader();
    reader.onload = function(evt) {
      loadRomFromBuffer(evt.target.result);
    };
    reader.readAsArrayBuffer(file);
  });

  // Keyboard
  document.addEventListener('keydown', onKeyDown);
  document.addEventListener('keyup', onKeyUp);

  // Mouse on canvas
  canvas.addEventListener('mousemove', onMouseMove);
  canvas.addEventListener('mousedown', onMouseDown);
  canvas.addEventListener('mouseup', onMouseUp);
  canvas.addEventListener('wheel', onWheel, { passive: false });
  canvas.addEventListener('contextmenu', onContextMenu);

  // Touch support on canvas for mouse simulation
  canvas.addEventListener('touchstart', function (e) {
    resumeAudio();
    const touch = e.touches[0];
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    mouseX = ((touch.clientX - rect.left) * scaleX) | 0;
    mouseY = ((touch.clientY - rect.top) * scaleY) | 0;
    mouseButtons |= 1;
    e.preventDefault();
  }, { passive: false });

  canvas.addEventListener('touchmove', function (e) {
    const touch = e.touches[0];
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    mouseX = ((touch.clientX - rect.left) * scaleX) | 0;
    mouseY = ((touch.clientY - rect.top) * scaleY) | 0;
    e.preventDefault();
  }, { passive: false });

  canvas.addEventListener('touchend', function (e) {
    mouseButtons &= ~1;
    e.preventDefault();
  }, { passive: false });

  // Gamepad virtual buttons
  setupGamepadButtons();

  // Initial canvas style
  canvas.style.imageRendering = 'pixelated';

  console.log('Wagnostic Web Runner ready. Load a .wasm ROM to begin.');

  window.wagnosticLoadRomFromBuffer = loadRomFromBuffer;

  const urlParams = new URLSearchParams(window.location.search);
  const autoRom = urlParams.get('rom');
  if (autoRom) {
    const romUrl = autoRom.endsWith('.wasm') ? autoRom : autoRom + '.wasm';
    loadRom(romUrl);
  }
})();
