// F5a golden: a 2D scene rendered into a 320x240 render target and sampled
// back — full-frame (size derives from the target extent) and a top-left
// quarter via sourceRect. Pins segmentation, clear-per-begin, and upright
// sampling.
efx.graphics.setClearColor([0, 0.22, 0.05, 1]);
const tex = efx.graphics.createTexture(efx.graphics.createImageData({
    width: 2, height: 2,
    pixels: [255, 60, 40, 255, 40, 120, 255, 255,
             250, 220, 40, 255, 40, 200, 90, 255],
}));
const rt = efx.graphics.createRenderTarget({ width: 320, height: 240 });
function update() {}
function render() {
    efx.graphics.beginRenderTarget(rt);
    efx.graphics.drawQuad(0, 0, tex, { size: [160, 120] });
    efx.graphics.drawQuad(200, 110, tex, { size: [90, 90], rotation: 30 });
    efx.graphics.endRenderTarget();
    efx.graphics.drawQuad(0, 0, rt);
    efx.graphics.drawQuad(360, 260, rt, {
        sourceRect: { x: 0, y: 0, w: 160, h: 120 },
        size: [240, 180],
    });
}
