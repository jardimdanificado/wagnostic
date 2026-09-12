const { Piolho, createDefaultRegistry } = require('../../src');
const path = require('path');

let msgCount = 0;
let bytesCount = 0;
const linkStats = { ws: 0, broadcast: 0, local: 0 };

async function main() {
  const host = new Piolho({ intervalMs: 1 });
  const registry = createDefaultRegistry();
  host.use(registry);

  host.ipc.onMessage = ({ sender, target, size, data, op }) => {
    msgCount++;
    bytesCount += size;
    const s = (sender || '').toLowerCase();
    const t = (target || '').toLowerCase();
    if (t.includes('ws') || s.includes('ws')) linkStats.ws++;
    else if (t.includes('broadcast') || s.includes('broadcast') || t.includes('bc') || s.includes('bc')) linkStats.broadcast++;
    else linkStats.local++;
  };

  await host.loadRom(path.join(__dirname, 'client_ws_broadcast.wasm'), 'gamma_worker1');
  await host.loadRom(path.join(__dirname, 'echo_sibling.wasm'), 'gamma_worker2');
  await host.loadRom(path.join(__dirname, 'bc_echo.wasm'), 'gamma_bc_peer');

  process.on('message', (msg) => {
    if (msg === 'get_stats') {
      process.send({ msgCount, bytesCount, linkStats });
    }
  });

  await host.run(0);
}

main();
