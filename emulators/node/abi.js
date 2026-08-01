// Wagnostic ABI Specification & Offsets (ABI.md)
// Guarantee: State struct size is exactly 1024 bytes.

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
  audio_size: 428,         // uint32
  audio_sample_rate: 432,  // uint32
  audio_bpp: 436,          // uint32
  audio_channels: 440,     // uint32
  audio_write: 444,        // uint32
  audio_read: 448,         // uint32
  audio_underrun: 452,     // uint32
  audio_overrun: 456,      // uint32
  audio_chunk_samples: 460,// uint32
  audio_volume: 464,       // uint32
  audio_paused: 468,       // uint32
  vram_offset: 472,        // uint32
  audio_buffer_offset: 476,// uint32
  r_bits: 480, r_shift: 484, // uint32
  g_bits: 488, g_shift: 492, // uint32
  b_bits: 496, b_shift: 500, // uint32
  a_bits: 504, a_shift: 508, // uint32
  x_bits: 512, x_shift: 516, // uint32
  unique: 520,             // int32
};

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

  // Audio getters / setters
  get audioSize() { return this.view.getUint32(OFFSETS.audio_size, true); }
  get audioSampleRate() { return this.view.getUint32(OFFSETS.audio_sample_rate, true); }
  get audioBpp() { return this.view.getUint32(OFFSETS.audio_bpp, true); }
  get audioChannels() { return this.view.getUint32(OFFSETS.audio_channels, true); }
  get audioWrite() { return this.view.getUint32(OFFSETS.audio_write, true); }
  get audioRead() { return this.view.getUint32(OFFSETS.audio_read, true); }
  get audioBufferOffset() { return this.view.getUint32(OFFSETS.audio_buffer_offset, true); }

  setAudioRead(ptr) {
    this.view.setUint32(OFFSETS.audio_read, ptr, true);
  }
}

module.exports = {
  STRUCT_SIZE,
  OFFSETS,
  WagnosticState,
};
