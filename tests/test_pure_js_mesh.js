const { piolho, OK, DONE, ANY } = require('../src');

async function main() {
  console.log('================================================================');
  console.log(' PIOLHO — PURE JAVASCRIPT MESH TEST');
  console.log(' Trinity: update(), say(), listen()');
  console.log('================================================================');

  const mesh = piolho.create({ intervalMs: 1 });

  let receivedCount = 0;
  let lastPayload = null;

  // 1. Define Sensor Node
  mesh.add('sensor', {
    update({ say, step }) {
      if (step < 5) {
        const payload = { seq: step + 1, temp: 24.0 + step * 0.5, status: 'OK' };
        say('display', payload, 5);
      } else {
        return DONE; // Finished
      }
      return OK;
    }
  });

  // 2. Define Display Node
  mesh.add('display', {
    update({ listen, step }) {
      const data = listen('sensor', 5);
      if (data) {
        receivedCount++;
        lastPayload = data;
        console.log(`[*] [Display] Received reading #${data.seq}:`, data);
      }
      if (step >= 5) return DONE;
      return OK;
    }
  });

  await mesh.run();

  console.log('----------------------------------------------------------------');
  console.log(' TEST SUMMARY:');
  console.log('----------------------------------------------------------------');
  console.log(`  Total Messages Received : ${receivedCount}`);
  console.log(`  Last Payload Verified   :`, lastPayload);
  
  if (receivedCount === 5 && lastPayload.seq === 5) {
    console.log('  Status                  : [  OK  ] 100% Verified Pure JS Mesh!');
    process.exit(0);
  } else {
    console.error('  Status                  : [FAILED]');
    process.exit(1);
  }
}

main().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
