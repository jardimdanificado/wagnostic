const { piolho, OK, DONE, ANY } = require('../src');

async function main() {
  console.log('================================================================');
  console.log(' PIOLHO — DISTRIBUTED PURE JS NETWORK MESH TEST (TCP)');
  console.log(' Cross-Process Rendezvous via say() and listen()');
  console.log('================================================================');

  const PORT = 9486;

  // 1. Server Hub Mesh
  const hubMesh = piolho.create({ intervalMs: 2 });
  await hubMesh.net.listenTCP(PORT, 'hub_node');

  let serverReceived = 0;
  hubMesh.add('receiver', {
    update({ listen, step }) {
      const data = listen(ANY, 10);
      if (data) {
        serverReceived++;
        console.log(`[*] [Hub Node] Received network packet #${data.seq}:`, data);
      }
      if (serverReceived >= 3) return DONE;
      return OK;
    }
  });

  const hubPromise = hubMesh.run();

  // 2. Client Mesh
  const clientMesh = piolho.create({ intervalMs: 2 });
  await clientMesh.net.connectTCP('127.0.0.1', PORT, 'client_node');

  let clientSent = 0;
  clientMesh.add('sender', {
    update({ say, step }) {
      if (clientSent < 3) {
        clientSent++;
        say('hub_node', { seq: clientSent, text: `Hello from Client #${clientSent}` }, 10);
      } else if (step > 10) {
        return DONE;
      }
      return OK;
    }
  });

  const clientPromise = clientMesh.run();

  await Promise.all([hubPromise, clientPromise]);

  hubMesh.net.cleanup();
  clientMesh.net.cleanup();

  console.log('----------------------------------------------------------------');
  console.log(' DISTRIBUTED TEST RESULTS:');
  console.log('----------------------------------------------------------------');
  console.log(`  Packets Sent     : ${clientSent}`);
  console.log(`  Packets Received : ${serverReceived}`);
  
  if (serverReceived === 3) {
    console.log('  Status           : [  OK  ] 100% Verified Distributed JS Mesh!');
    process.exit(0);
  } else {
    console.error('  Status           : [FAILED]');
    process.exit(1);
  }
}

main().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
