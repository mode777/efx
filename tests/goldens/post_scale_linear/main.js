// F5b golden: render scale 0.5 with a linear blit — the upscaled output is
// smoothly interpolated (differs deterministically from nearest).
efx.setClearColor([0.02, 0.03, 0.06, 1]);
efx.setCamera2D({ frame: [640, 480] });
const tex = efx.createTexture(efx.createImageData({
    width: 2, height: 2,
    pixels: [
        230, 40, 40, 255,   40, 210, 80, 255,
        40, 80, 230, 255,   240, 220, 60, 255,
    ],
}));
efx.setRenderScale(0.5, { filter: 'linear' });
function update() {}
function render() {
    efx.drawQuad(0, 0, tex, { size: [640, 480] });
    efx.drawQuad(120, 110, efx.whiteTexture, { size: [170, 70], color: [1, 1, 1, 1] });
    efx.drawQuad(350, 150, tex, { size: [150, 150], color: [0.9, 0.35, 0.2, 1], rotation: 18 });
    efx.drawQuad(180, 300, tex, { size: [130, 130], color: [0.2, 0.7, 0.95, 1], rotation: -12 });
    efx.drawQuad(430, 330, tex, { size: [110, 110], color: [0.6, 0.9, 0.3, 1] });
}
