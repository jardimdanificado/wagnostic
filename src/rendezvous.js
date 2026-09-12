/**
 * Piolho Symmetric Zero-Queue Rendezvous Engine
 * 
 * Supports two-way rendezvous: matching occurs whether sender arrives first
 * or receiver arrives first (within timeout window).
 */

const { OK, TIMEOUT, ERROR_TARGET, ERROR_PARAM, ANY } = require('./constants');

class RendezvousEngine {
  constructor(mesh = null) {
    this.mesh = mesh;
    this.listenWaiters = []; // { node, caller, target }
    this.sayWaiters = [];    // { node, caller, target, data, expireTime }
    this.rrCursor = 0;
    this.onMessage = null;   // ({ sender, target, data, op }) => void
  }

  logMessage(sender, target, data, op) {
    if (typeof this.onMessage === 'function') {
      this.onMessage({ sender, target, data, op });
    }
  }

  listen(callerNode, targetName = ANY, timeout = 0) {
    const isAny = !targetName || targetName === ANY || targetName.length === 0;
    const callerName = callerNode.name;

    if (!isAny && targetName === callerName) {
      return { status: ERROR_PARAM, data: null };
    }

    // 1. Check if a matching say() is already waiting for us
    const now = Date.now();
    const sayIdx = this.sayWaiters.findIndex(s =>
      s.caller !== callerName &&
      (s.target === callerName || !s.target) &&
      (isAny || s.caller === targetName) &&
      (s.expireTime === 0 || s.expireTime >= now)
    );

    if (sayIdx !== -1) {
      const match = this.sayWaiters.splice(sayIdx, 1)[0];
      if (callerNode && typeof callerNode.onReceive === 'function') {
        callerNode.onReceive(match.caller, match.data);
      }
      this.logMessage(match.caller, callerName, match.data, 'rendezvous_say_first');
      return { status: OK, data: match.data };
    }

    if (timeout === 0) {
      return { status: TIMEOUT, data: null };
    }

    // 2. Register listen waiter for incoming say()
    const existingIdx = this.listenWaiters.findIndex(w => w.caller === callerName);
    const waiter = {
      node: callerNode,
      caller: callerName,
      target: isAny ? null : targetName
    };

    if (existingIdx !== -1) {
      this.listenWaiters[existingIdx] = waiter;
    } else {
      this.listenWaiters.push(waiter);
    }

    return { status: OK, waiter };
  }

  say(callerNode, targetName = ANY, data = null, timeout = 0, nodeMap = null) {
    const isAny = !targetName || targetName === ANY || targetName.length === 0;
    const callerName = callerNode.name;

    if (!isAny) {
      if (targetName === callerName) return ERROR_PARAM;
      if (nodeMap && !nodeMap.has(targetName)) return ERROR_TARGET;

      const targetPeer = nodeMap ? nodeMap.get(targetName) : null;
      if (targetPeer && targetPeer.isRemote) {
        this.logMessage(callerName, targetName, data, 'say_remote');
        targetPeer.send(callerName, data);
        return OK;
      }
    }

    // 1. Check if receiver is already in listenWaiters
    const len = this.listenWaiters.length;
    let matchIdx = -1;

    for (let i = 0; i < len; i++) {
      const idx = (this.rrCursor + i) % len;
      const w = this.listenWaiters[idx];

      if (w.caller !== callerName &&
          (!w.target || w.target === callerName) &&
          (isAny || w.caller === targetName)) {
        matchIdx = idx;
        this.rrCursor = (idx + 1) % (len || 1);
        break;
      }
    }

    if (matchIdx !== -1) {
      const match = this.listenWaiters.splice(matchIdx, 1)[0];
      if (match.node && typeof match.node.onReceive === 'function') {
        match.node.onReceive(callerName, data);
      }
      this.logMessage(callerName, match.caller, data, 'rendezvous_listen_first');
      return OK;
    }

    // 2. If timeout > 0, register in sayWaiters for receiver to pick up
    if (timeout > 0 || timeout === -1) {
      const expireTime = timeout > 0 ? Date.now() + timeout : 0;
      this.sayWaiters.push({
        node: callerNode,
        caller: callerName,
        target: isAny ? null : targetName,
        data,
        expireTime
      });
      return OK;
    }

    // Zero-queue timeout
    return TIMEOUT;
  }

  receiveRemote(senderName, targetName, data) {
    this.logMessage(senderName, targetName || '*', data, 'recv_remote');

    const matchIdx = this.listenWaiters.findIndex(
      w => (!w.target || w.target === senderName || w.target === targetName) &&
           (!targetName || w.caller === targetName || !w.target)
    );

    if (matchIdx !== -1) {
      const match = this.listenWaiters.splice(matchIdx, 1)[0];
      if (match.node && typeof match.node.onReceive === 'function') {
        match.node.onReceive(senderName, data);
      }
    } else {
      const targetNode = targetName && this.mesh ? this.mesh.nodeMap.get(targetName) : null;
      if (targetNode && typeof targetNode.onReceive === 'function') {
        targetNode.onReceive(senderName, data);
      } else if (this.mesh && this.mesh.nodes.length > 0) {
        for (const node of this.mesh.nodes) {
          if (node.running) {
            node.onReceive(senderName, data);
            break;
          }
        }
      }
    }
  }

  clear() {
    this.listenWaiters.length = 0;
    this.sayWaiters.length = 0;
  }
}

module.exports = { RendezvousEngine };
