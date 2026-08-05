// mquickjs_wagner demo - using Wagner framework functions in JS!

var angle = 0;
var particles = [];

function setup() {
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
    // Clear background with dark blue
    fill(15, 15, 35);
    clear();
    
    // Draw animated rotating shapes using Wagner push/pop matrix transforms
    push();
    translate(160, 100);
    rotate(angle);
    
    // Outer square using Wagner rect
    fill(RED);
    rect(-40, -40, 80, 80);
    
    // Inner rotating triangle using Wagner triangle_pts
    push();
    rotate(-angle * 2);
    fill(YELLOW);
    triangle_pts(-25, 20, 25, 20, 0, -30);
    pop();
    
    pop();
    
    // Update and draw particles using Wagner circle
    fill(CYAN);
    for (var i = 0; i < particles.length; i++) {
        var p = particles[i];
        p.y += p.speed;
        if (p.y > 240) p.y = 0;
        circle(p.x, p.y, p.size);
    }
    
    // Draw text using Wagner text function
    fill(WHITE);
    text("WAGNER + MQUICKJS", 85, 12);
    
    var infoStr = "MOUSE X:" + wagnostic.mouse_x + " Y:" + wagnostic.mouse_y;
    text(infoStr, 10, 220);
    
    // Draw mouse target cursor using Wagner circle and lines
    push();
    translate(wagnostic.mouse_x, wagnostic.mouse_y);
    stroke(MAGENTA);
    no_fill();
    circle(0, 0, 12);
    line(-16, 0, 16, 0);
    line(0, -16, 0, 16);
    pop();
    
    // Sound feedback on mouse down
    if (wagnostic.mouse_down) {
        play_tone(440.0 + (wagnostic.mouse_x % 300), 0.05, 0.3);
    }
    
    angle += 0.04;
}
