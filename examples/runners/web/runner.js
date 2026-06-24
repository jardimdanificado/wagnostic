// Wagnostic Web Runner - Named Globals Version
// Reads/writes WASM globals by name instead of fixed memory offsets.

// UI Elements
const canvas = document.getElementById('gameCanvas');
const emptyState = document.getElementById('emptyState');
const ctx = canvas.getContext('2d', { alpha: false, desynchronized: true });

let wasmInstance = null;
let wasmMemory = null;
let animationFrameId = null;
let imageDataCache = null;
let lastFrameTime = 0;
const FRAME_MIN_TIME = 1000 / 60; // Target 60 FPS

// Config change detection
let lastWidth = 0;
let lastHeight = 0;
let lastScale = 0;
let lastBpp = 0;

// Audio State
let audioCtx = null;
let audioNode = null;

// Input state
const input = {
    keys: new Uint8Array(256),
    buttons: 0,
    mouse: { x: 0, y: 0, buttons: 0, wheel: 0 }
};

// USB HID Scancodes (approximate mapping for web)
const keyMap = {
    'KeyA': 4, 'KeyB': 5, 'KeyC': 6, 'KeyD': 7, 'KeyE': 8, 'KeyF': 9, 'KeyG': 10, 'KeyH': 11,
    'KeyI': 12, 'KeyJ': 13, 'KeyJ': 13, 'KeyK': 14, 'KeyL': 15, 'KeyM': 16, 'KeyN': 17, 'KeyO': 18, 'KeyP': 19,
    'KeyQ': 20, 'KeyR': 21, 'KeyS': 22, 'KeyT': 23, 'KeyU': 24, 'KeyV': 25, 'KeyW': 26, 'KeyX': 27,
    'KeyY': 28, 'KeyZ': 29, 'Digit1': 30, 'Digit2': 31, 'Digit3': 32, 'Digit4': 33, 'Digit5': 34,
    'Digit6': 35, 'Digit7': 36, 'Digit8': 37, 'Digit9': 38, 'Digit0': 39, 'Enter': 40, 'Escape': 41,
    'Backspace': 42, 'Tab': 43, 'Space': 44, 'Minus': 45, 'Equal': 46, 'BracketLeft': 47, 'BracketRight': 48,
    'Backslash': 49, 'Semicolon': 51, 'Quote': 52, 'Comma': 54, 'Period': 55, 'Slash': 56,
    'ArrowRight': 79, 'ArrowLeft': 80, 'ArrowDown': 81, 'ArrowUp': 82
};

const btnMap = {
    'up': 1 << 0, 'down': 1 << 1, 'left': 1 << 2, 'right': 1 << 3,
    'a': 1 << 4, 'b': 1 << 5, 'select': 1 << 6, 'start': 1 << 7
};

const wasmInput = document.getElementById('wasmInput');

wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const buffer = await file.arrayBuffer();
    await loadCartridge(buffer);
});

// ============================================
// Global Read/Write Helpers
// These read/write WASM globals by name.
// Each global holds a pointer (i32) to the actual
// data location in linear memory.
// ============================================

function readGlobalU32(name) {
    const g = wasmInstance.exports[name];
    if (!g || g.value === 0) return 0;
    const dv = new DataView(wasmMemory.buffer);
    return dv.getUint32(g.value, true);
}

function writeGlobalU32(name, val) {
    const g = wasmInstance.exports[name];
    if (!g || g.value === 0) return;
    const dv = new DataView(wasmMemory.buffer);
    dv.setUint32(g.value, val, true);
}

function readGlobalI32(name) {
    const g = wasmInstance.exports[name];
    if (!g || g.value === 0) return 0;
    const dv = new DataView(wasmMemory.buffer);
    return dv.getInt32(g.value, true);
}

function writeGlobalI32(name, val) {
    const g = wasmInstance.exports[name];
    if (!g || g.value === 0) return;
    const dv = new DataView(wasmMemory.buffer);
    dv.setInt32(g.value, val, true);
}

function readGlobalU8(name) {
    const g = wasmInstance.exports[name];
    if (!g || g.value === 0) return 0;
    const mem8 = new Uint8Array(wasmMemory.buffer);
    return mem8[g.value];
}

function writeGlobalU8(name, val) {
    const g = wasmInstance.exports[name];
    if (!g || g.value === 0) return;
    const mem8 = new Uint8Array(wasmMemory.buffer);
    mem8[g.value] = val;
}

function readGlobalStr(name, maxLen) {
    const g = wasmInstance.exports[name];
    if (!g || g.value === 0) return '';
    const mem8 = new Uint8Array(wasmMemory.buffer);
    let len = 0;
    while (len < maxLen - 1 && mem8[g.value + len] !== 0) len++;
    return new TextDecoder().decode(new Uint8Array(wasmMemory.buffer, g.value, len));
}

// Get pointer to array data from a global (for w_vram, w_keys, w_audio_buffer)
function getGlobalPtr(name) {
    const g = wasmInstance.exports[name];
    if (!g || g.value === 0) return 0;
    return g.value;
}

// ============================================
// Cartridge Loading
// ============================================

async function loadCartridge(buffer) {
    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    if (audioCtx.state === 'suspended') audioCtx.resume();
    await loadWasm(buffer);
}

async function loadWasm(buffer) {
    if (animationFrameId) cancelAnimationFrame(animationFrameId);
    if (audioNode) { audioNode.disconnect(); audioNode = null; }

    let instance = null;
    const importObject = {
        env: {
            strlen: function(ptr) {
                const buf = new Uint8Array(instance.exports.memory.buffer);
                let s = 0;
                while (buf[ptr + s] !== 0) s++;
                return s;
            }
        }
    };

    const result = await WebAssembly.instantiate(buffer, importObject);
    instance = result.instance;
    wasmInstance = instance;
    wasmMemory = instance.exports.memory;

    // Call winit if present
    if (instance.exports.winit) instance.exports.winit();

    // Read config from named globals
    const w = readGlobalU32('w_width') || 320;
    const h = readGlobalU32('w_height') || 240;
    const s = readGlobalU32('w_scale') || 1;

    canvas.width = w;
    canvas.height = h;
    canvas.style.width = (w * s) + 'px';
    canvas.style.height = (h * s) + 'px';
    canvas.style.imageRendering = 'pixelated';
    if (emptyState) emptyState.style.display = 'none';
    canvas.focus();

    // Set up audio if needed
    const audioSize = readGlobalU32('w_audio_size');
    if (audioSize > 0) {
        const audioRate = readGlobalU32('w_audio_sample_rate');
        const audioBpp = readGlobalU32('w_audio_bpp');
        const audioChannels = readGlobalU32('w_audio_channels') || 1;
        setupWebAudio(audioSize, audioRate, audioBpp, audioChannels);
    }

    lastFrameTime = performance.now();
    animationFrameId = requestAnimationFrame(gameLoop);
}

// ============================================
// Audio
// ============================================

function setupWebAudio(size, rate, bpp, channels) {
    if (audioCtx.sampleRate !== rate) {
        console.warn("ROM requested different sample rate than AudioContext:", rate);
    }
    audioNode = audioCtx.createScriptProcessor(2048, 0, channels);

    audioNode.onaudioprocess = (e) => {
        if (!wasmMemory) return;

        let r = readGlobalU32('w_audio_read');
        const w = readGlobalU32('w_audio_write');
        const s = readGlobalU32('w_audio_size');
        const audioBpp = readGlobalU32('w_audio_bpp');
        const audioBufPtr = getGlobalPtr('w_audio_buffer');
        if (!audioBufPtr) return;

        const frameCount = e.outputBuffer.length;
        const outChannels = [];
        for (let ch = 0; ch < channels; ch++) outChannels.push(e.outputBuffer.getChannelData(ch));

        const mem8 = new Uint8Array(wasmMemory.buffer);
        const mem16 = new Int16Array(wasmMemory.buffer);
        const memF32 = new Float32Array(wasmMemory.buffer);

        for (let i = 0; i < frameCount; i++) {
            if (r === w) {
                for (let ch = 0; ch < channels; ch++) outChannels[ch][i] = 0;
                continue;
            }
            for (let ch = 0; ch < channels; ch++) {
                let sample = 0;
                if (audioBpp === 1) {
                    sample = (mem8[audioBufPtr + r] - 128) / 128;
                    r = (r + 1) % s;
                } else if (audioBpp === 4) {
                    sample = memF32[(audioBufPtr + r) / 4];
                    r = (r + 4) % s;
                } else {
                    sample = mem16[(audioBufPtr + r) / 2] / 32768;
                    r = (r + 2) % s;
                }
                outChannels[ch][i] = sample;
            }
        }
        writeGlobalU32('w_audio_read', r);
    };
    audioNode.connect(audioCtx.destination);
}

// ============================================
// Game Loop
// ============================================

function gameLoop(now) {
    if (!wasmInstance) return;
    const elapsed = now - lastFrameTime;

    if (elapsed >= FRAME_MIN_TIME) {
        lastFrameTime = now - (elapsed % FRAME_MIN_TIME);

        // Update ticks
        writeGlobalU32('w_ticks', performance.now());

        // Update keyboard input - write to w_keys array
        const keysPtr = getGlobalPtr('w_keys');
        if (keysPtr) {
            const wasmKeys = new Uint8Array(wasmMemory.buffer, keysPtr, 256);
            wasmKeys.set(input.keys);
        }

        // Update gamepad buttons
        writeGlobalU32('w_gamepad_buttons', input.buttons);

        // Update mouse
        writeGlobalI32('w_mouse_x', input.mouse.x);
        writeGlobalI32('w_mouse_y', input.mouse.y);
        writeGlobalU32('w_mouse_buttons', input.mouse.buttons);
        writeGlobalI32('w_mouse_wheel', input.mouse.wheel);
        input.mouse.wheel = 0; // Reset wheel

        // Auto-detect config changes
        detectConfigChanges();

        // Call ROM update function — returns 0 to quit
        let keep = 1;
        if (wasmInstance.exports.wupdate) keep = wasmInstance.exports.wupdate();
        else if (wasmInstance.exports.frame) wasmInstance.exports.frame();
        if (!keep) {
            if (animationFrameId) cancelAnimationFrame(animationFrameId);
            return;
        }

        // Read dirty count and render dirty regions
        const dirtyCount = readGlobalU32('w_dirty_count');
        if (dirtyCount > 0) {
            renderDirtyRects(dirtyCount);
        }
    }
    animationFrameId = requestAnimationFrame(gameLoop);
}

// ============================================
// Config Change Detection
// ============================================

function detectConfigChanges() {
    const w = readGlobalU32('w_width') || 320;
    const h = readGlobalU32('w_height') || 240;
    const s = readGlobalU32('w_scale') || 1;
    const bpp = readGlobalU32('w_bpp') || 16;

    if (w !== lastWidth || h !== lastHeight || s !== lastScale || bpp !== lastBpp) {
        lastWidth = w;
        lastHeight = h;
        lastScale = s;
        lastBpp = bpp;

        canvas.width = w;
        canvas.height = h;
        canvas.style.width = (w * s) + 'px';
        canvas.style.height = (h * s) + 'px';
        canvas.style.imageRendering = 'pixelated';

        // Invalidate image data cache so it's recreated on next render
        imageDataCache = null;

        // Check for title change
        const title = readGlobalStr('w_title', 128);
        if (title) document.title = title;
    }
}

// ============================================
// Rendering (Canvas 2D, dirty rectangles)
// ============================================

function renderDirtyRects(count) {
    const w = readGlobalU32('w_width') || 320;
    const h = readGlobalU32('w_height') || 240;
    const bpp = readGlobalU32('w_bpp') || 16;
    if (w === 0 || h === 0) return;

    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }

    // Get pointer to dirty_rects array (each rect = 16 bytes: x, y, w, h as int32)
    const rectsPtr = getGlobalPtr('w_dirty_rects');
    if (!rectsPtr) {
        // Fallback: full screen render if dirty rects unavailable
        renderFrame();
        return;
    }

    const dv = new DataView(wasmMemory.buffer);

    // Fast path: if single dirty rect covers entire screen, do full render
    if (count === 1) {
        const rx = dv.getInt32(rectsPtr, true);
        const ry = dv.getInt32(rectsPtr + 4, true);
        const rw = dv.getInt32(rectsPtr + 8, true);
        const rh = dv.getInt32(rectsPtr + 12, true);
        if (rx === 0 && ry === 0 && rw === w && rh === h) {
            renderFrame();
            return;
        }
    }

    // General case: render each dirty rect
    const vramPtr = getGlobalPtr('w_vram');
    if (!vramPtr) return;

    for (let i = 0; i < count && i < 32; i++) {
        const offset = rectsPtr + i * 16;
        const rx = dv.getInt32(offset, true);
        const ry = dv.getInt32(offset + 4, true);
        const rw = dv.getInt32(offset + 8, true);
        const rh = dv.getInt32(offset + 12, true);

        // Clamp to screen bounds
        const sx = Math.max(0, rx);
        const sy = Math.max(0, ry);
        const ex = Math.min(w, rx + rw);
        const ey = Math.min(h, ry + rh);
        if (sx >= ex || sy >= ey) continue;

        renderRegion(sx, sy, ex - sx, ey - sy, vramPtr, w, bpp);
    }

    // Flush to canvas
    ctx.putImageData(imageDataCache, 0, 0);
}

function renderRegion(sx, sy, rw, rh, vramPtr, w, bpp) {
    const data32 = new Uint32Array(imageDataCache.data.buffer);

    if (bpp === 32) {
        for (let y = sy; y < sy + rh; y++) {
            const srcStart = y * w + sx;
            let di = y * w + sx;
            const src32 = new Uint32Array(wasmMemory.buffer, vramPtr + srcStart * 4, rw);
            for (let si = 0; si < rw; si++) {
                data32[di++] = src32[si];
            }
        }
    } else if (bpp === 8) {
        const frame8 = new Uint8Array(wasmMemory.buffer, vramPtr);
        for (let y = sy; y < sy + rh; y++) {
            let di = y * w + sx;
            let si = y * w + sx;
            for (let x = 0; x < rw; x++) {
                const c = frame8[si++];
                const r = ((c >> 5) & 0x07) * 255 / 7;
                const g = ((c >> 2) & 0x07) * 255 / 7;
                const b = (c & 0x03) * 255 / 3;
                data32[di++] = (255 << 24) | (b << 16) | (g << 8) | r;
            }
        }
    } else {
        // 16bpp (RGB565)
        const frame16 = new Uint16Array(wasmMemory.buffer, vramPtr);
        for (let y = sy; y < sy + rh; y++) {
            let di = y * w + sx;
            let si = y * w + sx;
            for (let x = 0; x < rw; x++) {
                const c = frame16[si++];
                const r = ((c >> 11) & 0x1f) * 255 / 31;
                const g = ((c >> 5) & 0x3f) * 255 / 63;
                const b = (c & 0x1f) * 255 / 31;
                data32[di++] = (255 << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }
}

// Full screen render (used by fast path)
function renderFrame() {
    const w = readGlobalU32('w_width') || 320;
    const h = readGlobalU32('w_height') || 240;
    const bpp = readGlobalU32('w_bpp') || 16;
    if (w === 0 || h === 0) return;

    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }

    const data32 = new Uint32Array(imageDataCache.data.buffer);
    const vramPtr = getGlobalPtr('w_vram');
    if (!vramPtr) return;

    if (bpp === 32) {
        data32.set(new Uint32Array(wasmMemory.buffer, vramPtr, w * h));
    } else if (bpp === 8) {
        const frame8 = new Uint8Array(wasmMemory.buffer, vramPtr, w * h);
        for (let i = 0; i < w * h; i++) {
            const c = frame8[i];
            const r = ((c >> 5) & 0x07) * 255 / 7;
            const g = ((c >> 2) & 0x07) * 255 / 7;
            const b = (c & 0x03) * 255 / 3;
            data32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
        }
    } else {
        // 16bpp (RGB565)
        const frame16 = new Uint16Array(wasmMemory.buffer, vramPtr, w * h);
        for (let i = 0; i < w * h; i++) {
            const c = frame16[i];
            const r = ((c >> 11) & 0x1f) * 255 / 31;
            const g = ((c >> 5) & 0x3f) * 255 / 63;
            const b = (c & 0x1f) * 255 / 31;
            data32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
        }
    }
    ctx.putImageData(imageDataCache, 0, 0);
}

// ============================================
// Input Event Handlers
// ============================================

canvas.addEventListener('mousemove', e => {
    const r = canvas.getBoundingClientRect();
    input.mouse.x = Math.floor((e.clientX - r.left) * (canvas.width / r.width));
    input.mouse.y = Math.floor((e.clientY - r.top) * (canvas.height / r.height));
});
canvas.addEventListener('mousedown', e => input.mouse.buttons |= (1 << e.button));
canvas.addEventListener('mouseup', e => input.mouse.buttons &= ~(1 << e.button));
canvas.addEventListener('wheel', e => input.mouse.wheel += Math.sign(e.deltaY), {passive:true});

document.querySelectorAll('[data-btn]').forEach(btn => {
    const bit = btnMap[btn.dataset.btn];
    if (!bit) return;
    btn.addEventListener('mousedown', () => input.buttons |= bit);
    btn.addEventListener('mouseup', () => input.buttons &= ~bit);
    btn.addEventListener('mouseleave', () => input.buttons &= ~bit);
});

window.addEventListener('keydown', e => {
    const c = keyMap[e.code];
    if (c) input.keys[c] = 1;
    if (e.key === 'z' || e.key === 'Z') input.buttons |= btnMap.a;
    if (e.key === 'x' || e.key === 'X') input.buttons |= btnMap.b;
    if (e.key === 'Shift') input.buttons |= btnMap.select;
    if (e.key === 'Enter') input.buttons |= btnMap.start;
});

window.addEventListener('keyup', e => {
    const c = keyMap[e.code];
    if (c) input.keys[c] = 0;
    if (e.key === 'z' || e.key === 'Z') input.buttons &= ~btnMap.a;
    if (e.key === 'x' || e.key === 'X') input.buttons &= ~btnMap.b;
    if (e.key === 'Shift') input.buttons &= ~btnMap.select;
    if (e.key === 'Enter') input.buttons &= ~btnMap.start;
});
