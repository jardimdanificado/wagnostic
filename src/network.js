/**
 * Piolho Network Transport Bridges
 * 
 * Interconnects distributed Piolho meshes across processes, servers, and browsers.
 */

const net = require('net');

class RemotePeer {
  constructor(name, transportType, rawSocket, sendFn, closeFn) {
    this.name = name;
    this.isRemote = true;
    this.transportType = transportType;
    this.rawSocket = rawSocket;
    this.sendFn = sendFn;
    this.closeFn = closeFn;
  }

  send(sender, data) {
    if (typeof this.sendFn === 'function') {
      this.sendFn(sender, data);
    }
  }

  close() {
    if (typeof this.closeFn === 'function') {
      this.closeFn();
    }
  }
}

class NetworkBridge {
  constructor(mesh) {
    this.mesh = mesh;
    this.servers = [];
    this.peers = new Map();
  }

  registerRemote(peerName, transportType, socket, sendFn, closeFn) {
    const peer = new RemotePeer(peerName, transportType, socket, sendFn, closeFn);
    this.peers.set(peerName, peer);
    this.mesh.nodeMap.set(peerName, peer);
    return peer;
  }

  async listenTCP(port, localAlias = 'hub') {
    return new Promise((resolve) => {
      const server = net.createServer((socket) => {
        let remoteAlias = '';
        let buf = '';

        socket.on('data', (chunk) => {
          buf += chunk.toString();
          const lines = buf.split('\n');
          buf = lines.pop();

          for (const line of lines) {
            if (!line.trim()) continue;
            try {
              const msg = JSON.parse(line);
              if (msg.type === 'hello') {
                remoteAlias = msg.sender;
                this.registerRemote(remoteAlias, 'tcp', socket, (s, data) => {
                  socket.write(JSON.stringify({ type: 'msg', sender: s, target: remoteAlias, data }) + '\n');
                }, () => socket.destroy());
                socket.write(JSON.stringify({ type: 'hello_ack', sender: localAlias }) + '\n');
              } else if (msg.type === 'msg') {
                this.mesh.engine.receiveRemote(msg.sender, msg.target, msg.data);
              }
            } catch (e) {}
          }
        });
      });

      server.listen(port, () => {
        this.servers.push(server);
        resolve(server);
      });
    });
  }

  async connectTCP(host, port, localAlias = 'client') {
    return new Promise((resolve) => {
      const socket = net.createConnection({ host, port }, () => {
        socket.write(JSON.stringify({ type: 'hello', sender: localAlias }) + '\n');
      });

      let remoteAlias = '';
      let buf = '';

      socket.on('data', (chunk) => {
        buf += chunk.toString();
        const lines = buf.split('\n');
        buf = lines.pop();

        for (const line of lines) {
          if (!line.trim()) continue;
          try {
            const msg = JSON.parse(line);
            if (msg.type === 'hello_ack') {
              remoteAlias = msg.sender;
              const peer = this.registerRemote(remoteAlias, 'tcp', socket, (s, data) => {
                socket.write(JSON.stringify({ type: 'msg', sender: s, target: remoteAlias, data }) + '\n');
              }, () => socket.destroy());
              resolve(peer);
            } else if (msg.type === 'msg') {
              this.mesh.engine.receiveRemote(msg.sender, msg.target, msg.data);
            }
          } catch (e) {}
        }
      });
    });
  }

  cleanup() {
    for (const s of this.servers) {
      try { s.close(); } catch (e) {}
    }
    for (const p of this.peers.values()) {
      p.close();
    }
    this.peers.clear();
  }
}

module.exports = { NetworkBridge, RemotePeer };
