const { Piolho, commTcpExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(commTcpExtension);
  await host.loadRom(path.join(__dirname, '../comm_tcp_test.wasm'), 'client');
  await host.run(10);
}
main();
