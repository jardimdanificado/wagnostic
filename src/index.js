/**
 * Piolho 2.0 — Universal WebAssembly Host & Multi-ROM Runtime
 */

const { ENV, isTxiki, isNode, isBun, isDeno } = require('./env');
const { PiolhoHost } = require('./host');
const { WWorker } = require('./worker');
const {
  ExtensionRegistry,
  createDefaultRegistry,
  defaultRegistry,
  framebufferExtension,
  getFramebuffer,
  clockExtension,
  keyboardExtension,
  mouseExtension,
  gamepadExtension,
  loggerExtension,
  gifExtension
} = require('./extensions');
const { IpcEngine } = require('./ipc');
const { MinimalGifEncoder } = require('./gif');
const { extractFromTar } = require('./tar');

async function createHost(options = {}) {
  return new PiolhoHost(options);
}

module.exports = {
  createHost,
  PiolhoHost,
  WWorker,
  ExtensionRegistry,
  createDefaultRegistry,
  defaultRegistry,
  framebufferExtension,
  getFramebuffer,
  clockExtension,
  keyboardExtension,
  mouseExtension,
  gamepadExtension,
  loggerExtension,
  gifExtension,
  IpcEngine,
  MinimalGifEncoder,
  extractFromTar,
  ENV,
  isTxiki,
  isNode,
  isBun,
  isDeno
};
