const { Piolho, commUdpExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(commUdpExtension);
  await host.loadRom(path.join(__dirname, '../comm_udp_test.wasm'), 'client');
  await host.run(10);
}
main();
