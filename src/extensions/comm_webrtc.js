/**
 * comm:webrtc Extension
 * 
 * WebRTC DataChannel communication.
 * Checks environment before registering/dispatching.
 */

const { encodePacket, PacketParser, MSG_HELLO, MSG_HELLO_ACK, MSG_TELL } = require('../wire');

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 144;

class CommWebrtcExtension {
  constructor() {
    this.name = ['comm:webrtc', 'webrtc'];
  }

  isSupported() {
    return typeof RTCPeerConnection !== 'undefined' || typeof globalThis.RTCPeerConnection !== 'undefined';
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;

    if (!worker.extState.has('comm:webrtc')) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set('comm:webrtc', {
        ptr,
        pc: null,
        dc: null,
        peerName: '',
        peerCount: 0,
        status: COMM_STATUS_IDLE
      });
    }
    return worker.extState.get('comm:webrtc').ptr;
  }

  onBeforeUpdate(worker, host) {}
  onDestroy(host) {}
}

const commWebrtcExtension = new CommWebrtcExtension();

module.exports = {
  CommWebrtcExtension,
  commWebrtcExtension
};
