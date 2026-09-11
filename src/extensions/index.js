/**
 * Piolho Extensions Index & Default Registry
 */

const { ExtensionRegistry } = require('./registry');
const { clockExtension } = require('./clock');
const { loggerExtension } = require('./logger');
const { commTcpExtension } = require('./comm_tcp');
const { commPipeExtension } = require('./comm_pipe');
const { commWsExtension } = require('./comm_ws');
const { commUdpExtension } = require('./comm_udp');
const { commWorkersExtension } = require('./comm_workers');
const { commStdioExtension } = require('./comm_stdio');
const { commBroadcastExtension } = require('./comm_broadcast');
const { commWebrtcExtension } = require('./comm_webrtc');
const { commWebtransportExtension } = require('./comm_webtransport');
const { commSerialExtension } = require('./comm_serial');
const { commBluetoothExtension } = require('./comm_bluetooth');
const { commHttpExtension } = require('./comm_http');
const { commShmExtension } = require('./comm_shm');

function createDefaultRegistry() {
  const registry = new ExtensionRegistry();
  registry.register(clockExtension);
  registry.register(loggerExtension);
  registry.register(commTcpExtension);
  registry.register(commPipeExtension);
  registry.register(commWsExtension);
  registry.register(commUdpExtension);
  registry.register(commWorkersExtension);
  registry.register(commStdioExtension);
  registry.register(commBroadcastExtension);
  registry.register(commWebrtcExtension);
  registry.register(commWebtransportExtension);
  registry.register(commSerialExtension);
  registry.register(commBluetoothExtension);
  registry.register(commHttpExtension);
  registry.register(commShmExtension);
  return registry;
}

const defaultRegistry = createDefaultRegistry();

module.exports = {
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
};
