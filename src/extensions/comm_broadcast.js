/**
 * comm:broadcast Extension
 * 
 * Multi-instance rendezvous communication across tabs/workers/processes via BroadcastChannel.
 */

const { encodePacket, PacketParser, MSG_HELLO, MSG_HELLO_ACK, MSG_TELL } = require('../wire');

const COMM_MODE_JOIN        = 0;

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 144; // 64 (channel) + 4 (mode) + 4 (status) + 4 (peer_count) + 32 (adv_name) + 32 (peer_name) + 4 (pad)

class CommBroadcastExtension {
  constructor() {
    this.name = ['comm:broadcast', 'broadcast'];
  }

  isSupported() {
    return typeof BroadcastChannel !== 'undefined' || typeof globalThis.BroadcastChannel !== 'undefined';
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;

    if (!worker.extState.has('comm:broadcast')) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set('comm:broadcast', {
        ptr,
        channel: null,
        peerName: '',
        peerCount: 0,
        status: COMM_STATUS_IDLE
      });
    }
    return worker.extState.get('comm:broadcast').ptr;
  }

  onBeforeUpdate(worker, host) {
    if (!this.isSupported()) return;
    const state = worker.extState.get('comm:broadcast');
    if (!state || !worker.memory) return;

    const view = new DataView(worker.memory.buffer, state.ptr, STRUCT_SIZE);
    const bytes = new Uint8Array(worker.memory.buffer, state.ptr, STRUCT_SIZE);

    const mode = view.getInt32(64, true);
    const status = view.getInt32(68, true);

    if (status === COMM_STATUS_CONNECTING && state.status !== COMM_STATUS_CONNECTING) {
      state.status = COMM_STATUS_CONNECTING;

      let channelName = '';
      for (let i = 0; i < 64 && bytes[i] !== 0; i++) channelName += String.fromCharCode(bytes[i]);
      if (!channelName) channelName = 'piolho_bus';

      let advName = '';
      for (let i = 0; i < 32 && bytes[76 + i] !== 0; i++) advName += String.fromCharCode(bytes[76 + i]);
      if (!advName) advName = worker.name;
      state.advertisedName = advName;

      this._joinChannel(worker, host, state, channelName);
    }
  }

  _joinChannel(worker, host, state, channelName) {
    try {
      const BC = typeof BroadcastChannel !== 'undefined' ? BroadcastChannel : globalThis.BroadcastChannel;
      const channel = new BC(channelName);

      const parser = new PacketParser((pkt) => {
        this._handlePacket(worker, host, state, channel, pkt);
      });

      channel.onmessage = (event) => {
        if (event.data instanceof Uint8Array) {
          parser.push(event.data);
        } else if (event.data instanceof ArrayBuffer) {
          parser.push(new Uint8Array(event.data));
        } else if (typeof event.data === 'string') {
          parser.push(new TextEncoder().encode(event.data));
        }
      };

      // Broadcast HELLO to announce our presence on the bus
      const helloPkt = encodePacket(MSG_HELLO, state.advertisedName || worker.name, '', null);
      channel.postMessage(helloPkt);

      state.channel = channel;
      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, '');
    } catch (e) {
      this._updateStatus(worker, state, COMM_STATUS_ERROR, '');
    }
  }

  _handlePacket(worker, host, state, channel, pkt) {
    if (pkt.sender === (state.advertisedName || worker.name)) {
      return; // Ignore own messages
    }

    if (pkt.msgType === MSG_HELLO) {
      const ackPkt = encodePacket(MSG_HELLO_ACK, state.advertisedName || worker.name, pkt.sender, null);
      channel.postMessage(ackPkt);

      const remoteName = pkt.sender;
      state.peerName = remoteName;
      state.peerCount++;
      host.peers.registerRemote(remoteName, 'broadcast', channel, (s, data) => {
        channel.postMessage(encodePacket(MSG_TELL, s, remoteName, data));
      }, () => channel.close());

      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, remoteName);
    } else if (pkt.msgType === MSG_HELLO_ACK) {
      const remoteName = pkt.sender;
      state.peerName = remoteName;
      state.peerCount++;
      host.peers.registerRemote(remoteName, 'broadcast', channel, (s, data) => {
        channel.postMessage(encodePacket(MSG_TELL, s, remoteName, data));
      }, () => channel.close());

      this._updateStatus(worker, state, COMM_STATUS_CONNECTED, remoteName);
    } else if (pkt.msgType === MSG_TELL) {
      host.ipc.receiveRemoteTell(pkt.sender, pkt.target, pkt.payload);
    }
  }

  _updateStatus(worker, state, statusCode, peerName) {
    state.status = statusCode;
    if (!worker.memory) return;
    const view = new DataView(worker.memory.buffer, state.ptr, STRUCT_SIZE);
    view.setInt32(68, statusCode, true);
    view.setInt32(72, state.peerCount, true);

    if (peerName) {
      const bytes = new Uint8Array(worker.memory.buffer, state.ptr, STRUCT_SIZE);
      const peerOffset = 108;
      for (let i = 0; i < 32; i++) {
        bytes[peerOffset + i] = i < peerName.length ? peerName.charCodeAt(i) : 0;
      }
    }
  }

  onDestroy(host) {
    for (const w of host.workers) {
      const state = w.extState.get('comm:broadcast');
      if (state && state.channel) {
        try { state.channel.close(); } catch (e) {}
      }
    }
  }
}

const commBroadcastExtension = new CommBroadcastExtension();

module.exports = {
  CommBroadcastExtension,
  commBroadcastExtension
};
