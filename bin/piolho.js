#!/usr/bin/env node

/**
 * Piolho CLI — Universal WebAssembly Host & Rendezvous IPC Coordinator
 */

const path = require('path');
const {
  Piolho,
  ENV,
  clockExtension,
  loggerExtension,
  commTcpExtension,
  commPipeExtension,
  commWsExtension,
  commUdpExtension,
  commWorkersExtension,
  commStdioExtension,
  commBroadcastExtension,
  commWebrtcExtension,
  commWebtransportExtension,
  commSerialExtension,
  commBluetoothExtension,
  commHttpExtension,
  commShmExtension
} = require('../src');

const BUILTIN_EXTENSIONS = {
  'clock': clockExtension,
  'logger': loggerExtension
};

function printHelp() {
  console.log(`
Piolho 2.0 — Universal WASM Host & Rendezvous IPC Coordinator

Usage:
  piolho [options] <rom.wasm[:name]> [rom2.wasm[:name]...]

Options:
  -e, --ext <name|file>    Enable extension (e.g. clock, logger, ./custom.js)
  -E, --ext-dir <path>     Directory to search for extensions (name.js) when requested by name
  -s, --steps <N>          Run for N steps/ticks and exit (default: 0 = infinite)
  -r, --rate <hz>          Tick rate in Hz / ticks per second (default: 30)
  -i, --interval <ms>      Tick interval in milliseconds
  -v, --version            Display version
  -h, --help               Display this help message

Notes:
  - Communication extensions (comm:tcp, comm:pipe, comm:ws, comm:udp, comm:workers) are always enabled.
  - Standard extensions (clock, logger, etc.) are strictly opt-in via -e/--ext.
  - When -E <path> is specified, requesting 'abc' will search for path/abc.js.
  - Instance name defaults to file basename if not specified as 'path.wasm:name'.

Examples:
  piolho worker.wasm
  piolho master.wasm:master worker.wasm:worker
  piolho -E ./my_extensions -e custom_dsp worker.wasm
  piolho -s 100 benchmark.wasm
`);
}

async function main() {
  const args = ENV.args ? ENV.args.slice(1) : (process.argv.slice(2));

  if (args.length === 0 || args.includes('-h') || args.includes('--help')) {
    printHelp();
    return;
  }

  if (args.includes('-v') || args.includes('--version')) {
    const pkg = require('../package.json');
    console.log(`piolho v${pkg.version}`);
    return;
  }

  let steps = 0;
  let intervalMs = 0;
  let rateHz = 0;
  const extDirs = [];
  const requestedExts = [];
  const romSpecs = [];

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];

    if (arg === '-s' || arg === '--steps' || arg === '--frames') {
      steps = parseInt(args[++i], 10) || 0;
    } else if (arg === '-r' || arg === '--rate' || arg === '--fps') {
      rateHz = parseFloat(args[++i]) || 30;
    } else if (arg === '-i' || arg === '--interval') {
      intervalMs = parseFloat(args[++i]) || 0;
    } else if (arg === '-E' || arg === '--ext-dir' || arg === '--extensions-dir') {
      const dir = args[++i];
      if (dir) extDirs.push(dir);
    } else if (arg === '-e' || arg === '--ext' || arg === '--use') {
      const extArg = args[++i];
      if (extArg) {
        for (const item of extArg.split(',')) {
          const trimmed = item.trim();
          if (trimmed) requestedExts.push(trimmed);
        }
      }
    } else if (arg.startsWith('-')) {
      console.error(`Unknown option: ${arg}`);
      printHelp();
      if (typeof process !== 'undefined' && process.exit) process.exit(1);
      return;
    } else {
      romSpecs.push(arg);
    }
  }

  if (romSpecs.length === 0) {
    console.error('Error: No ROM files specified.');
    printHelp();
    if (typeof process !== 'undefined' && process.exit) process.exit(1);
    return;
  }

  const hostOptions = { extDirs };
  if (intervalMs > 0) {
    hostOptions.intervalMs = intervalMs;
  } else if (rateHz > 0) {
    hostOptions.tickRate = rateHz;
  }

  const host = new Piolho(hostOptions);

  // Hardcoded communication extensions (auto-checked against environment)
  host.use(commTcpExtension)
      .use(commPipeExtension)
      .use(commWsExtension)
      .use(commUdpExtension)
      .use(commWorkersExtension)
      .use(commStdioExtension)
      .use(commBroadcastExtension)
      .use(commWebrtcExtension)
      .use(commWebtransportExtension)
      .use(commSerialExtension)
      .use(commBluetoothExtension)
      .use(commHttpExtension)
      .use(commShmExtension);

  // User-requested extensions
  for (const extName of requestedExts) {
    if (BUILTIN_EXTENSIONS[extName]) {
      host.use(BUILTIN_EXTENSIONS[extName]);
    } else {
      try {
        host.use(extName);
      } catch (err) {
        console.error(`Error loading extension '${extName}':`, err.message);
        if (typeof process !== 'undefined' && process.exit) process.exit(1);
        return;
      }
    }
  }

  // Load all ROMs
  for (const spec of romSpecs) {
    let filePath = spec;
    let name = null;

    if (spec.includes(':')) {
      const idx = spec.lastIndexOf(':');
      filePath = spec.substring(0, idx);
      name = spec.substring(idx + 1);
    }

    try {
      await host.loadRom(filePath, name);
    } catch (err) {
      console.error(`Error loading ROM '${spec}':`, err.message);
      if (typeof process !== 'undefined' && process.exit) process.exit(1);
      return;
    }
  }

  const exitCode = await host.run(steps);
  if (typeof process !== 'undefined' && process.exit && exitCode !== 0) {
    process.exit(exitCode);
  }
}

if (typeof require !== 'undefined' && require.main === module) {
  main().catch((err) => {
    console.error('Fatal runtime error:', err);
    if (typeof process !== 'undefined' && process.exit) process.exit(1);
  });
}

module.exports = { main };
