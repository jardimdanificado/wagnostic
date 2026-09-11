/**
 * Piolho Environment Abstraction Layer
 * Supports: txiki.js (tjs), Node.js, Bun, Deno
 */

const isTxiki = typeof tjs !== 'undefined' || typeof globalThis.tjs !== 'undefined';
const isDeno = typeof Deno !== 'undefined';
const isBun = typeof Bun !== 'undefined';
const isNode = typeof process !== 'undefined' && process.versions && process.versions.node && !isBun && !isDeno;

const ENV = {
  isTxiki,
  isNode,
  isBun,
  isDeno,

  argv: isNode || isBun
    ? process.argv.slice(2)
    : (isDeno
      ? Deno.args
      : (typeof tjs !== 'undefined' && tjs.args ? tjs.args.slice(1) : [])),

  cwd: isNode || isBun
    ? process.cwd()
    : (isDeno
      ? Deno.cwd()
      : (typeof tjs !== 'undefined' && tjs.cwd ? tjs.cwd() : '.')),

  exit: (code = 0) => {
    if (isNode || isBun) process.exit(code);
    else if (isDeno) Deno.exit(code);
    else if (typeof tjs !== 'undefined' && tjs.exit) tjs.exit(code);
  },

  stdoutWrite: (str) => {
    if (isNode || isBun) process.stdout.write(str);
    else if (isDeno) Deno.stdout.writeSync(new TextEncoder().encode(str));
    else if (typeof tjs !== 'undefined' && tjs.stdout) tjs.stdout.write(new TextEncoder().encode(str));
    else console.log(str);
  },

  readFile: (filePath) => {
    if (isNode || isBun) {
      const fs = require('fs');
      return fs.readFileSync(filePath);
    } else if (isDeno) {
      return Deno.readFileSync(filePath);
    } else if (typeof tjs !== 'undefined') {
      const f = tjs.open(filePath, 'r');
      const stat = f.stat();
      const buf = new Uint8Array(stat.size);
      f.read(buf);
      f.close();
      return buf;
    }
    throw new Error('Unsupported runtime for file reading');
  },

  writeFile: (filePath, data) => {
    if (isNode || isBun) {
      const fs = require('fs');
      fs.writeFileSync(filePath, data);
    } else if (isDeno) {
      Deno.writeFileSync(filePath, data);
    } else if (typeof tjs !== 'undefined') {
      const f = tjs.open(filePath, 'w');
      f.write(data);
      f.close();
    }
  },

  onSignal: (signal, handler) => {
    if (isNode || isBun) {
      process.on(signal, handler);
    } else if (isDeno) {
      try { Deno.addSignalListener(signal, handler); } catch (e) {}
    }
  }
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { ENV, isTxiki, isNode, isBun, isDeno };
}
