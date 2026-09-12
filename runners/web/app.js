import { Wagnostic } from '../js/wagnostic.js';
import {
  handleExtension,
  updateStd,
  getFramebuffer,
  setKey,
  setMouse,
  resetStd,
  setLogHandler,
  KEY_MAP
} from '../js/std.js';

const canvas = document.getElementById('screen');
const ctx = canvas.getContext('2d', { alpha: false });
const fileInput = document.getElementById('fileInput');
const romSelect = document.getElementById('romSelect');
const btnPlay = document.getElementById('btnPlay');
const btnStep = document.getElementById('btnStep');
const btnReset = document.getElementById('btnReset');
const fpsDisplay = document.getElementById('fpsDisplay');
const resDisplay = document.getElementById('resDisplay');
const frameDisplay = document.getElementById('frameDisplay');
const statusDisplay = document.getElementById('statusDisplay');
const logOutput = document.getElementById('logOutput');

let runner = null;
let currentWasmBytes = null;
let isRunning = false;
let animationFrameId = null;
let frameCount = 0;
let lastFpsTime = performance.now();
let framesInSecond = 0;
let imageData = null;

function appendLog(msg) {
  logOutput.value += `[${new Date().toLocaleTimeString()}] ${msg}\n`;
  logOutput.scrollTop = logOutput.scrollHeight;
}

setLogHandler(appendLog);

function pauseLoop() {
  isRunning = false;
  btnPlay.textContent = 'Play';
  if (animationFrameId !== null) {
    cancelAnimationFrame(animationFrameId);
    animationFrameId = null;
  }
}

function startLoop() {
  if (isRunning) return;
  if (!runner) return;
  isRunning = true;
  btnPlay.textContent = 'Pause';
  lastFpsTime = performance.now();
  framesInSecond = 0;
  statusDisplay.textContent = 'Running';
  loop();
}

async function loadRom(wasmBytes) {
  pauseLoop();

  currentWasmBytes = wasmBytes;
  frameCount = 0;
  framesInSecond = 0;
  frameDisplay.textContent = '0';
  fpsDisplay.textContent = '0';
  imageData = null;

  // Clear canvas
  ctx.fillStyle = '#000000';
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  // Reset extension state and instantiate fresh runner with directly exported handleExtension
  resetStd();
  runner = new Wagnostic(handleExtension);

  try {
    statusDisplay.textContent = 'Loading...';
    await runner.init(wasmBytes);
    appendLog('ROM loaded successfully.');
    startLoop();
  } catch (err) {
    statusDisplay.textContent = 'Load Error';
    appendLog(`Failed to load ROM: ${err.message}`);
    console.error(err);
  }
}

function renderFrame() {
  if (!runner || !runner.memory) return;

  updateStd(runner);

  let status = 0;
  try {
    status = runner.step();
  } catch (err) {
    appendLog(`Runtime error in update(): ${err.message}`);
    statusDisplay.textContent = 'Runtime Error';
    pauseLoop();
    return;
  }

  frameCount++;
  framesInSecond++;
  frameDisplay.textContent = frameCount;

  const now = performance.now();
  if (now - lastFpsTime >= 1000) {
    fpsDisplay.textContent = Math.round((framesInSecond * 1000) / (now - lastFpsTime));
    framesInSecond = 0;
    lastFpsTime = now;
  }

  const fb = getFramebuffer(runner);
  if (fb && fb.pixels) {
    if (canvas.width !== fb.width || canvas.height !== fb.height || !imageData) {
      canvas.width = fb.width;
      canvas.height = fb.height;
      imageData = ctx.createImageData(fb.width, fb.height);
      resDisplay.textContent = `${fb.width}x${fb.height}`;
    }

    const buf8 = new Uint8ClampedArray(
      runner.memory.buffer,
      fb.pixelsPtr,
      fb.width * fb.height * 4
    );
    imageData.data.set(buf8);
    ctx.putImageData(imageData, 0, 0);
  }

  if (status === 1) {
    appendLog('ROM requested exit (WUPDATE_EXIT).');
    statusDisplay.textContent = 'Finished (Exit)';
    pauseLoop();
  } else if (status < 0) {
    appendLog(`ROM returned error code: ${status}`);
    statusDisplay.textContent = `Error (${status})`;
    pauseLoop();
  }
}

function loop() {
  if (!isRunning) return;
  renderFrame();
  if (isRunning) {
    animationFrameId = requestAnimationFrame(loop);
  }
}

// Input Event Listeners
window.addEventListener('keydown', (e) => {
  const scancode = KEY_MAP[e.code];
  if (scancode !== undefined) setKey(scancode, true);
});

window.addEventListener('keyup', (e) => {
  const scancode = KEY_MAP[e.code];
  if (scancode !== undefined) setKey(scancode, false);
});

const updateMouseCoords = (e) => {
  const rect = canvas.getBoundingClientRect();
  const scaleX = canvas.width / rect.width;
  const scaleY = canvas.height / rect.height;
  const mx = Math.floor((e.clientX - rect.left) * scaleX);
  const my = Math.floor((e.clientY - rect.top) * scaleY);
  setMouse(mx, my);
};

canvas.addEventListener('mousemove', updateMouseCoords);
canvas.addEventListener('mousedown', (e) => {
  updateMouseCoords(e);
  let btns = 0;
  if (e.button === 0) btns |= (1 << 0);
  if (e.button === 2) btns |= (1 << 1);
  if (e.button === 1) btns |= (1 << 2);
  setMouse(undefined, undefined, btns);
});
canvas.addEventListener('mouseup', (e) => {
  updateMouseCoords(e);
  setMouse(undefined, undefined, 0);
});
canvas.addEventListener('contextmenu', (e) => e.preventDefault());

// UI Controls
btnPlay.addEventListener('click', () => {
  if (isRunning) {
    pauseLoop();
    statusDisplay.textContent = 'Paused';
  } else if (runner) {
    startLoop();
  }
});

btnStep.addEventListener('click', () => {
  pauseLoop();
  statusDisplay.textContent = 'Stepped';
  renderFrame();
});

btnReset.addEventListener('click', () => {
  if (currentWasmBytes) {
    loadRom(currentWasmBytes);
  }
});

fileInput.addEventListener('change', async (e) => {
  const file = e.target.files[0];
  if (file) {
    const bytes = await file.arrayBuffer();
    loadRom(bytes);
  }
});

romSelect.addEventListener('change', async (e) => {
  const url = e.target.value;
  if (url) {
    try {
      const res = await fetch(url);
      const bytes = await res.arrayBuffer();
      loadRom(bytes);
    } catch (err) {
      appendLog(`Failed to fetch ROM: ${err.message}`);
    }
  }
});
