/**
 * Wagnostic Extension Registry
 * 
 * Allows registering built-in and third-party host extensions.
 * Extensions receive `(worker, host, extName)` and return an i32 pointer (or 0).
 */

class ExtensionRegistry {
  constructor() {
    this.handlers = new Map();
    this.registerDefaults();
  }

  register(name, handler) {
    this.handlers.set(name, handler);
  }

  get(name) {
    return this.handlers.get(name);
  }

  has(name) {
    return this.handlers.has(name);
  }

  dispatch(worker, host, name) {
    const handler = this.handlers.get(name);
    if (!handler) return 0;
    return handler(worker, host, name) || 0;
  }

  registerDefaults() {
    // 1. Framebuffer: std:framebuffer / std:surface
    const fbHandler = (worker) => {
      if (!worker.fbPtr) {
        worker.fbPtr = worker.alloc(12, 4);
        worker.defaultFbPtr = worker.alloc(640 * 480 * 4, 4);
        const view = new DataView(worker.memory.buffer, worker.fbPtr, 12);
        view.setUint32(0, 320, true);
        view.setUint32(4, 240, true);
        view.setUint32(8, worker.defaultFbPtr, true);
      }
      return worker.fbPtr;
    };
    this.register('std:framebuffer', fbHandler);
    this.register('framebuffer', fbHandler);
    this.register('std:surface', fbHandler);
    this.register('surface', fbHandler);

    // 2. Clock: std:clock
    const clockHandler = (worker, host) => {
      if (!worker.clockPtr) {
        worker.clockPtr = worker.alloc(24, 8);
        const view = new DataView(worker.memory.buffer, worker.clockPtr, 24);
        view.setBigUint64(0, 0n, true);
        view.setBigUint64(8, 1000n, true);
        view.setFloat32(16, 1.0 / host.targetFps, true);
      }
      return worker.clockPtr;
    };
    this.register('std:clock', clockHandler);
    this.register('clock', clockHandler);

    // 3. Keyboard: std:keyboard
    const kbHandler = (worker) => {
      if (!worker.keyboardPtr) {
        worker.keyboardPtr = worker.alloc(256, 4);
        new Uint8Array(worker.memory.buffer, worker.keyboardPtr, 256).fill(0);
      }
      return worker.keyboardPtr;
    };
    this.register('std:keyboard', kbHandler);
    this.register('keyboard', kbHandler);

    // 4. Mouse: std:mouse
    const mouseHandler = (worker) => {
      if (!worker.mousePtr) {
        worker.mousePtr = worker.alloc(20, 4);
        new Uint8Array(worker.memory.buffer, worker.mousePtr, 20).fill(0);
      }
      return worker.mousePtr;
    };
    this.register('std:mouse', mouseHandler);
    this.register('mouse', mouseHandler);

    // 5. Gamepad: std:gamepad
    const gpHandler = (worker) => {
      if (!worker.gamepadPtr) {
        worker.gamepadPtr = worker.alloc(20, 4);
        new Uint8Array(worker.memory.buffer, worker.gamepadPtr, 20).fill(0);
      }
      return worker.gamepadPtr;
    };
    this.register('std:gamepad', gpHandler);
    this.register('gamepad', gpHandler);

    // 6. GIF Recording: std:gif
    const gifHandler = (worker, host) => {
      if (!worker.gifPtr) {
        worker.gifPtr = worker.alloc(20, 4);
        const view = new DataView(worker.memory.buffer, worker.gifPtr, 20);
        view.setUint32(0, (host.gifPath || host.maxFrames > 0) ? 1 : 0, true);
        view.setUint32(4, 0, true);
        view.setUint32(8, host.maxFrames, true);
        view.setUint32(12, 2, true);
        view.setUint32(16, 0, true);
      }
      return worker.gifPtr;
    };
    this.register('std:gif', gifHandler);
    this.register('gif', gifHandler);

    // 7. Logger: logger
    const loggerHandler = (worker) => {
      if (!worker.loggerPtr) {
        worker.loggerPtr = worker.alloc(12, 4);
        worker.loggerBufPtr = worker.alloc(1024, 4);
        const view = new DataView(worker.memory.buffer, worker.loggerPtr, 12);
        view.setUint32(0, worker.loggerBufPtr, true);
        view.setUint32(4, 1024, true);
        view.setUint32(8, 0, true);
      }
      return worker.loggerPtr;
    };
    this.register('logger', loggerHandler);
  }
}

const defaultRegistry = new ExtensionRegistry();

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { ExtensionRegistry, defaultRegistry };
}
