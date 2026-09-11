const { Piolho, commBroadcastExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(commBroadcastExtension);
  await host.loadRom(path.join(__dirname, '../comm_broadcast_test.wasm'), 'broadcast_runner');
  await host.run(5);
}
main();
