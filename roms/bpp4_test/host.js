const { Piolho, framebufferExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(framebufferExtension);
  await host.loadRom(path.join(__dirname, '../bpp4_test.wasm'), 'bpp4');
  await host.run(10);
}
main();
