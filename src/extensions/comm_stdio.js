/**
 * comm:stdio Extension
 * 
 * Exposes stdin, stdout, and stderr as synchronous rendezvous communication peers:
 * - tell("stdio:out", data, size, 0) -> writes to stdout
 * - tell("stdio:err", data, size, 0) -> writes to stderr
 * - hear("stdio:in", data, size, 0)  -> reads from stdin buffer
 * 
 * Supports: txiki.js (tjs), Node.js, Bun, Deno
 */

const { ENV } = require('../env');

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 16; // status (4) + auto_flush (4) + bytes_available (4) + reserved (4)

class CommStdioExtension {
  constructor() {
    this.name = ['comm:stdio', 'stdio'];
    this.stdinQueue = [];
    this.stdinInitialized = false;
  }

  isSupported() {
    return (
      (typeof process !== 'undefined' && process.stdout) ||
      (typeof tjs !== 'undefined' && tjs.stdout) ||
      (typeof Deno !== 'undefined' && Deno.stdout)
    );
  }

  _initStdin(host) {
    if (this.stdinInitialized) return;
    this.stdinInitialized = true;

    if (typeof process !== 'undefined' && process.stdin) {
      process.stdin.on('data', (chunk) => {
        const bytes = new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
        this.stdinQueue.push(bytes);
        host.ipc.receiveRemoteTell('stdio:in', '*', bytes);
      });
    } else if (typeof tjs !== 'undefined' && tjs.stdin) {
      try {
        const buf = new Uint8Array(1024);
        const poll = () => {
          try {
            const n = tjs.stdin.read(buf);
            if (n > 0) {
              const slice = buf.slice(0, n);
              this.stdinQueue.push(slice);
              host.ipc.receiveRemoteTell('stdio:in', '*', slice);
            }
          } catch (e) {}
        };
        if (typeof setInterval !== 'undefined') {
          setInterval(poll, 10);
        }
      } catch (e) {}
    }
  }

  onRequest(worker, host) {
    this._initStdin(host);

    // Register stdio peers on the host peer registry if not already registered
    if (!host.peers.has('stdio:out')) {
      host.peers.registerRemote('stdio:out', 'stdio', null, (sender, data) => {
        ENV.stdoutWrite(typeof data === 'string' ? data : new TextDecoder().decode(data));
      }, () => {});
    }
    if (!host.peers.has('stdout')) {
      host.peers.registerRemote('stdout', 'stdio', null, (sender, data) => {
        ENV.stdoutWrite(typeof data === 'string' ? data : new TextDecoder().decode(data));
      }, () => {});
    }

    if (!host.peers.has('stdio:err')) {
      host.peers.registerRemote('stdio:err', 'stdio', null, (sender, data) => {
        if (typeof process !== 'undefined' && process.stderr) {
          process.stderr.write(typeof data === 'string' ? data : Buffer.from(data));
        } else if (typeof tjs !== 'undefined' && tjs.stderr) {
          tjs.stderr.write(typeof data === 'string' ? new TextEncoder().encode(data) : data);
        } else {
          console.error(typeof data === 'string' ? data : new TextDecoder().decode(data));
        }
      }, () => {});
    }
    if (!host.peers.has('stderr')) {
      host.peers.registerRemote('stderr', 'stdio', null, (sender, data) => {
        if (typeof process !== 'undefined' && process.stderr) {
          process.stderr.write(typeof data === 'string' ? data : Buffer.from(data));
        } else {
          console.error(typeof data === 'string' ? data : new TextDecoder().decode(data));
        }
      }, () => {});
    }

    if (!worker.extState.has('comm:stdio')) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      const view = new DataView(worker.memory.buffer, ptr, STRUCT_SIZE);
      view.setInt32(0, COMM_STATUS_CONNECTED, true);
      view.setInt32(4, 1, true); // auto_flush = 1
      worker.extState.set('comm:stdio', { ptr });
    }

    return worker.extState.get('comm:stdio').ptr;
  }

  onBeforeUpdate(worker, host) {
    const state = worker.extState.get('comm:stdio');
    if (!state || !worker.memory) return;
    const view = new DataView(worker.memory.buffer, state.ptr, STRUCT_SIZE);
    let totalBytes = 0;
    for (const chunk of this.stdinQueue) totalBytes += chunk.length;
    view.setInt32(8, totalBytes, true);
  }

  onDestroy(host) {}
}

const commStdioExtension = new CommStdioExtension();

module.exports = {
  CommStdioExtension,
  commStdioExtension
};
