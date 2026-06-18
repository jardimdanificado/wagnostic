// Wagnostic WebGPU Runner - GPU-Heavy Implementation
// Maximizes GPU usage for rendering and audio processing

const canvas = document.getElementById('gameCanvas');
const emptyState = document.getElementById('emptyState');

let device = null;
let context = null;
let wasmInstance = null;
let wasmMemory = null;

// GPU Resources
let vramTexture = null;
let vramTextureView = null;
let sampler = null;
let renderPipeline = null;
let renderBindGroup = null;
let audioComputePipeline = null;
let audioBindGroup = null;
let audioBuffer = null;
let audioOutputBuffer = null;
let systemUniformsBuffer = null;

// State
let width = 320, height = 240, bpp = 8, scale = 1;
let audioSize = 0, audioRate = 44100, audioBpp = 1, audioChannels = 1;
let animationFrameId = null;
let lastFrameTime = 0;
const FRAME_MIN_TIME = 1000 / 60;

// Input state
const input = {
    keys: new Uint8Array(256),
    buttons: 0,
    mouse: { x: 0, y: 0, buttons: 0, wheel: 0 }
};

// USB HID Scancodes
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

// Shader code
const renderShaderCode = `
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texCoord: vec2<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var pos = array<vec2<f32>, 6>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>(-1.0,  1.0),
        vec2<f32>(-1.0,  1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>( 1.0,  1.0),
    );
    
    var tex = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 1.0),
        vec2<f32>(1.0, 1.0),
        vec2<f32>(0.0, 0.0),
        vec2<f32>(0.0, 0.0),
        vec2<f32>(1.0, 1.0),
        vec2<f32>(1.0, 0.0),
    );
    
    var output: VertexOutput;
    output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
    output.texCoord = tex[vertexIndex];
    return output;
}

@group(0) @binding(0) var vram_texture: texture_2d<f32>;
@group(0) @binding(1) var vram_sampler: sampler;
@group(0) @binding(2) var<uniform> bpp_uniform: u32;

@fragment
fn fs_main(@location(0) texCoord: vec2<f32>) -> @location(0) vec4<f32> {
    let texSize = vec2<f32>(textureDimensions(vram_texture));
    let pixel = floor(texCoord * texSize);
    let pixelCoord = vec2<i32>(pixel);
    
    var color: vec4<f32>;
    
    if (bpp_uniform == 8u) {
        // RGB332
        let raw = textureLoad(vram_texture, pixelCoord, 0).r;
        let rawByte = u32(raw * 255.0);
        let r = f32((rawByte >> 5u) & 0x07u) / 7.0;
        let g = f32((rawByte >> 2u) & 0x07u) / 7.0;
        let b = f32(rawByte & 0x03u) / 3.0;
        color = vec4<f32>(r, g, b, 1.0);
    } else if (bpp_uniform == 16u) {
        // RGB565
        let raw = textureLoad(vram_texture, pixelCoord, 0);
        let packed = u32(raw.r * 255.0) | (u32(raw.g * 255.0) << 8u);
        let r = f32((packed >> 11u) & 0x1Fu) / 31.0;
        let g = f32((packed >> 5u) & 0x3Fu) / 63.0;
        let b = f32(packed & 0x1Fu) / 31.0;
        color = vec4<f32>(r, g, b, 1.0);
    } else {
        // RGBA8888
        color = textureLoad(vram_texture, pixelCoord, 0);
    }
    
    return color;
}
`;

const audioComputeShaderCode = `
struct AudioParams {
    read_ptr: u32,
    write_ptr: u32,
    buffer_size: u32,
    sample_rate: u32,
    channels: u32,
    bpp: u32,
    _padding0: u32,
    _padding1: u32,
}

@group(0) @binding(0) var<storage, read> audio_input: array<u32>;
@group(0) @binding(1) var<storage, read_write> audio_output: array<f32>;
@group(0) @binding(2) var<uniform> params: AudioParams;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    let idx = id.x;
    let total_samples = params.buffer_size / params.bpp;
    
    if (idx >= total_samples) {
        return;
    }
    
    // Calculate read position in ring buffer
    let read_offset = (params.read_ptr + idx * params.bpp) % params.buffer_size;
    let word_idx = read_offset / 4u;
    let byte_offset = read_offset % 4u;
    
    var sample: f32;
    
    if (params.bpp == 1u) {
        // 8-bit unsigned PCM
        let raw = audio_input[word_idx];
        let byte_val = (raw >> (byte_offset * 8u)) & 0xFFu;
        sample = (f32(byte_val) - 128.0) / 128.0;
    } else if (params.bpp == 2u) {
        // 16-bit signed PCM
        let raw = audio_input[word_idx];
        let word_val = i32((raw >> (byte_offset * 8u)) & 0xFFFFu);
        if (word_val > 32767) {
            word_val = word_val - 65536;
        }
        sample = f32(word_val) / 32768.0;
    } else {
        // 32-bit float
        sample = bitcast<f32>(audio_input[word_idx]);
    }
    
    // Apply simple low-pass filter
    let prev = select(0.0, audio_output[idx - 1u], idx > 0u);
    audio_output[idx] = sample * 0.8 + prev * 0.2;
}
`;

async function initWebGPU() {
    if (!navigator.gpu) {
        alert('WebGPU not supported in this browser');
        return false;
    }
    
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
        alert('Failed to get GPU adapter');
        return false;
    }
    
    device = await adapter.requestDevice();
    context = canvas.getContext('webgpu');
    
    const format = navigator.gpu.getPreferredCanvasFormat();
    context.configure({
        device: device,
        format: format,
        alphaMode: 'opaque',
    });
    
    // Create sampler (nearest neighbor for pixel-perfect scaling)
    sampler = device.createSampler({
        magFilter: 'nearest',
        minFilter: 'nearest',
    });
    
    return true;
}

function createVRAMTexture() {
    if (vramTexture) {
        vramTexture.destroy();
    }
    
    let format;
    if (bpp === 8) {
        format = 'r8unorm';
    } else if (bpp === 16) {
        format = 'rg8unorm';
    } else {
        format = 'rgba8unorm';
    }
    
    vramTexture = device.createTexture({
        size: { width, height },
        format: format,
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
    });
    
    vramTextureView = vramTexture.createView();
}

function createRenderPipeline() {
    const shaderModule = device.createShaderModule({
        code: renderShaderCode,
    });
    
    const bindGroupLayout = device.createBindGroupLayout({
        entries: [
            { binding: 0, visibility: GPUShaderStage.FRAGMENT, texture: {} },
            { binding: 1, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
            { binding: 2, visibility: GPUShaderStage.FRAGMENT, buffer: { type: 'uniform' } },
        ],
    });
    
    const pipelineLayout = device.createPipelineLayout({
        bindGroupLayouts: [bindGroupLayout],
    });
    
    renderPipeline = device.createRenderPipeline({
        layout: pipelineLayout,
        vertex: {
            module: shaderModule,
            entryPoint: 'vs_main',
        },
        fragment: {
            module: shaderModule,
            entryPoint: 'fs_main',
            targets: [{ format: navigator.gpu.getPreferredCanvasFormat() }],
        },
        primitive: {
            topology: 'triangle-list',
        },
    });
    
    // Create uniforms buffer
    systemUniformsBuffer = device.createBuffer({
        size: 4, // Just bpp for now
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM,
    });
    
    // Create bind group
    renderBindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        entries: [
            { binding: 0, resource: vramTextureView },
            { binding: 1, resource: sampler },
            { binding: 2, resource: { buffer: systemUniformsBuffer } },
        ],
    });
}

function createAudioPipeline() {
    const shaderModule = device.createShaderModule({
        code: audioComputeShaderCode,
    });
    
    const bindGroupLayout = device.createBindGroupLayout({
        entries: [
            { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: 'read-only-storage' } },
            { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: 'storage' } },
            { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: 'uniform' } },
        ],
    });
    
    const pipelineLayout = device.createPipelineLayout({
        bindGroupLayouts: [bindGroupLayout],
    });
    
    audioComputePipeline = device.createComputePipeline({
        layout: pipelineLayout,
        compute: {
            module: shaderModule,
            entryPoint: 'main',
        },
    });
    
    // Create audio buffers
    audioBuffer = device.createBuffer({
        size: audioSize,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE,
    });
    
    audioOutputBuffer = device.createBuffer({
        size: audioSize,
        usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
    });
}

async function loadWasm(buffer) {
    if (animationFrameId) cancelAnimationFrame(animationFrameId);
    
    const importObject = { env: {} };
    const { instance } = await WebAssembly.instantiate(buffer, importObject);
    wasmInstance = instance;
    wasmMemory = instance.exports.memory;
    
    if (instance.exports.winit) instance.exports.winit();
    
    // Read system config
    const dv = new DataView(wasmMemory.buffer);
    width = dv.getUint32(128, true);
    height = dv.getUint32(132, true);
    bpp = dv.getUint32(136, true);
    scale = dv.getUint32(140, true) || 1;
    
    canvas.width = width;
    canvas.height = height;
    canvas.style.width = (width * scale) + 'px';
    canvas.style.height = (height * scale) + 'px';
    if (emptyState) emptyState.style.display = 'none';
    canvas.focus();
    
    // Create GPU resources
    createVRAMTexture();
    createRenderPipeline();
    
    // Setup audio if needed
    audioSize = dv.getUint32(144, true);
    if (audioSize > 0) {
        audioRate = dv.getUint32(156, true);
        audioBpp = dv.getUint32(160, true);
        audioChannels = dv.getUint32(164, true);
        createAudioPipeline();
        setupWebAudio();
    }
    
    lastFrameTime = performance.now();
    animationFrameId = requestAnimationFrame(gameLoop);
}

let audioCtx = null;
let scriptNode = null;

function setupWebAudio() {
    if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    
    scriptNode = audioCtx.createScriptProcessor(2048, 0, audioChannels);
    scriptNode.onaudioprocess = processAudioGPU;
    scriptNode.connect(audioCtx.destination);
}

async function processAudioGPU(e) {
    if (!wasmMemory || !audioComputePipeline) return;
    
    const dv = new DataView(wasmMemory.buffer);
    const readPtr = dv.getUint32(152, true);
    const writePtr = dv.getUint32(148, true);
    const vramSize = width * height * (bpp / 8);
    const audioOffset = 512 + vramSize;
    
    // Copy audio data to GPU
    const audioData = new Uint8Array(wasmMemory.buffer, audioOffset, audioSize);
    device.queue.writeBuffer(audioBuffer, 0, audioData);
    
    // Update audio params
    const paramsBuffer = new ArrayBuffer(32);
    const paramsView = new DataView(paramsBuffer);
    paramsView.setUint32(0, readPtr, true);
    paramsView.setUint32(4, writePtr, true);
    paramsView.setUint32(8, audioSize, true);
    paramsView.setUint32(12, audioRate, true);
    paramsView.setUint32(16, audioChannels, true);
    paramsView.setUint32(20, audioBpp, true);
    device.queue.writeBuffer(audioComputePipeline.getBindGroup(0).getEntry(2).buffer, 0, paramsBuffer);
    
    // Run compute shader
    const commandEncoder = device.createCommandEncoder();
    const computePass = commandEncoder.beginComputePass();
    computePass.setPipeline(audioComputePipeline);
    computePass.setBindGroup(0, audioComputePipeline.getBindGroup(0));
    computePass.dispatchWorkgroups(Math.ceil(audioSize / audioBpp / 64));
    computePass.end();
    device.queue.submit([commandEncoder.finish()]);
    
    // Read back processed audio
    const outputData = new Float32Array(e.outputBuffer.length * audioChannels);
    await audioOutputBuffer.mapAsync(GPUMapMode.READ);
    const mappedData = new Float32Array(audioOutputBuffer.getMappedRange());
    outputData.set(mappedData.slice(0, outputData.length));
    audioOutputBuffer.unmap();
    
    // Fill output channels
    for (let ch = 0; ch < audioChannels; ch++) {
        const channelData = e.outputBuffer.getChannelData(ch);
        for (let i = 0; i < channelData.length; i++) {
            channelData[i] = outputData[i * audioChannels + ch] || 0;
        }
    }
    
    // Update read pointer
    const samplesProcessed = e.outputBuffer.length * audioChannels;
    const newReadPtr = (readPtr + samplesProcessed * audioBpp) % audioSize;
    dv.setUint32(152, newReadPtr, true);
}

function gameLoop(now) {
    const elapsed = now - lastFrameTime;
    
    if (elapsed >= FRAME_MIN_TIME) {
        lastFrameTime = now - (elapsed % FRAME_MIN_TIME);
        
        const dv = new DataView(wasmMemory.buffer);
        
        // Update ticks
        dv.setUint32(168, performance.now(), true);
        
        // Update inputs
        const wasmKeys = new Uint8Array(wasmMemory.buffer, 192, 256);
        wasmKeys.set(input.keys);
        dv.setUint32(172, input.buttons, true);
        dv.setInt32(448, input.mouse.x, true);
        dv.setInt32(452, input.mouse.y, true);
        dv.setUint32(456, input.mouse.buttons, true);
        dv.setInt32(460, input.mouse.wheel, true);
        input.mouse.wheel = 0;
        
        // Call wupdate
        if (wasmInstance.exports.wupdate) {
            wasmInstance.exports.wupdate();
        }
        
        // Process signals
        const signals = new Uint8Array(wasmMemory.buffer, 464, 4);
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
                width = dv.getUint32(128, true);
                height = dv.getUint32(132, true);
                scale = dv.getUint32(140, true) || 1;
                canvas.width = width;
                canvas.height = height;
                canvas.style.width = (width * scale) + 'px';
                canvas.style.height = (height * scale) + 'px';
                createVRAMTexture();
                createRenderPipeline();
            } else if (sig === 6) {
                const msg = new TextDecoder().decode(new Uint8Array(wasmMemory.buffer, 0, 128)).split('\0')[0];
                console.info("ROM:", msg);
            }
            signals[i] = 0;
        }
        
        if (shouldRedraw) {
            renderFrame();
        }
    }
    
    animationFrameId = requestAnimationFrame(gameLoop);
}

function renderFrame() {
    const dv = new DataView(wasmMemory.buffer);
    const vramSize = width * height * (bpp / 8);
    const vramOffset = 512;
    
    // Copy VRAM to GPU texture
    const vramData = new Uint8Array(wasmMemory.buffer, vramOffset, vramSize);
    device.queue.writeTexture(
        { texture: vramTexture },
        vramData,
        { bytesPerRow: width * (bpp / 8) },
        { width, height }
    );
    
    // Update bpp uniform
    const bppData = new Uint32Array([bpp]);
    device.queue.writeBuffer(systemUniformsBuffer, 0, bppData);
    
    // Render to canvas
    const commandEncoder = device.createCommandEncoder();
    const renderPass = commandEncoder.beginRenderPass({
        colorAttachments: [{
            view: context.getCurrentTexture().createView(),
            clearValue: { r: 0, g: 0, b: 0, a: 1 },
            loadOp: 'clear',
            storeOp: 'store',
        }],
    });
    
    renderPass.setPipeline(renderPipeline);
    renderPass.setBindGroup(0, renderBindGroup);
    renderPass.draw(6); // Fullscreen quad
    renderPass.end();
    
    device.queue.submit([commandEncoder.finish()]);
}

// Input handlers
const wasmInput = document.getElementById('wasmInput');
wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const buffer = await file.arrayBuffer();
    
    if (!device) {
        if (!await initWebGPU()) return;
    }
    
    await loadWasm(buffer);
});

canvas.addEventListener('mousemove', e => {
    const r = canvas.getBoundingClientRect();
    input.mouse.x = Math.floor((e.clientX - r.left) * (canvas.width / r.width));
    input.mouse.y = Math.floor((e.clientY - r.top) * (canvas.height / r.height));
});

canvas.addEventListener('mousedown', e => input.mouse.buttons |= (1 << e.button));
canvas.addEventListener('mouseup', e => input.mouse.buttons &= ~(1 << e.button));
canvas.addEventListener('wheel', e => input.mouse.wheel += Math.sign(e.deltaY), { passive: true });

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
