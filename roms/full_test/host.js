const { 
  Piolho, 
  framebufferExtension, 
  clockExtension, 
  keyboardExtension, 
  mouseExtension, 
  gamepadExtension 
} = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  host.use(framebufferExtension)
      .use(clockExtension)
      .use(keyboardExtension)
      .use(mouseExtension)
      .use(gamepadExtension);
  await host.loadRom(path.join(__dirname, '../full_test.wasm'), 'full_test');
  await host.run(10);
}
main();
