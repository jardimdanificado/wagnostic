/**
 * comm:ws Extension
 * 
 * Supports Active (WebSocket client) and Passive (WebSocket server) modes.
 */

const { encodePacket, PacketParser, MSG_HELLO, MSG_HELLO_ACK, MSG_TELL } = require('../wire');

const COMM_MODE_CONNECT      = 0;
const COMM_MODE_LISTEN       = 1;

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_LISTENING  = 3;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 208; // 128 (url) + 4 (port) + 4 (mode) + 4 (status) + 4 (peer_count) + 32 (adv_name) + 32 (peer_name)

class CommWsExtension {
  constructor() {
    this.name = 'comm:ws';
  }

  isSupported() {
    return (
      typeof WebSocket !== 'undefined' ||
      typeof globalThis.WebSocket !== 'undefined' ||
      (typeof require !== 'undefined' && (function() { try { return !!require('ws'); } catch (e) { return false; } })())
    );
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;
    if (!worker.extState.has(this.name)) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set(this.name, {
        ptr,
        ws: null,
        httpServer: null,
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

    const port = view.getInt32(128, true);
    const mode = view.getInt32(132, true);
    const status = view.getInt32(136, true);

    if (status === COMM_STATUS_CONNECTING && state.status !== COMM_STATUS_CONNECTING) {
      state.status = COMM_STATUS_CONNECTING;

      let urlStr = '';
      for (let i = 0; i < 128 && bytes[i] !== 0; i++) urlStr += String.fromCharCode(bytes[i]);
      if (!urlStr) urlStr = 'ws://127.0.0.1:8080';

      let advName = '';
      for (let i = 0; i < 32 && bytes[144 + i] !== 0; i++) advName += String.fromCharCode(bytes[144 + i]);
      if (!advName) advName = worker.name;
      state.advertisedName = advName;

      if (mode === COMM_MODE_CONNECT) {
        this._connectClient(worker, host, state, urlStr);
      } else {
        this._startServer(worker, host, state, port || 8080);
      }
    }
  }

  _connectClient(worker, host, state, targetUrl) {
    try {
      if (typeof WebSocket !== 'undefined') {
        const ws = new WebSocket(targetUrl);
        ws.binaryType = 'arraybuffer';

        ws.onopen = () => {
          const helloPkt = encodePacket(MSG_HELLO, state.advertisedName || worker.name, '', null);
          ws.send(helloPkt);
        };

        const parser = new PacketParser((pkt) => {
          this._handlePacket(worker, host, state, ws, pkt);
        });

        ws.onmessage = (event) => {
          const data = event.data instanceof ArrayBuffer ? new Uint8Array(event.data) : new Uint8Array(Buffer.from(event.data));
          parser.push(data);
        };

        ws.onerror = () => {
          this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
        };

        ws.onclose = () => {
          if (state.peerName) {
            host.peers.unregister(state.peerName);
            state.peerCount = Math.max(0, state.peerCount - 1);
            this._updateStatus(worker, state, COMM_STATUS_IDLE, '');
          }
        };

        state.ws = ws;
      } else {
        this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
      }
    } catch (e) {
      this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
    }
  }

  _startServer(worker, host, state, port) {
    try {
      const http = require('http');
      const server = http.createServer((req, res) => {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end('Piolho WS Endpoint\n');
      });

      server.on('upgrade', (req, socket, head) => {
        const key = req.headers['sec-websocket-key'];
        if (!key) { socket.destroy(); return; }

        const crypto = require('crypto');
        const acceptKey = crypto.createHash('sha1')
          .update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')
          .digest('base64');

        socket.write(
          'HTTP/1.1 101 Switching Protocols\r\n' +
          'Upgrade: websocket\r\n' +
          'Connection: Upgrade\r\n' +
          'Sec-WebSocket-Accept: ' + acceptKey + '\r\n\r\n'
        );

        const parser = new PacketParser((pkt) => {
          this._handlePacket(worker, host, state, {
            send: (buf) => this._sendWsFrame(socket, buf),
            close: () => socket.destroy()
          }, pkt);
        });

        socket.on('data', (chunk) => {
          const unmasked = this._decodeWsFrame(chunk);
          if (unmasked) parser.push(unmasked);
        });

        socket.on('close', () => {
          if (state.peerName) {
            host.peers.unregister(state.peerName);
            state.peerCount = Math.max(0, state.peerCount - 1);
            this._updateStatus(worker, state, state.peerCount > 0 ? COMM_STATUS_CONNECTED : COMM_STATUS_LISTENING, '');
          }
        });
      });

      server.listen(port, () => {
        this._updateStatus(worker, state, COMM_STATUS_LISTENING, '');
      });

      server.on('error', () => {
        this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
      });

      state.httpServer = server;
    } catch (e) {
      this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
    }
  }

  _decodeWsFrame(buf) {
    if (buf.length < 2) return null;
    const secondByte = buf[1];
    const isMasked = (secondByte & 0x80) !== 0;
    let payloadLen = secondByte & 0x7F;
    let offset = 2;

    if (payloadLen === 126) {
      payloadLen = (buf[2] << 8) | buf[3];
      offset = 4;
    } else if (payloadLen === 127) {
      offset = 10;
    }

    let mask = null;
    if (isMasked) {
      mask = buf.slice(offset, offset + 4);
      offset += 4;
    }

    const payload = buf.slice(offset, offset + payloadLen);
    if (isMasked && mask) {
      for (let i = 0; i < payload.length; i++) {
        payload[i] ^= mask[i % 4];
      }
    }
    return payload;
  }

  _sendWsFrame(socket, data) {
    const len = data.length;
    let header;
    if (len < 126) {
      header = Buffer.from([0x82, len]);
    } else if (len <= 0xFFFF) {
      header = Buffer.from([0x82, 126, (len >> 8) & 0xFF, len & 0xFF]);
    } else {
      header = Buffer.alloc(10);
      header[0] = 0x82;
      header[1] = 127;
      header.writeBigUInt64BE(BigInt(len), 2);
    }
    try {
      socket.write(Buffer.concat([header, Buffer.from(data)]));
    } catch (e) {}
  }

  _handlePacket(worker, host, state, wsWrapper, pkt) {
    if (pkt.msgType === MSG_HELLO) {
      const ackPkt = encodePacket(MSG_HELLO_ACK, state.advertisedName || worker.name, pkt.sender, null);
      if (wsWrapper.send) wsWrapper.send(ackPkt);

      const remoteName = pkt.sender;
      state.peerName = remoteName;
      state.peerCount++;
      host.peers.registerRemote(remoteName, 'ws', wsWrapper, (s, data) => {
        if (wsWrapper.send) wsWrapper.send(encodePacket(MSG_TELL, s, remoteName, data));
      }, () => { if (wsWrapper.close) wsWrapper.close(); });

      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, remoteName);
    } else if (pkt.msgType === MSG_HELLO_ACK) {
      const remoteName = pkt.sender;
      state.peerName = remoteName;
      state.peerCount++;
      host.peers.registerRemote(remoteName, 'ws', wsWrapper, (s, data) => {
        if (wsWrapper.send) wsWrapper.send(encodePacket(MSG_TELL, s, remoteName, data));
      }, () => { if (wsWrapper.close) wsWrapper.close(); });

      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, remoteName);
    } else if (pkt.msgType === MSG_TELL) {
      host.ipc.receiveRemoteTell(pkt.sender, pkt.target, pkt.payload);
    }
  }

  _updateStatus(worker, state, statusCode, peerName) {
    state.status = statusCode;
    if (!worker.memory) return;
    const view = new DataView(worker.memory.buffer, state.ptr, STRUCT_SIZE);
    view.setInt32(136, statusCode, true);
    view.setInt32(140, state.peerCount, true);

    if (peerName) {
      const bytes = new Uint8Array(worker.memory.buffer, state.ptr, STRUCT_SIZE);
      const peerOffset = 176;
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
        if (state.ws) try { state.ws.close(); } catch (e) {}
        if (state.httpServer) try { state.httpServer.close(); } catch (e) {}
      }
    }
  }
}

const commWsExtension = new CommWsExtension();

module.exports = {
  CommWsExtension,
  commWsExtension
};
