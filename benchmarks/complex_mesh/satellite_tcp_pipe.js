const { Piolho, createDefaultRegistry } = require('../../src');
const path = require('path');

let msgCount = 0;
let bytesCount = 0;

async function main() {
  const host = new Piolho({ intervalMs: 1 });
  const registry = createDefaultRegistry();
  host.use(registry);

  host.ipc.onMessage = ({ sender, target, size, data, op }) => {
    msgCount++;
    bytesCount += size;
  };

  await host.loadRom(path.join(__dirname, 'client_tcp_pipe.wasm'), 'beta_worker1');
  await host.loadRom(path.join(__dirname, 'echo_sibling.wasm'), 'beta_worker2');

  process.on('message', (msg) => {
    if (msg === 'get_stats') {
      process.send({ msgCount, bytesCount });
    }
  });

  await host.run(0);
}

main();
