const fs = require('fs');
const jsdom = require('jsdom');
const { JSDOM } = jsdom;
const html = fs.readFileSync('runners/web/index.html', 'utf8');
const script = fs.readFileSync('runners/web/runner.js', 'utf8');
const dom = new JSDOM(html, { runScripts: "dangerously", resources: "usable" });
dom.window.console.log = function(...args) { console.log('LOG:', ...args); };
dom.window.console.error = function(...args) { console.log('ERR:', ...args); };
dom.window.romData = new Uint8Array(fs.readFileSync('examples/audio_ogg.wasm'));
setTimeout(() => { 
    dom.window.eval(script);
    dom.window.eval(`
        wagnosticModule = new Wagnostic(romData);
        wagnosticModule.init().then(() => wagnosticModule.start()).catch(e => console.error(e));
    `);
}, 500);
setTimeout(() => { console.log('Done.'); process.exit(0); }, 3000);
