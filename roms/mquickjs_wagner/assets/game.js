// mquickjs_wagner GIF Demo — Load 3-frame animated GIF and render each frame separately!

var gifFrames = null;
var staticGif = null;

function setup() {
    set_title("Wagner JS — GIF Demo");

    // 1. Load GIF as animated frames array
    gifFrames = load_gif_anim("sample.gif");

    // 2. Load GIF as static single image (first frame)
    staticGif = load_image("sample.gif");
}

function draw() {
    fill(20, 20, 35);
    clear();

    fill(WHITE);
    text("GIF DECODER DEMO — 3 FRAMES SEPARATE", 15, 12);

    // --- Render each of the 3 GIF frames side-by-side ---
    if (gifFrames && gifFrames.length >= 3) {
        // Frame 0
        fill(CYAN);
        text("FRAME 0", 25, 32);
        image(gifFrames[0], 15, 45, 85, 65);

        // Frame 1
        fill(YELLOW);
        text("FRAME 1", 125, 32);
        image(gifFrames[1], 115, 45, 85, 65);

        // Frame 2
        fill(GREEN);
        text("FRAME 2", 225, 32);
        image(gifFrames[2], 215, 45, 85, 65);

        // --- Render animated playback loop in center ---
        var animIndex = Math.floor(wagnostic.frame_count / 15) % gifFrames.length;
        fill(WHITE);
        text("PLAYBACK LOOP (FRAME " + animIndex + ")", 55, 122);
        image(gifFrames[animIndex], 100, 135, 120, 90);
    } else if (staticGif) {
        // Fallback: draw static image
        text("STATIC GIF LOADED", 90, 100);
        image(staticGif, 100, 115, 120, 90);
    }
}
