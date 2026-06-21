#!/usr/bin/env node
/**
 * Wagnostic Benchmark Runner (Node.js / V8)
 *
 * Like the V8 C++ benchmark runner, but using Node.js's built-in V8.
 * Usage: node host_benchmark_node.js <rom.wasm> [num_frames]
 *
 * This is the JavaScript equivalent of host_benchmark_v8.cpp — it loads a
 * Wagnostic ROM via WebAssembly, runs winit/wupdate in a timed loop, and
 * prints the same metrics (frame time, FPS, MP/s, VRAM bandwidth).
 */

const fs = require('fs');
const path = require('path');

// ── SystemConfig struct layout (must match SystemConfig in host_benchmark_v8.cpp) ──
// All fields are little-endian uint32/int32 unless noted.
const SYS = {
  message:        [0,   128],  // char[128]
  width:          [128, 4],    // uint32
  height:         [132, 4],
  bpp:            [136, 4],
  scale:          [140, 4],
  audio_size:     [144, 4],
  audio_write:    [148, 4],
  audio_read:     [152, 4],
  audio_rate:     [156, 4],
  audio_bpp:      [160, 4],
  audio_channels: [164, 4],
  ticks:          [168, 4],
  gamepad:        [172, 4],
  keys:           [192, 256],  // uint8[256]
  mouse_x:        [448, 4],    // int32
  mouse_y:        [452, 4],    // int32
  mouse_buttons:  [456, 4],    // uint32
  mouse_wheel:    [460, 4],    // int32
  signals:        [464, 4],    // uint8[4]
};

const SYS_SIZE = 512; // SystemConfig occupies bytes 0-511

// ── Helpers ──
class SysView {
  constructor(memory) {
    this.dv = new DataView(memory.buffer);
  }

  getU32(name) {
    const [off, len] = SYS[name];
    return this.dv.getUint32(off, true);
  }

  setU32(name, val) {
    const [off, len] = SYS[name];
    this.dv.setUint32(off, val, true);
  }

  getStr(name, maxLen) {
    const [off] = SYS[name];
    let s = '';
    for (let i = 0; i < maxLen; i++) {
      const c = this.dv.getUint8(off + i);
      if (c === 0) break;
      s += String.fromCharCode(c);
    }
    return s;
  }

  clearSignals() {
    const [off] = SYS.signals;
    for (let i = 0; i < 4; i++) this.dv.setUint8(off + i, 0);
  }
}

// ── Main ──
async function main() {
  const args = process.argv.slice(2);
  if (args.length < 1) {
    console.error('Usage: node host_benchmark_node.js <rom.wasm> [num_frames]');
    process.exit(1);
  }

  const wasmPath = path.resolve(args[0]);
  const numFrames = parseInt(args[1], 10) || 100;

  // Read WASM
  const wasmBytes = fs.readFileSync(wasmPath);

  // Instantiate
  const importObject = { env: {} };
  const { instance } = await WebAssembly.instantiate(wasmBytes, importObject);
  const exports = instance.exports;

  if (typeof exports.winit !== 'function' || typeof exports.wupdate !== 'function') {
    console.error('ERROR: ROM missing winit or wupdate export');
    process.exit(1);
  }

  if (!exports.memory || !(exports.memory instanceof WebAssembly.Memory)) {
    console.error('ERROR: ROM missing memory export');
    process.exit(1);
  }

  // Call winit
  exports.winit();

  const mem = exports.memory;
  const sys = new SysView(mem);

  // Read config after init
  const W = sys.getU32('width') || 320;
  const H = sys.getU32('height') || 240;
  const BPP = sys.getU32('bpp') || 8;
  const vramBytes = W * H * (BPP / 8);
  const title = sys.getStr('message', 128);
  const hasAudio = sys.getU32('audio_size') > 0;

  console.log('========================================');
  console.log('  WAGNOSTIC BENCHMARK (Node.js/V8)');
  console.log('========================================');
  console.log(`ROM:          ${wasmPath}`);
  console.log(`Title:        ${title}`);
  console.log(`Resolution:   ${W}x${H} @ ${BPP}bpp`);
  console.log(`VRAM:         ${Math.round(vramBytes / 1024)} KB`);
  console.log(`Engine:       Node.js ${process.version} (V8 ${process.versions.v8})`);
  if (hasAudio) {
    console.log(`Audio:        ${sys.getU32('audio_rate')} Hz, ${sys.getU32('audio_bpp') * 8}-bit, ${sys.getU32('audio_channels')} ch, ${Math.round(sys.getU32('audio_size') / 1024)} KB buffer`);
  } else {
    console.log('Audio:        none');
  }
  console.log(`Frames:       ${numFrames}`);
  console.log('========================================');
  console.log('Running benchmark...');

  // ── Benchmark loop ──
  const startMs = performance.now();
  let totalUpdateMs = 0;
  let minFrameMs = Infinity;
  let maxFrameMs = 0;

  for (let i = 0; i < numFrames; i++) {
    sys.setU32('ticks', i * 16);

    const frameStart = performance.now();

    exports.wupdate();

    const frameEnd = performance.now();
    const frameMs = frameEnd - frameStart;
    totalUpdateMs += frameMs;
    if (frameMs < minFrameMs) minFrameMs = frameMs;
    if (frameMs > maxFrameMs) maxFrameMs = frameMs;

    // Clear signals (host behaviour)
    sys.clearSignals();
    sys.setU32('mouse_wheel', 0);

    if ((i + 1) % 10 === 0 || i === numFrames - 1) {
      process.stdout.write(`\r  Frame ${i + 1}/${numFrames} (${(totalUpdateMs / (i + 1)).toFixed(1)} ms/frame avg)`);
    }
  }

  const endMs = performance.now();
  const totalMs = endMs - startMs;
  const avgMs = totalUpdateMs / numFrames;
  const fps = 1000.0 / avgMs;
  const mpps = (W * H * numFrames) / (totalMs / 1000) / 1e6;
  const bw = (vramBytes * numFrames) / totalMs * 1000 / (1024 * 1024);

  console.log('\n\n');
  console.log('========================================');
  console.log('  RESULTS (Node.js/V8)');
  console.log('========================================');
  console.log(`Total time:       ${totalMs.toFixed(2)} ms`);
  console.log(`Avg frame time:   ${avgMs.toFixed(3)} ms`);
  console.log(`Min frame time:   ${minFrameMs.toFixed(3)} ms`);
  console.log(`Max frame time:   ${maxFrameMs.toFixed(3)} ms`);
  console.log(`Avg FPS:          ${fps.toFixed(1)}`);
  console.log(`Pixels/sec:       ${mpps.toFixed(2)} MP/s`);
  console.log(`VRAM bandwidth:   ${bw.toFixed(2)} MB/s`);
  console.log(`Engine:           Node.js ${process.version} (V8 ${process.versions.v8})`);
  console.log('========================================');
}

main().catch(err => {
  console.error('FATAL:', err);
  process.exit(1);
});
