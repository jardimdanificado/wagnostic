const { Piolho } = require('../../src');
const path = require('path');

async function main() {
  const host = new Piolho();
  await host.loadRom(path.join(__dirname, '../ipc_producer.wasm'), 'producer');
  await host.loadRom(path.join(__dirname, '../ipc_consumer.wasm'), 'consumer');
  await host.run(10);
}
main();
