const fs = require('fs');
const wasm = fs.readFileSync('examples/audio_ogg.wasm');
const mem = new WebAssembly.Memory({ initial: 128 });
const env = {
    memory: mem,
    __stack_pointer: new WebAssembly.Global({value: "i32", mutable: true}, 8388608),
};
WebAssembly.instantiate(wasm, { env }).then(result => {
    const exports = result.instance.exports;
    console.log("Exports:", Object.keys(exports));
    console.log("Calling wupdate...");
    try {
        let ptr = exports.wupdate();
        console.log("wupdate returned:", ptr);
    } catch(e) {
        console.error("Exception:", e);
    }
}).catch(e => console.error(e));
