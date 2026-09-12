/**
 * Piolho — Pure Minimalist Isomorphic Actor Mesh
 * 
 * 3 Primitives: update(), say(), listen()
 */

const { PiolhoMesh } = require('./mesh');
const { PiolhoNode } = require('./node');
const { RendezvousEngine } = require('./rendezvous');
const { NetworkBridge } = require('./network');
const constants = require('./constants');

function createMesh(options = {}) {
  const mesh = new PiolhoMesh(options);
  mesh.net = new NetworkBridge(mesh);
  return mesh;
}

const piolho = {
  create: createMesh,
  createMesh,
  Mesh: PiolhoMesh,
  Node: PiolhoNode,
  Engine: RendezvousEngine,
  ...constants
};

module.exports = {
  piolho,
  createMesh,
  Piolho: PiolhoMesh,
  PiolhoMesh,
  PiolhoNode,
  RendezvousEngine,
  NetworkBridge,
  ...constants
};
