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
  let lastFrameTime = 0;

  // ── Helpers ────────────────────────────────────────────────────────────

  function getMem() {
    return new DataView(wasmMemory.buffer);
  }

  function computeBpp(s) {
    // Derive BPP from the highest (shift + bits) of any active channel.
    // x_bits/x_shift are for padding/unused channels that still consume bits.
    let maxBit = 0;
    function check(bits, shift) {
      if (bits > 0 && (shift + bits) > maxBit) maxBit = shift + bits;
    }
    check(s.rBits, s.rShift);
    check(s.gBits, s.gShift);
    check(s.bBits, s.bShift);
    check(s.aBits, s.aShift);
    check(s.xBits, s.xShift);
    if (maxBit <= 0)  return 32;  // no channel info → assume 32bpp
    if (maxBit <= 1)  return 1;
    if (maxBit <= 2)  return 2;
    if (maxBit <= 4)  return 4;
    if (maxBit <= 8)  return 8;
    if (maxBit <= 16) return 16;
    if (maxBit <= 24) return 24;
    if (maxBit <= 32) return 32;
    return 64;
  }

  function readGlobals() {
    if (!statePtr) return {};
    const mem = getMem();
    const ptr = statePtr;

    // Layout (1024 bytes, no packing):
    // +0   width      uint32
    // +4   height     uint32
    // +8   scale      uint32
    // +12  title      char[128]
    // +140 dirty_rects uint32  (WASM pointer to { uint32 count; Rect rects[32]; })
    // +144 mouse_x    int32
    // +148 mouse_y    int32
    // +152 mouse_buttons uint32
    // +156 mouse_wheel int32
    // +160 keys       uint8[256]
    // +416 gamepad_buttons uint32
    // +420 ticks      uint32
    // +424 target_fps uint32
    // +428 audio_size uint32
    // +432 audio_sample_rate uint32
    // +436 audio_bpp  uint32
    // +440 audio_channels uint32
    // +444 audio_write uint32
    // +448 audio_read uint32
    // +452 audio_underrun uint32
    // +456 audio_overrun uint32
    // +460 vram_offset uint32
    // +464 audio_buffer_offset uint32
    // +468 r_bits uint32  +472 r_shift uint32
    // +476 g_bits uint32  +480 g_shift uint32
    // +484 b_bits uint32  +488 b_shift uint32
    // +492 a_bits uint32  +496 a_shift uint32
    // +500 x_bits uint32  +504 x_shift uint32
    // +508 reserved[516]
    let s = {
      w:               mem.getUint32(ptr + 0, true),
      h:               mem.getUint32(ptr + 4, true),
      scale:           mem.getUint32(ptr + 8, true),
      dirtyRectsPtr:   mem.getUint32(ptr + 140, true),
      mouseX:          mem.getInt32(ptr + 144, true),
      mouseY:          mem.getInt32(ptr + 148, true),
      mouseButtons:    mem.getUint32(ptr + 152, true),
      mouseWheel:      mem.getInt32(ptr + 156, true),
      gamepadButtons:  mem.getUint32(ptr + 416, true),
      ticks:           mem.getUint32(ptr + 420, true),
      targetFps:       mem.getUint32(ptr + 424, true),
      audioSize:       mem.getUint32(ptr + 428, true),
      audioSampleRate: mem.getUint32(ptr + 432, true),
      audioBpp:        mem.getUint32(ptr + 436, true),
      audioChannels:   mem.getUint32(ptr + 440, true),
      audioWrite:      mem.getUint32(ptr + 444, true),
      audioRead:       mem.getUint32(ptr + 448, true),
      audioUnderrun:   mem.getUint32(ptr + 452, true),
      audioOverrun:    mem.getUint32(ptr + 456, true),
      vramOffset:      mem.getUint32(ptr + 460, true),
      audioBuffer:     mem.getUint32(ptr + 464, true),
      rBits:           mem.getUint32(ptr + 468, true),
      rShift:          mem.getUint32(ptr + 472, true),
      gBits:           mem.getUint32(ptr + 476, true),
      gShift:          mem.getUint32(ptr + 480, true),
      bBits:           mem.getUint32(ptr + 484, true),
      bShift:          mem.getUint32(ptr + 488, true),
      aBits:           mem.getUint32(ptr + 492, true),
      aShift:          mem.getUint32(ptr + 496, true),
      xBits:           mem.getUint32(ptr + 500, true),
      xShift:          mem.getUint32(ptr + 504, true),
    };

    // Derive BPP from channel info
    s.bpp = computeBpp(s);

    // If no channel bits set, fill in defaults based on computed BPP
    if (!s.rBits && !s.gBits && !s.bBits && !s.aBits && !s.xBits) {
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
    // title starts at offset +12
    const u8 = new Uint8Array(wasmMemory.buffer, statePtr + 12, TITLE_MAX);
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

  const FMT_GENERIC     = 0;
  const FMT_RGBA8888_LE = 1;
  const FMT_BGRA8888_LE = 2;
  const FMT_RGBX8888_LE = 3;
  const FMT_BGRX8888_LE = 4;
  const FMT_RGB888      = 5;
  const FMT_BGR888      = 6;
  const FMT_RGB565      = 7;
  const FMT_BGR565      = 8;
  const FMT_RGB555      = 9;
  const FMT_BGR555      = 10;
  const FMT_RGB444      = 11;
  const FMT_RGBA4444    = 12;
  const FMT_ARGB4444    = 13;
  const FMT_RGB333      = 14;
  const FMT_RGB332      = 15;
  const FMT_RGB222      = 16;
  const FMT_RGBA2222    = 17;
  const FMT_RGB111      = 18;
  const FMT_GRAY8       = 19;
  const FMT_RGB666      = 20;
  const FMT_MONO1       = 21;
  const FMT_MONO2       = 22;
  const FMT_MONO4       = 23;

  function detectPixelFormat(bpp, rb, rs, gb, gs, bb, bs, ab, ash) {
    if (bpp === 1) return FMT_MONO1;
    if (bpp === 2) return FMT_MONO2;
    if (bpp === 4) return FMT_MONO4;

    if (bpp === 32) {
      if (rb === 8 && rs === 0  && gb === 8 && gs === 8 && bb === 8 && bs === 16 && ab === 8 && ash === 24) return FMT_RGBA8888_LE;
      if (rb === 8 && rs === 16 && gb === 8 && gs === 8 && bb === 8 && bs === 0  && ab === 8 && ash === 24) return FMT_BGRA8888_LE;
      if (rb === 8 && rs === 0  && gb === 8 && gs === 8 && bb === 8 && bs === 16 && ab === 0) return FMT_RGBX8888_LE;
      if (rb === 8 && rs === 16 && gb === 8 && gs === 8 && bb === 8 && bs === 0  && ab === 0) return FMT_BGRX8888_LE;
    }

    if (bpp === 24) {
      if (rb === 8 && rs === 0  && gb === 8 && gs === 8 && bb === 8 && bs === 16) return FMT_RGB888;
      if (rb === 8 && rs === 16 && gb === 8 && gs === 8 && bb === 8 && bs === 0)  return FMT_BGR888;
    }

    if (bpp === 16) {
      if (rb === 5 && rs === 11 && gb === 6 && gs === 5  && bb === 5 && bs === 0  && ab === 0) return FMT_RGB565;
      if (rb === 5 && rs === 0  && gb === 6 && gs === 5  && bb === 5 && bs === 11 && ab === 0) return FMT_BGR565;
      if (rb === 5 && rs === 10 && gb === 5 && gs === 5  && bb === 5 && bs === 0  && (ab === 0 || ab === 1)) return FMT_RGB555;
      if (rb === 5 && rs === 0  && gb === 5 && gs === 5  && bb === 5 && bs === 10 && (ab === 0 || ab === 1)) return FMT_BGR555;
      if (rb === 4 && rs === 8  && gb === 4 && gs === 4  && bb === 4 && bs === 0  && ab === 0) return FMT_RGB444;
      if (rb === 4 && rs === 12 && gb === 4 && gs === 8  && bb === 4 && bs === 4  && ab === 4 && ash === 0) return FMT_RGBA4444;
      if (rb === 4 && rs === 8  && gb === 4 && gs === 4  && bb === 4 && bs === 0  && ab === 4 && ash === 12) return FMT_ARGB4444;
      if (rb === 3 && rs === 6  && gb === 3 && gs === 3  && bb === 3 && bs === 0  && ab === 0) return FMT_RGB333;
    }

    if (bpp === 8) {
      if (rb === 3 && rs === 5 && gb === 3 && gs === 2 && bb === 2 && bs === 0 && ab === 0) return FMT_RGB332;
      if (rb === 2 && rs === 4 && gb === 2 && gs === 2 && bb === 2 && bs === 0 && ab === 0) return FMT_RGB222;
      if (rb === 2 && rs === 6 && gb === 2 && gs === 4 && bb === 2 && bs === 2 && ab === 2 && ash === 0) return FMT_RGBA2222;
      if (rb === 1 && rs === 2 && gb === 1 && gs === 1 && bb === 1 && bs === 0 && ab === 0) return FMT_RGB111;
      if (rb === 0 && gb === 0 && bb === 0) return FMT_GRAY8;
    }

    if ((bpp === 24 || bpp === 32) && rb === 6 && rs === 12 && gb === 6 && gs === 6 && bb === 6 && bs === 0 && ab === 0) {
      return FMT_RGB666;
    }

    return FMT_GENERIC;
  }

  function unpackPixelsToImageData(w, h, bpp, vramPtr, pixels, u32, cx, cy, cw, ch, rb, rs, gb, gs, bb, bs, ab, ash, isGrayscale) {
    const fmt = detectPixelFormat(bpp, rb, rs, gb, gs, bb, bs, ab, ash);
    const buf = wasmMemory.buffer;
    const align32 = (vramPtr % 4 === 0);
    const align16 = (vramPtr % 2 === 0);

    if (fmt === FMT_RGBA8888_LE) {
      if (align32) {
        const v32 = new Uint32Array(buf, vramPtr, w * h);
        for (let row = 0; row < ch; row++) {
          const srcOff = (cy + row) * w + cx;
          const dstOff = row * cw;
          u32.set(v32.subarray(srcOff, srcOff + cw), dstOff);
        }
      } else {
        const v8 = new Uint8Array(buf, vramPtr);
        for (let row = 0; row < ch; row++) {
          const dstOff = row * cw;
          let srcIdx = ((cy + row) * w + cx) * 4;
          for (let col = 0; col < cw; col++) {
            u32[dstOff + col] = v8[srcIdx] | (v8[srcIdx+1] << 8) | (v8[srcIdx+2] << 16) | (v8[srcIdx+3] << 24);
            srcIdx += 4;
          }
        }
      }
      return;
    }

    if (fmt === FMT_BGRA8888_LE) {
      const v32 = align32 ? new Uint32Array(buf, vramPtr, w * h) : null;
      const v8 = !align32 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align32 ? v32[srcRow + col] : (v8[(srcRow + col)*4] | (v8[(srcRow + col)*4+1] << 8) | (v8[(srcRow + col)*4+2] << 16) | (v8[(srcRow + col)*4+3] << 24));
          u32[dstOff + col] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >>> 16) | ((px & 0x000000FF) << 16);
        }
      }
      return;
    }

    if (fmt === FMT_RGBX8888_LE) {
      const v32 = align32 ? new Uint32Array(buf, vramPtr, w * h) : null;
      const v8 = !align32 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align32 ? v32[srcRow + col] : (v8[(srcRow + col)*4] | (v8[(srcRow + col)*4+1] << 8) | (v8[(srcRow + col)*4+2] << 16));
          u32[dstOff + col] = 0xFF000000 | (px & 0x00FFFFFF);
        }
      }
      return;
    }

    if (fmt === FMT_BGRX8888_LE) {
      const v32 = align32 ? new Uint32Array(buf, vramPtr, w * h) : null;
      const v8 = !align32 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align32 ? v32[srcRow + col] : (v8[(srcRow + col)*4] | (v8[(srcRow + col)*4+1] << 8) | (v8[(srcRow + col)*4+2] << 16));
          u32[dstOff + col] = 0xFF000000 | (px & 0x0000FF00) | ((px & 0x00FF0000) >>> 16) | ((px & 0x000000FF) << 16);
        }
      }
      return;
    }

    if (fmt === FMT_RGB888) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        let srcIdx = ((cy + row) * w + cx) * 3;
        for (let col = 0; col < cw; col++) {
          u32[dstOff + col] = 0xFF000000 | (v8[srcIdx + 2] << 16) | (v8[srcIdx + 1] << 8) | v8[srcIdx];
          srcIdx += 3;
        }
      }
      return;
    }

    if (fmt === FMT_BGR888) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        let srcIdx = ((cy + row) * w + cx) * 3;
        for (let col = 0; col < cw; col++) {
          u32[dstOff + col] = 0xFF000000 | (v8[srcIdx] << 16) | (v8[srcIdx + 1] << 8) | v8[srcIdx + 2];
          srcIdx += 3;
        }
      }
      return;
    }

    if (fmt === FMT_RGB565) {
      const v16 = align16 ? new Uint16Array(buf, vramPtr, w * h) : null;
      const v8 = !align16 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align16 ? v16[srcRow + col] : (v8[(srcRow + col)*2] | (v8[(srcRow + col)*2+1] << 8));
          let r = (px >>> 11) & 0x1F; r = (r << 3) | (r >>> 2);
          let g = (px >>> 5)  & 0x3F; g = (g << 2) | (g >>> 4);
          let b = px & 0x1F;        b = (b << 3) | (b >>> 2);
          u32[dstOff + col] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_BGR565) {
      const v16 = align16 ? new Uint16Array(buf, vramPtr, w * h) : null;
      const v8 = !align16 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align16 ? v16[srcRow + col] : (v8[(srcRow + col)*2] | (v8[(srcRow + col)*2+1] << 8));
          let b = (px >>> 11) & 0x1F; b = (b << 3) | (b >>> 2);
          let g = (px >>> 5)  & 0x3F; g = (g << 2) | (g >>> 4);
          let r = px & 0x1F;        r = (r << 3) | (r >>> 2);
          u32[dstOff + col] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_RGB555) {
      const v16 = align16 ? new Uint16Array(buf, vramPtr, w * h) : null;
      const v8 = !align16 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align16 ? v16[srcRow + col] : (v8[(srcRow + col)*2] | (v8[(srcRow + col)*2+1] << 8));
          let r = (px >>> 10) & 0x1F; r = (r << 3) | (r >>> 2);
          let g = (px >>> 5)  & 0x1F; g = (g << 3) | (g >>> 2);
          let b = px & 0x1F;        b = (b << 3) | (b >>> 2);
          let a = (ab === 1 && !(px & (1 << ash))) ? 0 : 255;
          u32[dstOff + col] = (a << 24) | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_BGR555) {
      const v16 = align16 ? new Uint16Array(buf, vramPtr, w * h) : null;
      const v8 = !align16 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align16 ? v16[srcRow + col] : (v8[(srcRow + col)*2] | (v8[(srcRow + col)*2+1] << 8));
          let b = (px >>> 10) & 0x1F; b = (b << 3) | (b >>> 2);
          let g = (px >>> 5)  & 0x1F; g = (g << 3) | (g >>> 2);
          let r = px & 0x1F;        r = (r << 3) | (r >>> 2);
          let a = (ab === 1 && !(px & (1 << ash))) ? 0 : 255;
          u32[dstOff + col] = (a << 24) | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_RGB444) {
      const v16 = align16 ? new Uint16Array(buf, vramPtr, w * h) : null;
      const v8 = !align16 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align16 ? v16[srcRow + col] : (v8[(srcRow + col)*2] | (v8[(srcRow + col)*2+1] << 8));
          let r = (px >>> 8) & 0x0F; r = (r << 4) | r;
          let g = (px >>> 4) & 0x0F; g = (g << 4) | g;
          let b = px & 0x0F;        b = (b << 4) | b;
          u32[dstOff + col] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_RGBA4444) {
      const v16 = align16 ? new Uint16Array(buf, vramPtr, w * h) : null;
      const v8 = !align16 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align16 ? v16[srcRow + col] : (v8[(srcRow + col)*2] | (v8[(srcRow + col)*2+1] << 8));
          let r = (px >>> 12) & 0x0F; r = (r << 4) | r;
          let g = (px >>> 8)  & 0x0F; g = (g << 4) | g;
          let b = (px >>> 4)  & 0x0F; b = (b << 4) | b;
          let a = px & 0x0F;         a = (a << 4) | a;
          u32[dstOff + col] = (a << 24) | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_ARGB4444) {
      const v16 = align16 ? new Uint16Array(buf, vramPtr, w * h) : null;
      const v8 = !align16 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align16 ? v16[srcRow + col] : (v8[(srcRow + col)*2] | (v8[(srcRow + col)*2+1] << 8));
          let a = (px >>> 12) & 0x0F; a = (a << 4) | a;
          let r = (px >>> 8)  & 0x0F; r = (r << 4) | r;
          let g = (px >>> 4)  & 0x0F; g = (g << 4) | g;
          let b = px & 0x0F;         b = (b << 4) | b;
          u32[dstOff + col] = (a << 24) | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_RGB333) {
      const v16 = align16 ? new Uint16Array(buf, vramPtr, w * h) : null;
      const v8 = !align16 ? new Uint8Array(buf, vramPtr) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = align16 ? v16[srcRow + col] : (v8[(srcRow + col)*2] | (v8[(srcRow + col)*2+1] << 8));
          let r = (px >>> 6) & 7; r = (r << 5) | (r << 2) | (r >>> 1);
          let g = (px >>> 3) & 7; g = (g << 5) | (g << 2) | (g >>> 1);
          let b = px & 7;        b = (b << 5) | (b << 2) | (b >>> 1);
          u32[dstOff + col] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_RGB332) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = v8[srcRow + col];
          let r = (px >>> 5) & 7; r = (r << 5) | (r << 2) | (r >>> 1);
          let g = (px >>> 2) & 7; g = (g << 5) | (g << 2) | (g >>> 1);
          let b = px & 3;        b = (b << 6) | (b << 4) | (b << 2) | b;
          u32[dstOff + col] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_RGB222) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = v8[srcRow + col];
          let r = (px >>> 4) & 3; r = (r << 6) | (r << 4) | (r << 2) | r;
          let g = (px >>> 2) & 3; g = (g << 6) | (g << 4) | (g << 2) | g;
          let b = px & 3;        b = (b << 6) | (b << 4) | (b << 2) | b;
          u32[dstOff + col] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_RGBA2222) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = v8[srcRow + col];
          let r = (px >>> 6) & 3; r = (r << 6) | (r << 4) | (r << 2) | r;
          let g = (px >>> 4) & 3; g = (g << 6) | (g << 4) | (g << 2) | g;
          let b = (px >>> 2) & 3; b = (b << 6) | (b << 4) | (b << 2) | b;
          let a = px & 3;        a = (a << 6) | (a << 4) | (a << 2) | a;
          u32[dstOff + col] = (a << 24) | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_RGB111) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const px = v8[srcRow + col];
          let r = (px & 4) ? 255 : 0;
          let g = (px & 2) ? 255 : 0;
          let b = (px & 1) ? 255 : 0;
          u32[dstOff + col] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_GRAY8) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const lum = v8[srcRow + col];
          u32[dstOff + col] = 0xFF000000 | (lum << 16) | (lum << 8) | lum;
        }
      }
      return;
    }

    if (fmt === FMT_RGB666) {
      const v8 = new Uint8Array(buf, vramPtr);
      const v32 = align32 ? new Uint32Array(buf, vramPtr, w * h) : null;
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const idx = srcRow + col;
          const px = (bpp === 32) ? (align32 ? v32[idx] : (v8[idx*4] | (v8[idx*4+1]<<8) | (v8[idx*4+2]<<16))) : (v8[idx*3] | (v8[idx*3+1]<<8) | (v8[idx*3+2]<<16));
          let r = (px >>> 12) & 0x3F; r = (r << 2) | (r >>> 4);
          let g = (px >>> 6)  & 0x3F; g = (g << 2) | (g >>> 4);
          let b = px & 0x3F;         b = (b << 2) | (b >>> 4);
          u32[dstOff + col] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
      }
      return;
    }

    if (fmt === FMT_MONO1) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const idx = srcRow + col;
          const bVal = v8[Math.floor(idx / 8)];
          const bit = (bVal >>> (7 - (idx % 8))) & 1;
          u32[dstOff + col] = bit ? 0xFFFFFFFF : 0xFF000000;
        }
      }
      return;
    }

    if (fmt === FMT_MONO2) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const idx = srcRow + col;
          const bVal = v8[Math.floor(idx / 4)];
          let val = (bVal >>> (6 - (idx % 4) * 2)) & 0x03;
          val = (val << 6) | (val << 4) | (val << 2) | val;
          u32[dstOff + col] = 0xFF000000 | (val << 16) | (val << 8) | val;
        }
      }
      return;
    }

    if (fmt === FMT_MONO4) {
      const v8 = new Uint8Array(buf, vramPtr);
      for (let row = 0; row < ch; row++) {
        const dstOff = row * cw;
        const srcRow = (cy + row) * w + cx;
        for (let col = 0; col < cw; col++) {
          const idx = srcRow + col;
          const bVal = v8[Math.floor(idx / 2)];
          let val = (idx % 2 === 0) ? (bVal >>> 4) : (bVal & 0x0F);
          val = (val << 4) | val;
          u32[dstOff + col] = 0xFF000000 | (val << 16) | (val << 8) | val;
        }
      }
      return;
    }

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

  function renderDirtyRects(state, w, h, bpp, vramPtr, dirtyRectsWasmPtr) {
    // dirtyRectsWasmPtr points to: { uint32 count; Rect rects[32]; }
    // where Rect = { int32 x, y, w, h; } (16 bytes each)
    if (!dirtyRectsWasmPtr) return;
    const mem = getMem();
    const dirtyCount = mem.getUint32(dirtyRectsWasmPtr, true);
    if (dirtyCount === 0) return;

    const isGrayscale = (state.rBits === 0 && state.gBits === 0 && state.bBits === 0 && state.aBits > 0) || 
                        (state.rBits === 0 && state.gBits === 0 && state.bBits === 0 && state.aBits === 0 && bpp < 8);
    let ab = state.aBits > 0 ? state.aBits : (bpp < 8 ? bpp : 0);
    // Rects start at offset +4 in the dirty list struct
    const rectsBase = dirtyRectsWasmPtr + 4;
    const count = Math.min(dirtyCount, MAX_DIRTY_RECTS);

    const isFullScreen = count === 1 &&
      mem.getInt32(rectsBase + 0, true) === 0 &&
      mem.getInt32(rectsBase + 4, true) === 0 &&
      mem.getInt32(rectsBase + 8, true) === w &&
      mem.getInt32(rectsBase + 12, true) === h;

    if (isFullScreen) {
      renderFullFrame(state, w, h, bpp, vramPtr);
      // Clear the pointer so we don't render again next frame
      getMem().setUint32(statePtr + 140, 0, true);
      return;
    }

    for (let r = 0; r < count; r++) {
      const off = rectsBase + r * RECT_STRIDE;
      const rx = mem.getInt32(off + 0, true);
      const ry = mem.getInt32(off + 4, true);
      const rw = mem.getInt32(off + 8, true);
      const rh = mem.getInt32(off + 12, true);

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
    // Clear the dirty rects pointer
    getMem().setUint32(statePtr + 140, 0, true);
  }


  // ── Input ──────────────────────────────────────────────────────────────

  function writeInputToGlobals() {
    if (!statePtr) return;
    const mem = getMem();
    const ptr = statePtr;
    mem.setInt32(ptr + 144, mouseX, true);
    mem.setInt32(ptr + 148, mouseY, true);
    mem.setUint32(ptr + 152, mouseButtons, true);
    mem.setInt32(ptr + 156, mouseWheel, true);

    const keysMem = new Uint8Array(wasmMemory.buffer, ptr + 160, KEYS_COUNT);
    keysMem.set(keysDown);

    mem.setUint32(ptr + 416, gamepadBtns, true);
    mem.setUint32(ptr + 420, (performance.now() - startTime) | 0, true);
  }

  function resetInput() {
    if (!statePtr) return;
    mouseWheel = 0;
    getMem().setInt32(statePtr + 156, 0, true);
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
        memView.setUint32(statePtr + 448, readPtr, true);  // audio_read
        if (underrun) {
          const uOff = statePtr + 452;  // audio_underrun
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

  function frame(now) {
    if (!running) return;

    // Respect target_fps: read it from state if available
    const targetFps = statePtr ? getMem().getUint32(statePtr + 424, true) : 0;
    if (targetFps > 0) {
      const interval = 1000 / targetFps;
      if (now - lastFrameTime < interval - 0.5) {
        requestAnimationFrame(frame);
        return;
      }
    }
    lastFrameTime = now;

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

    // 4. Render dirty rects (via pointer)
    if (g.dirtyRectsPtr) {
      renderDirtyRects(g, w, h, bpp, statePtr + g.vramOffset, g.dirtyRectsPtr);
    }

    // 5. Reset mouse wheel
    resetInput();

    requestAnimationFrame(frame);
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
      lastFrameTime = 0;
      requestAnimationFrame(frame);
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
