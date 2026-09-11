const { Piolho, commStdioExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(commStdioExtension);
  await host.loadRom(path.join(__dirname, '../comm_stdio_test.wasm'), 'stdio_runner');
  await host.run(5);
}
main();
