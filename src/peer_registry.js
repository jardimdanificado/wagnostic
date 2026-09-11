/**
 * Piolho General Peer & Instance Registry
 * 
 * Tracks all local workers and discovered/connected remote peers (TCP, Pipe, WS, UDP).
 */

class RemotePeer {
  constructor(name, transportType, transportHandle, sendFn, closeFn) {
    this.name = name;
    this.isRemote = true;
    this.transportType = transportType; // 'tcp' | 'pipe' | 'ws' | 'udp'
    this.transportHandle = transportHandle;
    this.sendFn = sendFn;
    this.closeFn = closeFn;
  }

  sendTell(senderName, payloadBytes) {
    if (this.sendFn) {
      this.sendFn(senderName, payloadBytes);
    }
  }

  close() {
    if (this.closeFn) {
      this.closeFn();
    }
  }
}

class PeerRegistry {
  constructor(host) {
    this.host = host;
    this.peers = new Map(); // name -> WWorker | RemotePeer
  }

  registerLocal(worker) {
    this.peers.set(worker.name, worker);
  }

  registerRemote(name, transportType, transportHandle, sendFn, closeFn) {
    const peer = new RemotePeer(name, transportType, transportHandle, sendFn, closeFn);
    this.peers.set(name, peer);
    // Also mirror to host.workerMap so lookups by name work transparently
    this.host.workerMap.set(name, peer);
    return peer;
  }

  unregister(name) {
    const peer = this.peers.get(name);
    if (peer && peer.isRemote) {
      peer.close();
    }
    this.peers.delete(name);
    this.host.workerMap.delete(name);
  }

  get(name) {
    return this.peers.get(name);
  }

  has(name) {
    return this.peers.has(name);
  }

  clear() {
    for (const [name, peer] of this.peers.entries()) {
      if (peer.isRemote) {
        peer.close();
      }
    }
    this.peers.clear();
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { PeerRegistry, RemotePeer };
}
