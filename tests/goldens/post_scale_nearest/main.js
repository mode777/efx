// F5b golden: render scale 0.5 with a nearest blit — each scene pixel
// becomes a crisp 2x2 block on the surface (no interpolation).
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });
const tex = efx.graphics.createTexture(efx.graphics.createImageData({
    width: 2, height: 2,
    pixels: [
        230, 40, 40, 255,   40, 210, 80, 255,
        40, 80, 230, 255,   240, 220, 60, 255,
    ],
}));
efx.graphics.setRenderScale(0.5, { filter: 'nearest' });
function update() {}
function render() {
    efx.graphics.drawQuad(0, 0, tex, { size: [640, 480] });
    efx.graphics.drawQuad(120, 110, efx.whiteTexture, { size: [170, 70], color: [1, 1, 1, 1] });
    efx.graphics.drawQuad(350, 150, tex, { size: [150, 150], color: [0.9, 0.35, 0.2, 1], rotation: 18 });
    efx.graphics.drawQuad(180, 300, tex, { size: [130, 130], color: [0.2, 0.7, 0.95, 1], rotation: -12 });
    efx.graphics.drawQuad(430, 330, tex, { size: [110, 110], color: [0.6, 0.9, 0.3, 1] });
}
