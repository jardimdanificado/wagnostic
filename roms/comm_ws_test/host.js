const { Piolho, commWsExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(commWsExtension);
  await host.loadRom(path.join(__dirname, '../comm_ws_test.wasm'), 'client');
  await host.run(15);
}
main();
