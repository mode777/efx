const tex = efx.graphics.createTexture(
    efx.graphics.createImageData(2, 2, [255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255]));
efx.graphics.setClearColor([0.05, 0.05, 0.1, 1]);
efx.graphics.setCamera2D({ frame: [640, 480], x: 320, y: 240, zoom: 1.5, rotation: 20 });
function update() {}
function render() {
    for (let gy = 0; gy < 3; gy++) {
        for (let gx = 0; gx < 3; gx++) {
            efx.graphics.drawQuad(tex, 200 + gx * 100, 140 + gy * 80, {
                size: [60, 40],
                color: [0.2 + gx * 0.25, 0.3 + gy * 0.2, 0.8 - gx * 0.2, 1],
            });
        }
    }
}
