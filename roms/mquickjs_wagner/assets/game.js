// mquickjs_wagner complete demo — Textures, Audio Files, Keyboard & Mouse Events!

var angle = 0;
var playerX = 160;
var playerY = 180;
var particles = [];
var iconImg = null;
var beepSnd = null;

function setup() {
    set_title("Wagner JS — Demo");

    // Load PNG image asset
    iconImg = load_image("icon.png");

    for (var i = 0; i < 20; i++) {
        particles.push({
            x: (i * 16 + 10) % 320,
            y: (i * 23 + 15) % 240,
            speed: 1 + (i % 3),
            size: 4 + (i % 6)
        });
    }
}

function draw() {
    // 1. Keyboard Controls (WASD or Arrow Keys)
    var speed = 3;
    if (is_key_down(KEY_LEFT) || is_key_down(KEY_A)) playerX -= speed;
    if (is_key_down(KEY_RIGHT) || is_key_down(KEY_D)) playerX += speed;
    if (is_key_down(KEY_UP) || is_key_down(KEY_W)) playerY -= speed;
    if (is_key_down(KEY_DOWN) || is_key_down(KEY_S)) playerY += speed;

    // Boundary wrap
    if (playerX < 0) playerX = 320;
    if (playerX > 320) playerX = 0;
    if (playerY < 0) playerY = 240;
    if (playerY > 240) playerY = 0;

    // Clear background
    fill(15, 15, 35);
    clear();

    // Draw animated rotating textured shape using Wagner push/pop matrix transforms
    push();
    translate(160, 80);
    rotate(angle);

    if (iconImg) {
        texture(iconImg);
        rect(-32, -32, 64, 64);
        no_texture();
    } else {
        fill(RED);
        rect(-30, -30, 60, 60);
    }

    // Inner rotating triangle
    push();
    rotate(-angle * 2);
    fill(YELLOW);
    triangle_pts(-20, 15, 20, 15, 0, -25);
    pop();

    pop();

    // Update and draw background particles
    fill(CYAN);
    for (var i = 0; i < particles.length; i++) {
        var p = particles[i];
        p.y += p.speed;
        if (p.y > 240) p.y = 0;
        circle(p.x, p.y, p.size);
    }

    // Draw Keyboard-Controlled Player with PNG texture
    push();
    if (iconImg) {
        image(iconImg, playerX - 16, playerY - 16, 32, 32);
    } else {
        translate(playerX, playerY);
        fill(GREEN);
        circle(0, 0, 14);
    }
    pop();

    // Draw HUD text
    fill(WHITE);
    text("WAGNER + MQUICKJS COMPLETE API", 45, 10);
    text("POS X:" + Math.floor(playerX) + " Y:" + Math.floor(playerY), 10, 220);

    // Draw mouse target cursor
    push();
    translate(wagnostic.mouse_x, wagnostic.mouse_y);
    stroke(wagnostic.mouse_down ? RED : MAGENTA);
    no_fill();
    circle(0, 0, 10);
    line(-12, 0, 12, 0);
    line(0, -12, 0, 12);
    pop();

    angle += 0.04;
}
