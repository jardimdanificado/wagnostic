const { Piolho, createDefaultRegistry } = require('../../src');
const path = require('path');

async function runPipeline(isThreaded, durationSec = 3) {
  const host = new Piolho({ intervalMs: 0, threaded: isThreaded });
  host.use(createDefaultRegistry());

  let totalMsgs = 0;
  if (!isThreaded) {
    host.ipc.onMessage = () => { totalMsgs++; };
  }

  await host.loadRom(path.join(__dirname, 'stage1.wasm'), 'stage1');
  await host.loadRom(path.join(__dirname, 'stage2.wasm'), 'stage2');
  await host.loadRom(path.join(__dirname, 'stage3.wasm'), 'stage3');
  await host.loadRom(path.join(__dirname, 'stage4.wasm'), 'stage4');

  const startSabMsgs = isThreaded ? host.arena.totalMessages : 0;
  const startTime = Date.now();

  const runPromise = host.run({ steps: 0 });
  await new Promise(r => setTimeout(r, durationSec * 1000));

  const elapsedSec = (Date.now() - startTime) / 1000;
  const finalMsgs = isThreaded ? (host.arena.totalMessages - startSabMsgs) : totalMsgs;

  host.cleanup();
  await runPromise;

  return {
    mode: isThreaded ? 'Multi-Threaded (4 Parallel OS Threads / 4 CPU Cores)' : 'Single-Threaded (1 Core / Sequential Loop)',
    totalMsgs: finalMsgs,
    elapsedSec,
    msgsPerSec: Math.round(finalMsgs / elapsedSec)
  };
}

async function main() {
  console.log('================================================================');
  console.log(' PIOLHO — REALISTIC 4-STAGE PIPELINE BENCHMARK');
  console.log(' Topology : 4 Workers (Stage 1 -> Stage 2 -> Stage 3 -> Stage 4)');
  console.log(' Workload : 30,000 Hashing/Math cycles per step + 128B Rendezvous');
  console.log('================================================================');

  const duration = 2;

  console.log(`[*] [1/2] Running Single-Threaded (1 Core) for ${duration}s...`);
  const single = await runPipeline(false, duration);

  console.log(`[*] [2/2] Running Multi-Threaded (4 Cores Parallel) for ${duration}s...`);
  const multi = await runPipeline(true, duration);

  console.log('----------------------------------------------------------------');
  console.log(' BENCHMARK COMPARISON:');
  console.log('----------------------------------------------------------------');
  console.log(` [1] ${single.mode}:`);
  console.log(`     - Processed Messages : ${single.totalMsgs.toLocaleString()} msgs in ${single.elapsedSec.toFixed(2)}s`);
  console.log(`     - Pipeline Rate      : ${single.msgsPerSec.toLocaleString()} msgs/sec`);
  console.log('');
  console.log(` [2] ${multi.mode}:`);
  console.log(`     - Processed Messages : ${multi.totalMsgs.toLocaleString()} msgs in ${multi.elapsedSec.toFixed(2)}s`);
  console.log(`     - Pipeline Rate      : ${multi.msgsPerSec.toLocaleString()} msgs/sec`);
  console.log('----------------------------------------------------------------');
  
  const speedup = (multi.msgsPerSec / (single.msgsPerSec || 1)).toFixed(2);
  console.log(` PARALLEL SPEEDUP FACTOR: ${speedup}x`);
  console.log('================================================================');
  process.exit(0);
}

main().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
