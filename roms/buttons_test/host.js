const { Piolho, gamepadExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(gamepadExtension);
  await host.loadRom(path.join(__dirname, '../buttons_test.wasm'), 'buttons');
  await host.run(10);
}
main();
