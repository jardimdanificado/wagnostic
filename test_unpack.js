const fs = require('fs');

function unpackPixelsToImageData(bpp, px, rb, rs, gb, gs, bb, bs, ab, ash, isGrayscale) {
    let val = (bpp === 64) ? BigInt(px) : Number(px);
    let r = 0, g = 0, b = 0, a = 255;
    if (isGrayscale) {
        let lum = 0;
        if (bpp === 64) lum = Number((val >> BigInt(ash)) & ((1n << BigInt(ab)) - 1n));
        else lum = (val >> ash) & ((1 << ab) - 1);
        lum = (lum * 255 / ((1 << ab) - 1)) | 0;
        r = g = b = lum;
        a = 255;
    } else {
        if (bpp === 64) {
            // ...
        } else {
            if (rb) r = ((val >> rs) & ((1 << rb) - 1)) * 255 / ((1 << rb) - 1) | 0;
            if (gb) g = ((val >> gs) & ((1 << gb) - 1)) * 255 / ((1 << gb) - 1) | 0;
            if (bb) b = ((val >> bs) & ((1 << bb) - 1)) * 255 / ((1 << bb) - 1) | 0;
            if (ab) a = ((val >> ash) & ((1 << ab) - 1)) * 255 / ((1 << ab) - 1) | 0;
        }
    }
    console.log({px, r, g, b, a});
    let packed = (a << 24) | (b << 16) | (g << 8) | r;
    console.log("packed:", packed >>> 0); // show as unsigned
    
    // Simulate what happens in Uint32Array on little endian
    let arr = new Uint32Array(1);
    arr[0] = packed;
    let u8 = new Uint8Array(arr.buffer);
    console.log("u8:", u8[0], u8[1], u8[2], u8[3]); // R, G, B, A
}

// test display_test RGB565: r=5/s=11, g=6/s=5, b=5/s=0
// pure red: r=31
let px_red = (31 << 11);
unpackPixelsToImageData(16, px_red, 5, 11, 6, 5, 5, 0, 0, 0, false);
