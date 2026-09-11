const { Piolho, createDefaultRegistry } = require('../../src');
const path = require('path');

let msgCount = 0;
let bytesCount = 0;
const linkStats = { local: 0, tcp: 0, pipe: 0, ws: 0, udp: 0, broadcast: 0, stdio: 0 };

async function main() {
  const host = new Piolho({ intervalMs: 1 });
  const registry = createDefaultRegistry();
  host.use(registry);

  host.ipc.onMessage = ({ sender, target, size, data, op }) => {
    msgCount++;
    bytesCount += size;
    const s = (sender || '').toLowerCase();
    const t = (target || '').toLowerCase();
    if (t.includes('stdio') || s.includes('stdio')) linkStats.stdio++;
    else if (t.includes('tcp') || s.includes('tcp')) linkStats.tcp++;
    else if (t.includes('pipe') || s.includes('pipe') || t.includes('sock')) linkStats.pipe++;
    else if (t.includes('ws') || s.includes('ws')) linkStats.ws++;
    else if (t.includes('udp') || s.includes('udp')) linkStats.udp++;
    else if (t.includes('broadcast') || s.includes('broadcast') || t.includes('bc') || s.includes('bc')) linkStats.broadcast++;
    else linkStats.local++;
  };

  await host.loadRom(path.join(__dirname, 'hub_node.wasm'), 'hub_worker1');
  await host.loadRom(path.join(__dirname, 'echo_sibling.wasm'), 'hub_worker2');

  process.on('message', (msg) => {
    if (msg === 'get_stats') {
      process.send({ msgCount, bytesCount, linkStats });
    }
  });

  await host.run(0);
}

main();
