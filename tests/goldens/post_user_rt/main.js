// F5b golden: a user render target sampled on screen through a chain — the
// target's scene-side contents render raw (its own segment); only the final
// screen resolve passes through the blur.
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });
const tex = efx.graphics.createTexture(efx.graphics.createImageData(2, 2, [
        230, 40, 40, 255,   40, 210, 80, 255,
        40, 80, 230, 255,   240, 220, 60, 255,
    ]));
const rt = efx.graphics.createRenderTarget(320, 240);
efx.graphics.setPostEffects([{ effect: 'blur', radius: 6 }]);
function update() {}
function render() {
    efx.graphics.beginRenderTarget(rt);
    efx.graphics.drawQuad(tex, 0, 0, { size: [640, 480] });
    efx.graphics.drawQuad(efx.graphics.whiteTexture, 160, 120, { size: [320, 100], color: [1, 1, 1, 1] });
    efx.graphics.endRenderTarget();
    efx.graphics.drawQuad(rt, 160, 120);
}
