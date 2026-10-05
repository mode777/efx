efx.graphics.setClearColor([0.08, 0.09, 0.12, 1]);
// 4x4 checkerboard texture
const pixels = [];
for (let y = 0; y < 4; y++) {
    for (let x = 0; x < 4; x++) {
        const on = (x + y) % 2 === 0;
        pixels.push(on ? 255 : 30, 40, on ? 30 : 200, 255);
    }
}
const tex = efx.graphics.createTexture(
    efx.graphics.createImageData(4, 4, pixels));
function update() {}
function render() {
    efx.graphics.drawQuad(tex, 60, 60, { size: [160, 160] });              // full texture stretched
    efx.graphics.drawQuad(tex, 280, 60, {                                  // left half
        size: [160, 160],
        sourceRect: { x: 0, y: 0, w: 2, h: 4 },
    });
    efx.graphics.drawQuad(tex, 500, 60, {                                  // top-left quarter stretched
        size: [80, 160],
        sourceRect: { x: 0, y: 0, w: 1, h: 1 },
    });
}
