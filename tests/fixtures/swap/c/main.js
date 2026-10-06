/* ADR 0057 swap smoke: third swapped root; the seam ends the run after it. */
efx.log('swap-c-ok');
var tex = efx.graphics.createTexture(
    efx.graphics.createImageData(2, 2, [
        0, 0, 255, 255, 0, 0, 255, 255,
        0, 0, 255, 255, 0, 0, 255, 255]));
efx.registerRenderHook(function () {
    efx.graphics.setClearColor([0.0, 0.0, 0.15, 1]);
    efx.graphics.drawQuad(tex, 0, 0, { size: [16, 16] });
});
