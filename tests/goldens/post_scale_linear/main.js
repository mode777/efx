// F5b golden: render scale 0.5 with a linear blit — the upscaled output is
// smoothly interpolated (differs deterministically from nearest).
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });
const tex = efx.graphics.createTexture(efx.graphics.createImageData(2, 2, [
        230, 40, 40, 255,   40, 210, 80, 255,
        40, 80, 230, 255,   240, 220, 60, 255,
    ]));
efx.graphics.setRenderScale(0.5, { filter: 'linear' });
function update() {}
function render() {
    efx.graphics.drawQuad(tex, 0, 0, { size: [640, 480] });
    efx.graphics.drawQuad(efx.graphics.whiteTexture, 120, 110, { size: [170, 70], color: [1, 1, 1, 1] });
    efx.graphics.drawQuad(tex, 350, 150, { size: [150, 150], color: [0.9, 0.35, 0.2, 1], rotation: 18 });
    efx.graphics.drawQuad(tex, 180, 300, { size: [130, 130], color: [0.2, 0.7, 0.95, 1], rotation: -12 });
    efx.graphics.drawQuad(tex, 430, 330, { size: [110, 110], color: [0.6, 0.9, 0.3, 1] });
}
