import { WASI, File, Directory, OpenFile, ConsoleStdout, PreopenDirectory } from "https://unpkg.com/@bjorn3/browser_wasi_shim@0.3.0/dist/index.js";

self.onmessage = async (e) => {
    if (e.data.type !== 'compile') return;
    const sourceCode = e.data.code;
    
    try {
        self.postMessage({ type: 'log', data: 'Fetching sysroot...' });
        const sysrootRes = await fetch('sysroot.json');
        const sysrootFiles = await sysrootRes.json();
        
        // Combine all necessary C files into a single translation unit
        let combinedC = "";
        

        

        
        // Add user code
        combinedC += sourceCode;
        
        // Setup WASI filesystem
        // xcc will compile /project/combined.c to /project/main.wasm
        // We need to populate the sysroot files in the WASI memory FS
        // browser_wasi_shim doesn't easily let us recursively build nested maps on the fly, 
        // but we can just use a flat list or simple structure.
        // Actually, let's just dump ALL sysroot headers directly into /project flatly, and adjust the includes?
        // No, we can just build the nested structure!
        
        function buildTree(pathsAndContents) {
            let root = new Map();
            for (const [path, content] of Object.entries(pathsAndContents)) {
                const parts = path.split('/').filter(p => p);
                let current = root;
                for (let i = 0; i < parts.length - 1; i++) {
                    if (!current.has(parts[i])) {
                        current.set(parts[i], new Map());
                    }
                    current = current.get(parts[i]);
                }
                current.set(parts[parts.length - 1], new File(new Uint8Array(content)));
            }
            
            function toDirOrFile(map) {
                let entries = [];
                for (const [k, v] of map.entries()) {
                    if (v instanceof Map) {
                        entries.push([k, toDirOrFile(v)]);
                    } else {
                        entries.push([k, v]);
                    }
                }
                return new PreopenDirectory(".", entries).dir; // returns Directory
            }
            return toDirOrFile(root).contents;
        }
        
        const rootContents = buildTree(sysrootFiles);
        // Add tmp dir
        rootContents.set("tmp", new Directory(new Map()));
        // Add combined.c
        rootContents.set("combined.c", new File(new TextEncoder().encode(combinedC)));
        
        let rootDirectory = new PreopenDirectory("/", Array.from(rootContents.entries()));

        let args = [
            "cc",
            "-o", "/main.wasm",
            "-nostdlib",
            "-nodefaultlibs",
            "-I/sysroot/wagner/include",
            "-I/sysroot/wagner/lib/include",
            "-I/sysroot/wagner/lib/decoders",
            "-I/sysroot/include",
            "-D", "WAGNER_TITLE=\"web\"",
            "-D", "WAGNER_CFG_W=320",
            "-D", "WAGNER_CFG_H=240",
            "-D", "WAGNER_CFG_BPP=32",
            "-D", "WAGNER_CFG_SCALE=1",
            "--entry-point=",
            "-ewupdate",
            "--stack-size=16777216",
            "/combined.c"
        ];
        
        self.postMessage({ type: 'log', data: 'Compiling with xcc: ' + args.join(' ') });

        let env = ["PWD=/"];
        let fds = [
            new OpenFile(new File([])), // stdin
            ConsoleStdout.lineBuffered(msg => self.postMessage({ type: 'log', data: '[STDOUT] ' + msg })),
            ConsoleStdout.lineBuffered(msg => self.postMessage({ type: 'log', data: '[STDERR] ' + msg })),
            rootDirectory
        ];
        
        let wasi = new WASI(args, env, fds);
        
        self.postMessage({ type: 'log', data: 'Loading xcc.wasm...' });
        let wasmRes = await fetch('cc.wasm');
        let wasmBytes = await wasmRes.arrayBuffer();
        let inst = await WebAssembly.instantiate(wasmBytes, {
            "wasi_snapshot_preview1": wasi.wasiImport,
        });
        
        self.postMessage({ type: 'log', data: 'Running compiler...' });
        let exitCode = wasi.start(inst.instance);
        self.postMessage({ type: 'log', data: 'Exit code: ' + exitCode });
        
        if (exitCode !== 0) {
            self.postMessage({ type: 'error', data: 'Compilation failed (code ' + exitCode + ')' });
            return;
        }
        
        let outFile = rootDirectory.dir.contents.get("main.wasm");
        if (!outFile || !outFile.data) {
            self.postMessage({ type: 'error', data: 'main.wasm was not generated!' });
            return;
        }
        
        self.postMessage({ type: 'log', data: 'ROM generated: ' + outFile.data.byteLength + ' bytes' });
        self.postMessage({ type: 'done', data: outFile.data });
        
    } catch (err) {
        self.postMessage({ type: 'error', data: err.toString() + '\n' + (err.stack || '') });
    }
};
