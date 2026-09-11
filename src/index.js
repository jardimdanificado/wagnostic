/**
 * Piolho 2.0 — Universal WebAssembly Host & Multi-ROM Communication Runtime
 */

const { ENV, isTxiki, isNode, isBun, isDeno } = require('./env');
const { Piolho } = require('./host');
const { WWorker } = require('./worker');
const {
  ExtensionRegistry,
  createDefaultRegistry,
  defaultRegistry,
  clockExtension,
  loggerExtension,
  commTcpExtension,
  commPipeExtension,
  commWsExtension,
  commUdpExtension,
  commWorkersExtension,
  commStdioExtension,
  commBroadcastExtension,
  commWebrtcExtension,
  commWebtransportExtension,
  commSerialExtension,
  commBluetoothExtension,
  commHttpExtension,
  commShmExtension
} = require('./extensions');
const { IpcEngine } = require('./ipc');
const { PeerRegistry } = require('./peer_registry');
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
  clockExtension,
  loggerExtension,
  commTcpExtension,
  commPipeExtension,
  commWsExtension,
  commUdpExtension,
  commWorkersExtension,
  commStdioExtension,
  commBroadcastExtension,
  commWebrtcExtension,
  commWebtransportExtension,
  commSerialExtension,
  commBluetoothExtension,
  commHttpExtension,
  commShmExtension,
  PeerRegistry,
  IpcEngine,
  extractFromTar,
  ENV,
  isTxiki,
  isNode,
  isBun,
  isDeno
};
