/**
 * Wagnostic 2.0 — Universal WebAssembly Host & Multi-ROM Runtime
 */

const { ENV, isTxiki, isNode, isBun, isDeno } = require('./env');
const { WagnosticHost } = require('./host');
const { WWorker } = require('./worker');
const { ExtensionRegistry, defaultRegistry } = require('./extensions');
const { IpcEngine } = require('./ipc');
const { MinimalGifEncoder } = require('./gif');
const { extractFromTar } = require('./tar');

async function createHost(options = {}) {
  return new WagnosticHost(options);
}

module.exports = {
  createHost,
  WagnosticHost,
  WWorker,
  ExtensionRegistry,
  defaultRegistry,
  IpcEngine,
  MinimalGifEncoder,
  extractFromTar,
  ENV,
  isTxiki,
  isNode,
  isBun,
  isDeno
};
