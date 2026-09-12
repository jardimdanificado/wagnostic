const { Piolho, createDefaultRegistry } = require('../src');
const path = require('path');

async function run() {
  console.log('Testing Real OS Multi-Threading WebWorkers in Piolho...');
  const host = new Piolho({ intervalMs: 1, threaded: true });
  host.use(createDefaultRegistry());

  let receivedCount = 0;
  host.ipc.onMessage = ({ sender, target, size, op }) => {
    receivedCount++;
  };

  const w1 = await host.loadRom(path.join(__dirname, '../roms/ipc_producer.wasm'), 'producer');
  const w2 = await host.loadRom(path.join(__dirname, '../roms/ipc_consumer.wasm'), 'consumer');

  console.log('Threaded workers instantiated:');
  console.log(' - Worker 1 (Thread):', w1.name, 'isThreaded:', w1.isThreaded);
  console.log(' - Worker 2 (Thread):', w2.name, 'isThreaded:', w2.isThreaded);

  // Run for 300ms
  await new Promise(r => setTimeout(r, 300));

  host.cleanup();
  console.log('Threaded worker test completed. Messages processed:', receivedCount);
  if (w1.isThreaded && w2.isThreaded) {
    console.log('[OK] Real Multi-Threading WebWorkers verified.');
    process.exit(0);
  } else {
    console.error('[FAIL] Workers were not threaded.');
    process.exit(1);
  }
}

run().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
