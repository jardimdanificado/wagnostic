/**
 * comm:webtransport Extension
 * 
 * WebTransport (HTTP/3 QUIC) communication.
 * Checks environment before registering/dispatching.
 */

const COMM_STATUS_IDLE       = 0;
const COMM_STATUS_CONNECTING = 1;
const COMM_STATUS_CONNECTED  = 2;
const COMM_STATUS_ERROR     = -1;

const STRUCT_SIZE = 144;

class CommWebtransportExtension {
  constructor() {
    this.name = ['comm:webtransport', 'webtransport'];
  }

  isSupported() {
    return typeof WebTransport !== 'undefined' || typeof globalThis.WebTransport !== 'undefined';
  }

  onRequest(worker, host) {
    if (!this.isSupported()) return 0;

    if (!worker.extState.has('comm:webtransport')) {
      const ptr = worker.alloc(STRUCT_SIZE, 4);
      new Uint8Array(worker.memory.buffer, ptr, STRUCT_SIZE).fill(0);
      worker.extState.set('comm:webtransport', {
        ptr,
        wt: null,
        peerName: '',
        peerCount: 0,
        status: COMM_STATUS_IDLE
      });
    }
    return worker.extState.get('comm:webtransport').ptr;
  }

  onBeforeUpdate(worker, host) {}
  onDestroy(host) {}
}

const commWebtransportExtension = new CommWebtransportExtension();

module.exports = {
  CommWebtransportExtension,
  commWebtransportExtension
};
