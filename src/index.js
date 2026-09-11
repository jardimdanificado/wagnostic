/**
 * Piolho 2.0 — Universal WebAssembly Host & Multi-ROM Runtime
 */

const { ENV, isTxiki, isNode, isBun, isDeno } = require('./env');
const { Piolho } = require('./host');
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
  gifExtension,
  createGifExtension,
  commTcpExtension,
  commPipeExtension,
  commWsExtension,
  commUdpExtension
} = require('./extensions');
const { IpcEngine } = require('./ipc');
const { PeerRegistry } = require('./peer_registry');
const { MinimalGifEncoder } = require('./gif');
const { extractFromTar } = require('./tar');

async function createHost(options = {}) {
  return new Piolho(options);
}

module.exports = {
  createHost,
  Piolho,
  WWorker,
  Worker: WWorker,
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
  createGifExtension,
  commTcpExtension,
  commPipeExtension,
  commWsExtension,
  commUdpExtension,
  PeerRegistry,
  IpcEngine,
  MinimalGifEncoder,
  extractFromTar,
  ENV,
  isTxiki,
  isNode,
  isBun,
  isDeno
};
