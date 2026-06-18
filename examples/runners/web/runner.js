
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

const sysOffset = 0;

// Audio State
let audioCtx = null;
let audioNode = null;

const input = {
    keys: new Uint8Array(256),
    buttons: 0,
    mouse: { x: 0, y: 0, buttons: 0, wheel: 0 }
};

// USB HID Scancodes (approximate mapping for web)
const keyMap = {
    'KeyA': 4, 'KeyB': 5, 'KeyC': 6, 'KeyD': 7, 'KeyE': 8, 'KeyF': 9, 'KeyG': 10, 'KeyH': 11,
    'KeyI': 12, 'KeyJ': 13, 'KeyK': 14, 'KeyL': 15, 'KeyM': 16, 'KeyN': 17, 'KeyO': 18, 'KeyP': 19,
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


async function loadCartridge(buffer) {
    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    if (audioCtx.state === 'suspended') audioCtx.resume();
    await loadWasm(buffer);
}

async function loadWasm(buffer) {
    if (animationFrameId) cancelAnimationFrame(animationFrameId);
    if (audioNode) { audioNode.disconnect(); audioNode = null; }

    const importObject = { env: {} }; // Pure shared memory, no imports needed

    const { instance } = await WebAssembly.instantiate(buffer, importObject);
    wasmInstance = instance;
    wasmMemory = instance.exports.memory;

    if (instance.exports.winit) instance.exports.winit();

    const dv = new DataView(wasmMemory.buffer);
    const w = dv.getUint32(sysOffset + 128, true);
    const h = dv.getUint32(sysOffset + 132, true);
    const s = dv.getUint32(sysOffset + 140, true) || 1;

    canvas.width = w;
    canvas.height = h;
    canvas.style.width = (w * s) + 'px';
    canvas.style.height = (h * s) + 'px';
    if (emptyState) emptyState.style.display = 'none';
    canvas.focus();

    const audioSize = dv.getUint32(sysOffset + 144, true);
    if (audioSize > 0) {
        const audioRate = dv.getUint32(sysOffset + 156, true);
        const audioBpp = dv.getUint32(sysOffset + 160, true);
        const audioChannels = dv.getUint32(sysOffset + 164, true);
        setupWebAudio(audioSize, audioRate, audioBpp, audioChannels || 1);
    }
    
    lastFrameTime = performance.now();
    animationFrameId = requestAnimationFrame(gameLoop);
}

function setupWebAudio(size, rate, bpp, channels) {
    if (audioCtx.sampleRate !== rate) {
        // Note: Sample rate change in Web Audio is tricky, usually requires new context
        console.warn("ROM requested different sample rate than AudioContext:", rate);
    }
    audioNode = audioCtx.createScriptProcessor(2048, 0, channels);
    
    audioNode.onaudioprocess = (e) => {
        if (!wasmMemory) return;
        const dv = new DataView(wasmMemory.buffer);
        let r = dv.getUint32(sysOffset + 152, true);
        const w = dv.getUint32(sysOffset + 148, true);
        const s = dv.getUint32(sysOffset + 144, true);
        
        const width = dv.getUint32(sysOffset + 128, true);
        const height = dv.getUint32(sysOffset + 132, true);
        const videoBpp = dv.getUint32(sysOffset + 136, true);
        const vramSize = width * height * (videoBpp / 8);
        const audioBufPtr = 512 + vramSize;

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
                if (bpp === 1) {
                    sample = (mem8[audioBufPtr + r] - 128) / 128;
                    r = (r + 1) % s;
                } else if (bpp === 4) {
                    sample = memF32[(audioBufPtr + r) / 4];
                    r = (r + 4) % s;
                } else {
                    sample = mem16[(audioBufPtr + r) / 2] / 32768;
                    r = (r + 2) % s;
                }
                outChannels[ch][i] = sample;
            }
        }
        dv.setUint32(sysOffset + 152, r, true);
    };
    audioNode.connect(audioCtx.destination);
}

function gameLoop(now) {
    if (!wasmInstance) return;
    const elapsed = now - lastFrameTime;

    if (elapsed >= FRAME_MIN_TIME) {
        lastFrameTime = now - (elapsed % FRAME_MIN_TIME);
        const dv = new DataView(wasmMemory.buffer);

        // Update Ticks (Offset 168)
        dv.setUint32(sysOffset + 168, performance.now(), true);

        // Update Inputs
        const wasmKeys = new Uint8Array(wasmMemory.buffer, sysOffset + 192, 256);
        wasmKeys.set(input.keys);
        dv.setUint32(sysOffset + 172, input.buttons, true);
        dv.setInt32(sysOffset + 448, input.mouse.x, true);
        dv.setInt32(sysOffset + 452, input.mouse.y, true);
        dv.setUint32(sysOffset + 456, input.mouse.buttons, true);
        dv.setInt32(sysOffset + 460, input.mouse.wheel, true);
        input.mouse.wheel = 0; // Reset wheel

        if (wasmInstance.exports.wupdate) wasmInstance.exports.wupdate();
        else if (wasmInstance.exports.frame) wasmInstance.exports.frame();

        // Process 4 Signals (Offset 464)
        const signals = new Uint8Array(wasmMemory.buffer, sysOffset + 464, 4);
        let shouldRedraw = false;
        for (let i = 0; i < 4; i++) {
            const sig = signals[i];
            if (sig === 0) continue;
            if (sig === 1) shouldRedraw = true;
            else if (sig === 2) { window.close(); return; }
            else if (sig === 3) {
                const msg = new TextDecoder().decode(new Uint8Array(wasmMemory.buffer, 0, 128)).split('\0')[0];
                document.title = msg;
            } else if (sig === 4) {
                const w = dv.getUint32(sysOffset + 128, true);
                const h = dv.getUint32(sysOffset + 132, true);
                const s = dv.getUint32(sysOffset + 140, true) || 1;
                canvas.width = w; canvas.height = h;
                canvas.style.width = (w * s) + 'px'; canvas.style.height = (h * s) + 'px';
            } else if (sig === 6) {
                const msg = new TextDecoder().decode(new Uint8Array(wasmMemory.buffer, 0, 128)).split('\0')[0];
                console.info("ROM:", msg);
            }
            signals[i] = 0;
        }

        if (shouldRedraw) renderFrame(dv);
    }
    animationFrameId = requestAnimationFrame(gameLoop);
}

function renderFrame(dv) {
    const w = dv.getUint32(sysOffset + 128, true);
    const h = dv.getUint32(sysOffset + 132, true);
    const bpp = dv.getUint32(sysOffset + 136, true);
    if (w === 0 || h === 0) return;

    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }
    
    const data32 = new Uint32Array(imageDataCache.data.buffer);
    const vramPtr = 512;
    
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
