const { Piolho } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  await host.loadRom(path.join(__dirname, '../bare_counter.wasm'), 'counter');
  await host.run(10);
}
main();
