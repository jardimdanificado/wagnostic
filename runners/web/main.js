const compileBtn = document.getElementById('compileBtn');
const codeTextArea = document.getElementById('code');
const logDiv = document.getElementById('log');

const worker = new Worker('xcc-worker.js?v=3', { type: 'module' });

function logMessage(msg, isError) {
    const p = document.createElement('div');
    p.textContent = msg;
    if (isError) p.style.color = '#f55';
    logDiv.appendChild(p);
    logDiv.scrollTop = logDiv.scrollHeight;
}

worker.onmessage = async (e) => {
    const msg = e.data;
    if (msg.type === 'log') {
        logMessage(msg.data, false);
    } else if (msg.type === 'error') {
        logMessage(msg.data, true);
        compileBtn.disabled = false;
        compileBtn.textContent = 'Compile & Run';
    } else if (msg.type === 'done') {
        logMessage('Compilation succeeded! Loading into Wagnostic Runner...', false);
        
        try {
            if (window.wagnosticLoadRomFromBuffer) {
                window.wagnosticLoadRomFromBuffer(msg.data.buffer || msg.data);
                logMessage('Running...', false);
            } else {
                logMessage('Wagnostic Runner not initialized.', true);
            }
        } catch (err) {
            logMessage('Runner error: ' + err, true);
        }
        
        compileBtn.disabled = false;
        compileBtn.textContent = 'Compile & Run';
    }
};

compileBtn.addEventListener('click', () => {
    logDiv.innerHTML = '';
    compileBtn.disabled = true;
    compileBtn.textContent = 'Compiling...';
    worker.postMessage({
        type: 'compile',
        code: codeTextArea.value
    });
});
