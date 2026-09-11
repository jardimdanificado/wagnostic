/**
 * comm:udp Extension
 * 
 * Supports Active Probe, Passive Discoverable Listener, and Active Broadcast Beacon modes.
 */

const { encodePacket, PacketParser, MSG_HELLO, MSG_HELLO_ACK, MSG_TELL } = require('../wire');

const COMM_MODE_PROBE        = 0;
const COMM_MODE_LISTEN       = 1;
const COMM_MODE_BEACON       = 2;

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_LISTENING  = 3;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 144; // 64 (host) + 4 (port) + 4 (mode) + 4 (status) + 4 (peer_count) + 32 (adv_name) + 32 (peer_name)

class CommUdpExtension {
  constructor() {
    this.name = 'comm:udp';
  }

  onRequest(worker, host) {
    if (!worker.extState.has(this.name)) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set(this.name, {
        ptr,
        socket: null,
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

    const port = view.getInt32(64, true);
    const mode = view.getInt32(68, true);
    const status = view.getInt32(72, true);

    if (status === COMM_STATUS_CONNECTING && state.status !== COMM_STATUS_CONNECTING) {
      state.status = COMM_STATUS_CONNECTING;

      let hostStr = '';
      for (let i = 0; i < 64 && bytes[i] !== 0; i++) hostStr += String.fromCharCode(bytes[i]);
      if (!hostStr) hostStr = '127.0.0.1';

      let advName = '';
      for (let i = 0; i < 32 && bytes[80 + i] !== 0; i++) advName += String.fromCharCode(bytes[80 + i]);
      if (!advName) advName = worker.name;
      state.advertisedName = advName;

      if (mode === COMM_MODE_PROBE) {
        this._probeRemote(worker, host, state, hostStr, port);
      } else if (mode === COMM_MODE_BEACON) {
        this._startBeacon(worker, host, state, hostStr, port);
      } else {
        this._startListener(worker, host, state, port);
      }
    }
  }

  _probeRemote(worker, host, state, targetHost, port) {
    try {
      const dgram = require('dgram');
      const socket = dgram.createSocket('udp4');

      socket.on('message', (msg, rinfo) => {
        const parser = new PacketParser((pkt) => {
          this._handlePacket(worker, host, state, socket, rinfo.address, rinfo.port, pkt);
        });
        parser.push(new Uint8Array(msg));
      });

      socket.on('error', () => {
        this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
      });

      socket.bind(0, () => {
        const helloPkt = encodePacket(MSG_HELLO, state.advertisedName || worker.name, '', null);
        socket.send(helloPkt, port, targetHost);
      });

      state.socket = socket;
    } catch (e) {
      this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
    }
  }

  _startBeacon(worker, host, state, broadcastAddr, port) {
    try {
      const dgram = require('dgram');
      const socket = dgram.createSocket('udp4');

      socket.on('message', (msg, rinfo) => {
        const parser = new PacketParser((pkt) => {
          this._handlePacket(worker, host, state, socket, rinfo.address, rinfo.port, pkt);
        });
        parser.push(new Uint8Array(msg));
      });

      socket.bind(0, () => {
        socket.setBroadcast(true);
        const helloPkt = encodePacket(MSG_HELLO, state.advertisedName || worker.name, '', null);
        socket.send(helloPkt, port, broadcastAddr || '255.255.255.255');
        this._updateStatus(worker, state, COMM_STATUS_LISTENING, '');
      });

      state.socket = socket;
    } catch (e) {
      this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
    }
  }

  _startListener(worker, host, state, port) {
    try {
      const dgram = require('dgram');
      const socket = dgram.createSocket('udp4');

      socket.on('message', (msg, rinfo) => {
        const parser = new PacketParser((pkt) => {
          this._handlePacket(worker, host, state, socket, rinfo.address, rinfo.port, pkt);
        });
        parser.push(new Uint8Array(msg));
      });

      socket.bind(port, () => {
        this._updateStatus(worker, state, COMM_STATUS_LISTENING, '');
      });

      socket.on('error', () => {
        this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
      });

      state.socket = socket;
    } catch (e) {
      this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
    }
  }

  _handlePacket(worker, host, state, socket, rAddr, rPort, pkt) {
    if (pkt.msgType === MSG_HELLO) {
      const ackPkt = encodePacket(MSG_HELLO_ACK, state.advertisedName || worker.name, pkt.sender, null);
      socket.send(ackPkt, rPort, rAddr);

      const remoteName = pkt.sender;
      state.peerName = remoteName;
      state.peerCount++;
      host.peers.registerRemote(remoteName, 'udp', socket, (s, data) => {
        socket.send(encodePacket(MSG_TELL, s, remoteName, data), rPort, rAddr);
      }, () => socket.close());

      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, remoteName);
    } else if (pkt.msgType === MSG_HELLO_ACK) {
      const remoteName = pkt.sender;
      state.peerName = remoteName;
      state.peerCount++;
      host.peers.registerRemote(remoteName, 'udp', socket, (s, data) => {
        socket.send(encodePacket(MSG_TELL, s, remoteName, data), rPort, rAddr);
      }, () => socket.close());

      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, remoteName);
    } else if (pkt.msgType === MSG_TELL) {
      host.ipc.receiveRemoteTell(pkt.sender, pkt.target, pkt.payload);
    }
  }

  _updateStatus(worker, state, statusCode, peerName) {
    state.status = statusCode;
    if (!worker.memory) return;
    const view = new DataView(worker.memory.buffer, state.ptr, STRUCT_SIZE);
    view.setInt32(72, statusCode, true);
    view.setInt32(76, state.peerCount, true);

    if (peerName) {
      const bytes = new Uint8Array(worker.memory.buffer, state.ptr, STRUCT_SIZE);
      const peerOffset = 112;
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
      if (state && state.socket) {
        try { state.socket.close(); } catch (e) {}
      }
    }
  }
}

const commUdpExtension = new CommUdpExtension();

module.exports = {
  CommUdpExtension,
  commUdpExtension
};
