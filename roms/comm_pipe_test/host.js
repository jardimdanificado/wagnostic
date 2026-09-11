const { Piolho, commPipeExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(commPipeExtension);
  await host.loadRom(path.join(__dirname, '../comm_pipe_test.wasm'), 'client');
  await host.run(10);
}
main();
