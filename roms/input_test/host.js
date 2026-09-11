const { Piolho, keyboardExtension, mouseExtension, gamepadExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(keyboardExtension).use(mouseExtension).use(gamepadExtension);
  await host.loadRom(path.join(__dirname, '../input_test.wasm'), 'input');
  await host.run(10);
}
main();
