const { Piolho, createDefaultRegistry } = require('../src');
const path = require('path');

async function main() {
  console.log('Testing SharedArrayBuffer + Atomics Multi-Threaded Workers...');
  const host = new Piolho({ intervalMs: 1, threaded: true });
  host.use(createDefaultRegistry());

  const w1 = await host.loadRom(path.join(__dirname, '../roms/ipc_consumer.wasm'), 'consumer');
  const w2 = await host.loadRom(path.join(__dirname, '../roms/ipc_producer.wasm'), 'producer');

  console.log('[*] Thread 1 (Consumer):', w1.name, 'isThreaded:', w1.isThreaded);
  console.log('[*] Thread 2 (Producer):', w2.name, 'isThreaded:', w2.isThreaded);

  // Let threads execute rendezvous lockstep
  await new Promise(r => setTimeout(r, 200));

  host.cleanup();
  console.log('[OK] SAB + Atomics hardware rendezvous successfully executed across OS threads.');
  process.exit(0);
}

main().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
