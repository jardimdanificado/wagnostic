/**
 * Wagnostic — Standard Extensions Handler (`std:*`)
 * 
 * - std:framebuffer (12B struct: width, height, pixels)
 * - std:clock       (24B struct: ticks, frequency, delta)
 * - std:keyboard    (256B struct: keys)
 * - std:mouse       (20B struct: x, y, buttons, wheel_x, wheel_y)
 * - logger          (12B struct: buffer, capacity, length)
 */

export const KEY_MAP = {
  KeyA: 0x04, KeyB: 0x05, KeyC: 0x06, KeyD: 0x07, KeyE: 0x08, KeyF: 0x09, KeyG: 0x0A,
  KeyH: 0x0B, KeyI: 0x0C, KeyJ: 0x0D, KeyK: 0x0E, KeyL: 0x0F, KeyM: 0x10, KeyN: 0x11,
  KeyO: 0x12, KeyP: 0x13, KeyQ: 0x14, KeyR: 0x15, KeyS: 0x16, KeyT: 0x17, KeyU: 0x18,
  KeyV: 0x19, KeyW: 0x1A, KeyX: 0x1B, KeyY: 0x1C, KeyZ: 0x1D,
  Digit1: 0x1E, Digit2: 0x1F, Digit3: 0x20, Digit4: 0x21, Digit5: 0x22,
  Digit6: 0x23, Digit7: 0x24, Digit8: 0x25, Digit9: 0x26, Digit0: 0x27,
  Enter: 0x28, Escape: 0x29, Backspace: 0x2A, Tab: 0x2B, Space: 0x2C,
  Minus: 0x2D, Equal: 0x2E, BracketLeft: 0x2F, BracketRight: 0x30, Backslash: 0x31,
  Semicolon: 0x33, Quote: 0x34, Backquote: 0x35, Comma: 0x36, Period: 0x37, Slash: 0x38,
  CapsLock: 0x39, F1: 0x3A, F2: 0x3B, F3: 0x3C, F4: 0x3D, F5: 0x3E, F6: 0x3F,
  F7: 0x40, F8: 0x41, F9: 0x42, F10: 0x43, F11: 0x44, F12: 0x45,
  PrintScreen: 0x46, ScrollLock: 0x47, Pause: 0x48, Insert: 0x49, Home: 0x4A, PageUp: 0x4B,
  Delete: 0x4C, End: 0x4D, PageDown: 0x4E, ArrowRight: 0x4F, ArrowLeft: 0x50, ArrowDown: 0x51, ArrowUp: 0x52,
  ControlLeft: 0xE0, ShiftLeft: 0xE1, AltLeft: 0xE2, MetaLeft: 0xE3,
  ControlRight: 0xE4, ShiftRight: 0xE5, AltRight: 0xE6, MetaRight: 0xE7
};

// Module-level standard extension pointers & state
let fbPtr = 0, defaultFbPtr = 0;
let clockPtr = 0;
let keyboardPtr = 0;
let mousePtr = 0;
let loggerPtr = 0, loggerBufPtr = 0;

export const keyState = new Uint8Array(256);
export let mouseX = 0, mouseY = 0, mouseButtons = 0, mouseWheelX = 0, mouseWheelY = 0;
let startTime = Date.now(), lastTime = startTime;
let onLogCallback = (msg) => console.log(`[ROM Log] ${msg}`);
let targetFps = 60;

export function setLogHandler(fn) {
  onLogCallback = fn;
}

export function setTargetFps(fps) {
  targetFps = fps || 60;
}

export function setKey(scancode, pressed) {
  keyState[scancode] = pressed ? 1 : 0;
}

export function setMouse(x, y, buttons) {
  if (x !== undefined) mouseX = x;
  if (y !== undefined) mouseY = y;
  if (buttons !== undefined) mouseButtons = buttons;
}

export function resetStd() {
  fbPtr = 0;
  clockPtr = 0;
  keyboardPtr = 0;
  mousePtr = 0;
  loggerPtr = 0;
  keyState.fill(0);
  mouseX = 0;
  mouseY = 0;
  mouseButtons = 0;
  mouseWheelX = 0;
  mouseWheelY = 0;
  startTime = Date.now();
  lastTime = startTime;
}

/**
 * Standard Wagnostic extension dispatcher.
 * Pass this function directly to `new Wagnostic(handleExtension)`.
 */
export function handleExtension(name, host) {
  // 1. Framebuffer: std:framebuffer
  if (name === 'std:framebuffer') {
    if (!fbPtr) {
      fbPtr = host.hostAlloc(12, 4);
      defaultFbPtr = host.hostAlloc(640 * 480 * 4, 4);
      const view = new DataView(host.memory.buffer, fbPtr, 12);
      view.setUint32(0, 320, true);
      view.setUint32(4, 240, true);
      view.setUint32(8, defaultFbPtr, true);
    }
    return fbPtr;
  }

  // 2. Clock: std:clock
  if (name === 'std:clock') {
    if (!clockPtr) {
      clockPtr = host.hostAlloc(24, 8);
      const view = new DataView(host.memory.buffer, clockPtr, 24);
      view.setBigUint64(0, 0n, true);
      view.setBigUint64(8, 1000n, true);
      view.setFloat32(16, 1.0 / targetFps, true);
    }
    return clockPtr;
  }

  // 3. Keyboard: std:keyboard
  if (name === 'std:keyboard') {
    if (!keyboardPtr) keyboardPtr = host.hostAlloc(256, 4);
    return keyboardPtr;
  }

  // 4. Mouse: std:mouse
  if (name === 'std:mouse') {
    if (!mousePtr) mousePtr = host.hostAlloc(20, 4);
    return mousePtr;
  }

  // 5. Logger: logger
  if (name === 'logger') {
    if (!loggerPtr) {
      loggerPtr = host.hostAlloc(12, 4);
      loggerBufPtr = host.hostAlloc(1024, 4);
      const view = new DataView(host.memory.buffer, loggerPtr, 12);
      view.setUint32(0, loggerBufPtr, true);
      view.setUint32(4, 1024, true);
      view.setUint32(8, 0, true);
    }
    return loggerPtr;
  }

  return 0; // Unknown/unsupported extension
}

/**
 * Updates clock, inputs, and processes pending log messages before update().
 */
export function updateStd(host) {
  if (!host || !host.memory) return;
  const now = Date.now();
  const dt = (now - lastTime) / 1000.0;
  lastTime = now;

  if (clockPtr && clockPtr + 24 <= host.memory.buffer.byteLength) {
    const view = new DataView(host.memory.buffer, clockPtr, 24);
    view.setBigUint64(0, BigInt(now - startTime), true);
    view.setFloat32(16, dt, true);
  }

  if (keyboardPtr && keyboardPtr + 256 <= host.memory.buffer.byteLength) {
    new Uint8Array(host.memory.buffer, keyboardPtr, 256).set(keyState);
  }

  if (mousePtr && mousePtr + 20 <= host.memory.buffer.byteLength) {
    const view = new DataView(host.memory.buffer, mousePtr, 20);
    view.setInt32(0, mouseX, true);
    view.setInt32(4, mouseY, true);
    view.setUint32(8, mouseButtons, true);
    view.setInt32(12, mouseWheelX, true);
    view.setInt32(16, mouseWheelY, true);
    mouseWheelX = 0;
    mouseWheelY = 0;
  }

  if (loggerPtr && loggerPtr + 12 <= host.memory.buffer.byteLength) {
    const view = new DataView(host.memory.buffer, loggerPtr, 12);
    const len = view.getUint32(8, true);
    if (len > 0 && loggerBufPtr && loggerBufPtr + len <= host.memory.buffer.byteLength) {
      const textBytes = new Uint8Array(host.memory.buffer, loggerBufPtr, len);
      onLogCallback(new TextDecoder().decode(textBytes));
      view.setUint32(8, 0, true);
    }
  }
}

/**
 * Retrieves current framebuffer dimensions and pixels from WASM memory.
 */
export function getFramebuffer(host) {
  if (!host || !host.memory || !fbPtr || fbPtr + 12 > host.memory.buffer.byteLength) return null;
  const view = new DataView(host.memory.buffer, fbPtr, 12);
  const width = view.getUint32(0, true) || 320;
  const height = view.getUint32(4, true) || 240;
  const pixelsPtr = view.getUint32(8, true);
  if (!pixelsPtr || pixelsPtr + width * height * 4 > host.memory.buffer.byteLength) return null;
  const pixels = new Uint32Array(host.memory.buffer, pixelsPtr, width * height);
  return { width, height, pixels, pixelsPtr };
}
