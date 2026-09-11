const { Piolho, framebufferExtension, clockExtension } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(framebufferExtension).use(clockExtension);
  await host.loadRom(path.join(__dirname, '../pathtracer_master.wasm'), 'master');
  await host.loadRom(path.join(__dirname, '../pathtracer_worker.wasm'), 'worker0');
  await host.loadRom(path.join(__dirname, '../pathtracer_worker.wasm'), 'worker1');
  await host.run(5);
}
main();
