const { Piolho, framebufferExtension, keyboardExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(framebufferExtension).use(keyboardExtension);
  await host.loadRom(path.join(__dirname, '../display_test.wasm'), 'display');
  await host.run(10);
}
main();
