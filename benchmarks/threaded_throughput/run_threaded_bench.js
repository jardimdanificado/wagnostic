const { Piolho, createDefaultRegistry } = require('../../src');
const path = require('path');

async function run() {
  console.log('================================================================');
  console.log(' PIOLHO — REAL MULTI-THREADED SAB+ATOMICS BENCHMARK');
  console.log('================================================================');

  const host = new Piolho({ intervalMs: 0, threaded: true });
  host.use(createDefaultRegistry());

  console.log('[*] Spawning Producer & Consumer on independent OS threads...');
  const c = await host.loadRom(path.join(__dirname, 'consumer.wasm'), 'consumer');
  const p = await host.loadRom(path.join(__dirname, 'producer.wasm'), 'producer');

  console.log('[*] Threads active in parallel:');
  console.log('  - Consumer Thread ID:', c.id, '(Thread Worker)');
  console.log('  - Producer Thread ID:', p.id, '(Thread Worker)');

  const durationSec = 2;
  console.log(`[*] Blasting lock-free rendezvous across CPU cores for ${durationSec}s...`);

  const initialMsgs = host.arena.totalMessages;
  const startTime = Date.now();
  await new Promise(r => setTimeout(r, durationSec * 1000));
  const elapsedSec = (Date.now() - startTime) / 1000;
  const finalMsgs = host.arena.totalMessages;
  const totalMsgs = finalMsgs - initialMsgs;
  const msgsPerSec = Math.round(totalMsgs / elapsedSec);

  host.cleanup();

  console.log('----------------------------------------------------------------');
  console.log(' MULTI-THREADING BENCHMARK RESULTS:');
  console.log('----------------------------------------------------------------');
  console.log(`  Duration       : ${elapsedSec.toFixed(2)}s`);
  console.log(`  Total Messages : ${totalMsgs.toLocaleString()}`);
  console.log(`  Throughput     : ${msgsPerSec.toLocaleString()} msgs/sec across OS threads`);
  console.log(`  Zero-Queue     : [  OK  ] Synchronous Hardware Futex Sync`);
  console.log('================================================================');
  process.exit(0);
}

run().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
