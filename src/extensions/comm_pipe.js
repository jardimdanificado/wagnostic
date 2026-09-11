/**
 * comm:pipe Extension
 * 
 * Supports Active and Passive Unix Domain Socket / Named Pipe modes.
 */

const { encodePacket, PacketParser, MSG_HELLO, MSG_HELLO_ACK, MSG_TELL } = require('../wire');

const COMM_MODE_CONNECT      = 0;
const COMM_MODE_LISTEN       = 1;

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_LISTENING  = 3;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 204; // 128 (path) + 4 (mode) + 4 (status) + 4 (peer_count) + 32 (adv_name) + 32 (peer_name)

class CommPipeExtension {
  constructor() {
    this.name = 'comm:pipe';
  }

  isSupported() {
    return (
      (typeof require !== 'undefined' && (function() { try { return !!require('net'); } catch (e) { return false; } })()) ||
      (typeof tjs !== 'undefined' && typeof tjs.listen === 'function')
    );
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;
    if (!worker.extState.has(this.name)) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set(this.name, {
        ptr,
        socket: null,
        server: null,
        peerName: '',
        peerCount: 0,
        status: COMM_STATUS_IDLE
      });
    }
    return worker.extState.get(this.name).ptr;
  }

  onBeforeUpdate(worker, host) {
    const state = worker.extState.get(this.name);
    if (!state || !worker.memory) return;

    const view = new DataView(worker.memory.buffer, state.ptr, STRUCT_SIZE);
    const bytes = new Uint8Array(worker.memory.buffer, state.ptr, STRUCT_SIZE);

    const mode = view.getInt32(128, true);
    const status = view.getInt32(132, true);

    if (status === COMM_STATUS_CONNECTING && state.status !== COMM_STATUS_CONNECTING) {
      state.status = COMM_STATUS_CONNECTING;

      let pathStr = '';
      for (let i = 0; i < 128 && bytes[i] !== 0; i++) pathStr += String.fromCharCode(bytes[i]);
      if (!pathStr) pathStr = '/tmp/piolho_pipe.sock';

      let advName = '';
      for (let i = 0; i < 32 && bytes[140 + i] !== 0; i++) advName += String.fromCharCode(bytes[140 + i]);
      if (!advName) advName = worker.name;
      state.advertisedName = advName;

      if (mode === COMM_MODE_CONNECT) {
        this._connectClient(worker, host, state, pathStr);
      } else {
        this._startServer(worker, host, state, pathStr);
      }
    }
  }

  _connectClient(worker, host, state, pipePath) {
    try {
      const net = require('net');
      const socket = net.createConnection({ path: pipePath }, () => {
        const helloPkt = encodePacket(MSG_HELLO, state.advertisedName || worker.name, '', null);
        socket.write(helloPkt);
      });

      const parser = new PacketParser((pkt) => {
        this._handlePacket(worker, host, state, socket, pkt);
      });

      socket.on('data', (chunk) => parser.push(chunk));
      socket.on('error', () => {
        this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
      });
      socket.on('close', () => {
        if (state.peerName) {
          host.peers.unregister(state.peerName);
          state.peerCount = Math.max(0, state.peerCount - 1);
          this._updateStatus(worker, state, COMM_STATUS_IDLE, '');
        }
      });

      state.socket = socket;
    } catch (e) {
      this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
    }
  }

  _startServer(worker, host, state, pipePath) {
    try {
      const net = require('net');
      const fs = require('fs');
      try { if (fs.existsSync(pipePath)) fs.unlinkSync(pipePath); } catch (e) {}

      const server = net.createServer((socket) => {
        const parser = new PacketParser((pkt) => {
          this._handlePacket(worker, host, state, socket, pkt);
        });

        socket.on('data', (chunk) => parser.push(chunk));
        socket.on('close', () => {
          if (state.peerName) {
            host.peers.unregister(state.peerName);
            state.peerCount = Math.max(0, state.peerCount - 1);
            this._updateStatus(worker, state, state.peerCount > 0 ? COMM_STATUS_CONNECTED : COMM_STATUS_LISTENING, '');
          }
        });
      });

      server.listen(pipePath, () => {
        this._updateStatus(worker, state, COMM_STATUS_LISTENING, '');
      });

      server.on('error', () => {
        this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
      });

      state.server = server;
      state.pipePath = pipePath;
    } catch (e) {
      this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
    }
  }

  _handlePacket(worker, host, state, socket, pkt) {
    if (pkt.msgType === MSG_HELLO) {
      const ackPkt = encodePacket(MSG_HELLO_ACK, state.advertisedName || worker.name, pkt.sender, null);
      socket.write(ackPkt);

      const remoteName = pkt.sender;
      state.peerName = remoteName;
      state.peerCount++;
      host.peers.registerRemote(remoteName, 'pipe', socket, (s, data) => {
        socket.write(encodePacket(MSG_TELL, s, remoteName, data));
      }, () => socket.destroy());

      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, remoteName);
    } else if (pkt.msgType === MSG_HELLO_ACK) {
      const remoteName = pkt.sender;
      state.peerName = remoteName;
      state.peerCount++;
      host.peers.registerRemote(remoteName, 'pipe', socket, (s, data) => {
        socket.write(encodePacket(MSG_TELL, s, remoteName, data));
      }, () => socket.destroy());

      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, remoteName);
    } else if (pkt.msgType === MSG_TELL) {
      host.ipc.receiveRemoteTell(pkt.sender, pkt.target, pkt.payload);
    }
  }

  _updateStatus(worker, state, statusCode, peerName) {
    state.status = statusCode;
    if (!worker.memory) return;
    const view = new DataView(worker.memory.buffer, state.ptr, STRUCT_SIZE);
    view.setInt32(132, statusCode, true);
    view.setInt32(136, state.peerCount, true);

    if (peerName) {
      const bytes = new Uint8Array(worker.memory.buffer, state.ptr, STRUCT_SIZE);
      const peerOffset = 172;
      for (let i = 0; i < 32; i++) {
        bytes[peerOffset + i] = i < peerName.length ? peerName.charCodeAt(i) : 0;
      }
    }
  }

  onAfterUpdate(worker, host) {}
  onFrameComplete(host) {}

  onDestroy(host) {
    for (const w of host.workers) {
      const state = w.extState.get(this.name);
      if (state) {
        if (state.socket) try { state.socket.destroy(); } catch (e) {}
        if (state.server) try { state.server.close(); } catch (e) {}
        if (state.pipePath) {
          try { require('fs').unlinkSync(state.pipePath); } catch (e) {}
        }
      }
    }
  }
}

const commPipeExtension = new CommPipeExtension();

module.exports = {
  CommPipeExtension,
  commPipeExtension
};
