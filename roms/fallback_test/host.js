const { Piolho } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  await host.loadRom(path.join(__dirname, '../fallback_test.wasm'), 'fallback');
  await host.run(10);
}
main();
