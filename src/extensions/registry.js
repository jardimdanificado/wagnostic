/**
 * Piolho Extension Registry & Lifecycle Manager
 * 
 * Extensions can be registered as:
 * - Simple function: (worker, host, extName) => ptr
 * - Extension Object with lifecycle hooks:
 *   {
 *     name: string | string[],
 *     onRequest?(worker, host, extName): number,
 *     onBeforeUpdate?(worker, host): void,
 *     onAfterUpdate?(worker, host): void,
 *     onFrameComplete?(host): void,
 *     onDestroy?(host): void
 *   }
 */

class ExtensionRegistry {
  constructor() {
    this.extensions = new Map();       // name -> Extension Object
    this.activeList = [];             // List of unique extension objects
  }

  register(ext) {
    if (typeof ext === 'function') {
      ext = { onRequest: ext };
    }

    const names = Array.isArray(ext.name) ? ext.name : (ext.name ? [ext.name] : []);
    for (const name of names) {
      this.extensions.set(name, ext);
    }
    if (!this.activeList.includes(ext)) {
      this.activeList.push(ext);
    }
    return this;
  }

  get(name) {
    return this.extensions.get(name);
  }

  has(name) {
    return this.extensions.has(name);
  }

  dispatch(worker, host, name) {
    const ext = this.extensions.get(name);
    if (!ext) return 0;

    // Track that this worker activated this extension
    if (!worker.activeExtensions.includes(ext)) {
      worker.activeExtensions.push(ext);
    }

    if (typeof ext.onRequest === 'function') {
      return ext.onRequest(worker, host, name) || 0;
    }
    return 0;
  }

  onBeforeUpdate(worker, host) {
    for (const ext of worker.activeExtensions) {
      if (typeof ext.onBeforeUpdate === 'function') {
        ext.onBeforeUpdate(worker, host);
      }
    }
  }

  onAfterUpdate(worker, host) {
    for (const ext of worker.activeExtensions) {
      if (typeof ext.onAfterUpdate === 'function') {
        ext.onAfterUpdate(worker, host);
      }
    }
  }

  onFrameComplete(host) {
    for (const ext of this.activeList) {
      if (typeof ext.onFrameComplete === 'function') {
        ext.onFrameComplete(host);
      }
    }
  }

  onDestroy(host) {
    for (const ext of this.activeList) {
      if (typeof ext.onDestroy === 'function') {
        ext.onDestroy(host);
      }
    }
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { ExtensionRegistry };
}
