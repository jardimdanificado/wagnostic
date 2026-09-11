const { Piolho, framebufferExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(framebufferExtension);
  await host.loadRom(path.join(__dirname, '../exotic2_test.wasm'), 'exotic2');
  await host.run(10);
}
main();
