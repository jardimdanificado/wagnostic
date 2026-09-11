const { Piolho, loggerExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(loggerExtension);
  await host.loadRom(path.join(__dirname, '../logger_example.wasm'), 'logger');
  await host.run(10);
}
main();
