// Eight textured quads bouncing inside a 640x480 virtual frame.
efx.graphics.setClearColor([0.05, 0.03, 0.10, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const pixels = [];
for (let i = 0; i < 8 * 8; i++) { pixels.push(255, 255, 255, 255); }
const tex = efx.graphics.createTexture(efx.graphics.createImageData(8, 8, pixels));

const dots = [];
for (let i = 0; i < 8; i++) {
    dots.push({
        x: 40 + i * 70, y: 60 + i * 35,
        vx: 40 + i * 7, vy: 55 + i * 5,
        c: [0.3 + 0.08 * i, 0.8 - 0.06 * i, 0.9, 1],
    });
}

function update(dt) {
    for (const d of dots) {
        d.x += d.vx * dt;
        d.y += d.vy * dt;
        if (d.x < 0 || d.x > 640 - 48) { d.vx *= -1; }
        if (d.y < 0 || d.y > 480 - 48) { d.vy *= -1; }
    }
}

function render() {
    for (const d of dots) {
        efx.graphics.drawQuad(tex, d.x, d.y, { size: [48, 48], color: d.c });
    }
}
