/**
 * Piolho Mesh Coordinator
 * 
 * Manages node lifecycles, ticks, and communication mesh.
 */

const { PiolhoNode } = require('./node');
const { RendezvousEngine } = require('./rendezvous');
const { OK, DONE, ANY } = require('./constants');
const path = require('path');

class PiolhoMesh {
  constructor(options = {}) {
    this.name = options.name || 'mesh';
    this.intervalMs = options.intervalMs !== undefined ? options.intervalMs : (options.fps ? 1000 / options.fps : 16);
    this.engine = new RendezvousEngine(this);
    this.nodes = [];
    this.nodeMap = new Map();
    this.isRunning = false;
    this.stepCount = 0;
    this.startTime = 0;
  }

  add(name, definition) {
    if (this.nodeMap.has(name)) {
      throw new Error(`Node '${name}' already exists in mesh.`);
    }

    const node = new PiolhoNode(name, definition, this);
    this.nodes.push(node);
    this.nodeMap.set(name, node);
    return node;
  }

  load(filePath, customName) {
    const fullPath = path.isAbsolute(filePath) ? filePath : path.resolve(process.cwd(), filePath);
    const mod = require(fullPath);
    const name = customName || mod.name || path.basename(filePath, '.js');
    return this.add(name, mod.default || mod);
  }

  step() {
    if (!this.isRunning) return false;
    this.stepCount++;

    let anyRunning = false;
    for (const node of this.nodes) {
      if (!node.running) continue;

      const status = node.update();
      if (status === DONE) {
        node.running = false;
      } else if (status < 0) {
        node.running = false;
      } else {
        anyRunning = true;
      }
    }

    if (!anyRunning) {
      this.cleanup();
      return false;
    }

    return true;
  }

  async run(options = {}) {
    const maxSteps = typeof options === 'number' ? options : (options.steps || options.ticks || 0);
    this.isRunning = true;
    this.startTime = Date.now();

    return new Promise((resolve) => {
      const loop = () => {
        const shouldContinue = this.step();
        if (!shouldContinue || (maxSteps > 0 && this.stepCount >= maxSteps)) {
          this.cleanup();
          resolve(0);
          return;
        }

        if (this.intervalMs === 0) {
          if (typeof setImmediate !== 'undefined') setImmediate(loop);
          else setTimeout(loop, 0);
        } else {
          setTimeout(loop, this.intervalMs);
        }
      };

      loop();
    });
  }

  cleanup() {
    if (!this.isRunning) return;
    this.isRunning = false;

    for (const node of this.nodes) {
      node.exit();
    }
    this.engine.clear();
  }
}

module.exports = { PiolhoMesh };
