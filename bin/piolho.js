#!/usr/bin/env node
/**
 * Piolho 2.0 CLI
 * Universal Zero-Dependency WebAssembly Multi-ROM Host
 * Supports: txiki.js (tjs), Node.js, Bun, Deno
 */

const { ENV } = require('../src/env');
const { PiolhoHost } = require('../src/host');

async function main() {
  const romSpecs = [];
  let maxFrames = 1;
  let targetFps = 30;
  let gifPath = null;

  for (let i = 0; i < ENV.argv.length; i++) {
    const arg = ENV.argv[i];
    if (arg === '-n' || arg === '--frames') {
      maxFrames = parseInt(ENV.argv[++i], 10) || 0;
    } else if (arg.startsWith('-n=')) {
      maxFrames = parseInt(arg.split('=')[1], 10) || 0;
    } else if (arg === '-fps' || arg.startsWith('--fps=')) {
      targetFps = parseInt(arg.includes('=') ? arg.split('=')[1] : ENV.argv[++i], 10) || 30;
    } else if (arg === '--headless') {
      /* No-op: runners are headless/embedded by default */
    } else if (arg === '-g' || arg.startsWith('--gif=')) {
      gifPath = arg.includes('=') ? arg.split('=')[1] : ENV.argv[++i];
    } else if (!arg.startsWith('-')) {
      romSpecs.push(arg);
    }
  }

  if (romSpecs.length === 0) {
    console.log('Piolho 2.0 Universal Host (txiki.js, Node.js, Bun, Deno)');
    console.log('Usage: piolho <rom1.wasm[:name1]> [rom2.wasm[:name2] ...] [-n <frames>] [-fps <fps>] [-g <out.gif>]');
    ENV.exit(1);
  }

  const host = new PiolhoHost({
    maxFrames,
    targetFps,
    gifPath
  });

  try {
    for (const spec of romSpecs) {
      await host.loadRom(spec);
    }
    await host.run();
    ENV.exit(0);
  } catch (err) {
    console.error('Fatal error:', err.message);
    ENV.exit(1);
  }
}

main();
