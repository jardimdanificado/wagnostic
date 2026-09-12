const { Piolho, createDefaultRegistry } = require('../../src');
const path = require('path');

async function benchmarkSingleThread(durationSec = 2) {
  const host = new Piolho({ intervalMs: 0, threaded: false });
  host.use(createDefaultRegistry());

  let totalMsgs = 0;
  host.ipc.onMessage = () => { totalMsgs++; };

  await host.loadRom(path.join(__dirname, 'consumer.wasm'), 'consumer');
  await host.loadRom(path.join(__dirname, 'producer.wasm'), 'producer');

  host.isRunning = true;
  const startTime = Date.now();
  while (Date.now() - startTime < durationSec * 1000) {
    host.step();
  }
  const elapsedSec = (Date.now() - startTime) / 1000;
  host.cleanup();

  return {
    mode: 'Single-Thread In-Process (1 CPU Core / Direct Linear Memory Pointer)',
    totalMsgs,
    elapsedSec,
    msgsPerSec: Math.round(totalMsgs / elapsedSec)
  };
}

async function benchmarkMultiThread(durationSec = 2) {
  const host = new Piolho({ intervalMs: 0, threaded: true });
  host.use(createDefaultRegistry());

  await host.loadRom(path.join(__dirname, 'consumer.wasm'), 'consumer');
  await host.loadRom(path.join(__dirname, 'producer.wasm'), 'producer');

  const startMsgs = host.arena.totalMessages;
  const startTime = Date.now();
  await new Promise(r => setTimeout(r, durationSec * 1000));
  const elapsedSec = (Date.now() - startTime) / 1000;
  const totalMsgs = host.arena.totalMessages - startMsgs;
  host.cleanup();

  return {
    mode: 'Multi-Thread True Parallel (2 CPU Cores / SAB + Atomics Futex Sync)',
    totalMsgs,
    elapsedSec,
    msgsPerSec: Math.round(totalMsgs / elapsedSec)
  };
}

async function main() {
  console.log('================================================================');
  console.log(' PIOLHO — FAIR APPLES-TO-APPLES RENDEZVOUS BENCHMARK');
  console.log(' (Exact same WASM Producer/Consumer ROMs tested on both engines)');
  console.log('================================================================');

  const duration = 2;
  console.log(`[*] Testing [1] Single-Thread In-Process for ${duration}s...`);
  const r1 = await benchmarkSingleThread(duration);

  console.log(`[*] Testing [2] Multi-Thread SAB+Atomics for ${duration}s...`);
  const r2 = await benchmarkMultiThread(duration);

  console.log('----------------------------------------------------------------');
  console.log(' BENCHMARK COMPARISON RESULTS:');
  console.log('----------------------------------------------------------------');
  console.log(` [1] ${r1.mode}:`);
  console.log(`     - Total Messages : ${r1.totalMsgs.toLocaleString()} msgs in ${r1.elapsedSec.toFixed(2)}s`);
  console.log(`     - Throughput     : ${r1.msgsPerSec.toLocaleString()} msgs/sec`);
  console.log(`     - Latency/Msg    : ${(1000000 / r1.msgsPerSec).toFixed(2)} µs (in-place memory pointer swap)`);
  console.log('');
  console.log(` [2] ${r2.mode}:`);
  console.log(`     - Total Messages : ${r2.totalMsgs.toLocaleString()} msgs in ${r2.elapsedSec.toFixed(2)}s`);
  console.log(`     - Throughput     : ${r2.msgsPerSec.toLocaleString()} msgs/sec`);
  console.log(`     - Latency/Msg    : ${(1000000 / r2.msgsPerSec).toFixed(2)} µs (inter-core hardware futex interrupt)`);
  console.log('----------------------------------------------------------------');
  console.log(' ANALYSIS & TRADE-OFF:');
  console.log('  * Single-Thread: Máxima taxa para mensagens leves no mesmo núcleo (sem context switch de SO).');
  console.log('  * Multi-Thread : Paralelismo real onde código WASM pesado em CPU roda sem travar outros núcleos.');
  console.log('================================================================');
  process.exit(0);
}

main().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
