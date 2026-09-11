const { Piolho, framebufferExtension, clockExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(framebufferExtension).use(clockExtension);
  await host.loadRom(path.join(__dirname, '../use_test.wasm'), 'use_test');
  await host.run(10);
}
main();
