#!/usr/bin/env js140
/**
 * Wagnostic Benchmark Runner (SpiderMonkey JS shell)
 *
 * "Node.js-style" runner using the `js140` shell directly.
 * Uses SpiderMonkey's built-in WebAssembly API.
 *
 * Usage: js140 host_benchmark_js140.js <rom.wasm> [num_frames]
 */

// ── Helpers ──
var SYS_SIZE = 512;

function strFromMem(dv, off, maxLen) {
    var s = '';
    for (var i = 0; i < maxLen; i++) {
        var c = dv.getUint8(off + i);
        if (!c) break;
        s += String.fromCharCode(c);
    }
    return s;
}

function clearSignals(dv) {
    dv.setUint8(464, 0);
    dv.setUint8(465, 0);
    dv.setUint8(466, 0);
    dv.setUint8(467, 0);
}

// ── Main ──
var args = scriptArgs;  // SpiderMonkey shell built-in env variable
if (args.length < 1) {
    printErr("Usage: js140 host_benchmark_js140.js <rom.wasm> [num_frames]");
    quit(1);
}

var wasmPath = args[0];
var numFrames = parseInt(args[1], 10) || 100;

// Read WASM (synchronous, SpiderMonkey shell built-in)
var wasmBytes = read(wasmPath, "binary");

// Instantiate (synchronous API available in SM shell)
var module = new WebAssembly.Module(wasmBytes);
var instance = new WebAssembly.Instance(module, {env:{}});
var exports = instance.exports;

if (typeof exports.winit !== 'function' || typeof exports.wupdate !== 'function') {
    printErr("ERROR: ROM missing winit or wupdate export");
    quit(1);
}

if (!exports.memory) {
    printErr("ERROR: ROM missing memory export");
    quit(1);
}

// Call winit
exports.winit();

var mem = exports.memory;
var dv = new DataView(mem.buffer);

var W = dv.getUint32(128, true) || 320;
var H = dv.getUint32(132, true) || 240;
var BPP = dv.getUint32(136, true) || 8;
var vramBytes = W * H * (BPP / 8);
var title = strFromMem(dv, 0, 128);
var hasAudio = dv.getUint32(144, true) > 0;

print("========================================");
print("  WAGNOSTIC BENCHMARK (js140/SM)");
print("========================================");
print("ROM:          " + wasmPath);
print("Title:        " + title);
print("Resolution:   " + W + "x" + H + " @ " + BPP + "bpp");
print("VRAM:         " + Math.round(vramBytes / 1024) + " KB");
print("Engine:       SpiderMonkey (js140 shell)");
if (hasAudio) {
    print("Audio:        " + dv.getUint32(156, true) + " Hz, " +
          (dv.getUint32(160, true) * 8) + "-bit, " +
          dv.getUint32(164, true) + " ch, " +
          Math.round(dv.getUint32(144, true) / 1024) + " KB buffer");
} else {
    print("Audio:        none");
}
print("Frames:       " + numFrames);
print("========================================");
print("Running benchmark...");

// ── Benchmark loop ──
// performance.now() is available in SM140 and has sub-ms precision
var t1 = performance.now();
var totalUpdateMs = 0;
var minFrameMs = Infinity;
var maxFrameMs = 0;

// Escape the loop with a label so we can break out on error
benchLoop:
for (var i = 0; i < numFrames; i++) {
    dv.setUint32(168, i * 16, true);  // ticks

    var fs = performance.now();

    try {
        exports.wupdate();
    } catch (e) {
        printErr("\nERROR: wupdate failed at frame " + i + ": " + e);
        quit(1);
    }

    // Re-fetch DataView (memory may have grown)
    dv = new DataView(mem.buffer);

    var fe = performance.now();
    var frameMs = fe - fs;
    totalUpdateMs += frameMs;
    if (frameMs < minFrameMs) minFrameMs = frameMs;
    if (frameMs > maxFrameMs) maxFrameMs = frameMs;

    // Clear signals (host behavior)
    clearSignals(dv);
    dv.setInt32(460, 0, true);  // mouse_wheel

    if ((i + 1) % 10 === 0 || i === numFrames - 1) {
        var avg = totalUpdateMs / (i + 1);
        var progress = "  Frame " + (i + 1) + "/" + numFrames +
                       " (" + avg.toFixed(1) + " ms/frame avg)";
        // Use print w/o newline — SpiderMonkey shell doesn't support
        // write() the same way Node does, so use print with \r manually
        if (i === numFrames - 1) {
            print(progress);
        } else {
            print("\r" + progress);
        }
    }
}

var t2 = performance.now();
var totalMs = t2 - t1;
var avgMs = totalUpdateMs / numFrames;
var fps = 1000.0 / avgMs;
var mpps = (W * H * numFrames) / (totalMs / 1000) / 1e6;
var bw = (vramBytes * numFrames) / totalMs * 1000 / (1024 * 1024);

print("");
print("========================================");
print("  RESULTS (SpiderMonkey/js140)");
print("========================================");
print("Total time:       " + totalMs.toFixed(2) + " ms");
print("Avg frame time:   " + avgMs.toFixed(3) + " ms");
print("Min frame time:   " + minFrameMs.toFixed(3) + " ms");
print("Max frame time:   " + maxFrameMs.toFixed(3) + " ms");
print("Avg FPS:          " + fps.toFixed(1));
print("Pixels/sec:       " + mpps.toFixed(2) + " MP/s");
print("VRAM bandwidth:   " + bw.toFixed(2) + " MB/s");
print("Engine:           SpiderMonkey (js140 shell)");
print("========================================");

quit(0);
