/**
 * Piolho Node Instance
 * 
 * Represents a discrete actor node executing in the mesh.
 */

const { OK, TIMEOUT, ANY } = require('./constants');

class PiolhoNode {
  constructor(name, definition, mesh) {
    this.name = name;
    this.mesh = mesh;
    this.running = true;
    this.stepCount = 0;
    this.inbox = [];
    this.lastSender = null;

    if (typeof definition === 'function') {
      this.updateFn = definition;
    } else if (definition && typeof definition.update === 'function') {
      this.updateFn = definition.update.bind(definition);
    } else {
      this.updateFn = null;
    }

    this.say = (target = ANY, data = null, timeout = 0) => {
      return this.mesh.engine.say(this, target, data, timeout, this.mesh.nodeMap);
    };

    this.listen = (target = ANY, timeout = 0) => {
      let receivedData = null;

      const msgIdx = this.inbox.findIndex(m => !target || target === ANY || m.sender === target);
      if (msgIdx !== -1) {
        const msg = this.inbox.splice(msgIdx, 1)[0];
        this.lastSender = msg.sender;
        receivedData = msg.data;
      }

      // Keep listener armed for next rendezvous if timeout is active
      if (timeout > 0 || timeout === -1) {
        this.mesh.engine.listen(this, target, timeout);
      }

      return receivedData;
    };
  }

  onReceive(sender, data) {
    this.inbox.push({ sender, data });
  }

  update() {
    if (!this.running || !this.updateFn) return OK;

    try {
      const result = this.updateFn({
        say: this.say,
        listen: this.listen,
        node: this,
        step: this.stepCount
      });

      this.stepCount++;
      return result !== undefined ? result : OK;
    } catch (err) {
      console.error(`[Node ${this.name}] update() exception:`, err);
      this.running = false;
      return -1;
    }
  }

  exit() {
    this.running = false;
  }
}

module.exports = { PiolhoNode };
