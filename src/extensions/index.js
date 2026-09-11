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

function createDefaultRegistry() {
  const registry = new ExtensionRegistry();
  registry.register(clockExtension);
  registry.register(loggerExtension);
  registry.register(commTcpExtension);
  registry.register(commPipeExtension);
  registry.register(commWsExtension);
  registry.register(commUdpExtension);
  registry.register(commWorkersExtension);
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
  commWorkersExtension
};
