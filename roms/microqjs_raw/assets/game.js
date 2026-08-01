// Raw Wagnostic + MicroQuickJS demo

var t = 0;

function frame() {
    // Read window dimensions
    var w = wagnostic.width;
    var h = wagnostic.height;

    // Draw an animated XOR pattern
    for (var y = 0; y < h; y++) {
        for (var x = 0; x < w; x++) {
            var v = (x ^ y) + t;
            // set_pixel(x, y, r, g, b)
            set_pixel(x, y, v % 255, (v * 2) % 255, (v * 3) % 255);
        }
    }
    
    // Draw a small red square at mouse position
    var mx = wagnostic.mouse_x;
    var my = wagnostic.mouse_y;
    for (var dy = -5; dy <= 5; dy++) {
        for (var dx = -5; dx <= 5; dx++) {
            set_pixel(mx + dx, my + dy, 255, 0, 0);
        }
    }
    
    t += 2;
}
