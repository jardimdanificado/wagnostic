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

const path = require('path');
const fs = typeof require !== 'undefined' ? (function() { try { return require('fs'); } catch (e) { return null; } })() : null;

class ExtensionRegistry {
  constructor() {
    this.extensions = new Map();       // name -> Extension Object
    this.activeList = [];             // List of unique extension objects
    this.searchPaths = [];            // Directories to search for dynamic extensions
  }

  addSearchPath(dirPath) {
    if (!dirPath) return this;
    const resolved = path ? path.resolve(dirPath) : dirPath;
    if (!this.searchPaths.includes(resolved)) {
      this.searchPaths.push(resolved);
    }
    return this;
  }

  resolve(name) {
    if (this.extensions.has(name)) {
      return this.extensions.get(name);
    }

    const candidates = [name];
    if (name.includes(':')) {
      candidates.push(name.split(':').pop());
    }

    for (const searchDir of this.searchPaths) {
      for (const cand of candidates) {
        const filePath = path.join(searchDir, `${cand}.js`);
        try {
          if (fs && fs.existsSync(filePath)) {
            const stat = fs.statSync(filePath);
            if (stat.isFile()) {
              const mod = require(filePath);
              const ext = mod.extension || mod.default || mod;
              if (ext) {
                if (typeof ext.isSupported === 'function' && !ext.isSupported()) {
                  return null;
                }
                if (!ext.name) ext.name = name;
                this.register(ext);
                return ext;
              }
            }
          }
        } catch (e) {
          // Continue search on require error
        }
      }
    }

    return null;
  }

  register(ext) {
    if (typeof ext === 'function') {
      ext = { onRequest: ext };
    }

    if (typeof ext.isSupported === 'function' && !ext.isSupported()) {
      return this; // Skip unsupported extension in current environment
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
    return this.extensions.get(name) || this.resolve(name);
  }

  has(name) {
    return this.extensions.has(name) || this.resolve(name) !== null;
  }

  dispatch(worker, host, name) {
    let ext = this.extensions.get(name);
    if (!ext) {
      ext = this.resolve(name);
    }
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

  onPostStep(host) {
    for (const ext of this.activeList) {
      if (typeof ext.onPostStep === 'function') {
        ext.onPostStep(host);
      } else if (typeof ext.onStep === 'function') {
        ext.onStep(host);
      } else if (typeof ext.onFrameComplete === 'function') {
        ext.onFrameComplete(host);
      }
    }
  }

  onFrameComplete(host) {
    this.onPostStep(host);
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
