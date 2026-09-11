const { Piolho, commWorkersExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(commWorkersExtension);
  await host.loadRom(path.join(__dirname, '../comm_workers_test.wasm'), 'worker_test');
  await host.run(10);
}
main();
