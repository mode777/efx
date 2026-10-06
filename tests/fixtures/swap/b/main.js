/* ADR 0057 swap smoke: second swapped root. */
efx.log('swap-b-ok');
var tex = efx.graphics.createTexture(
    efx.graphics.createImageData(2, 2, [
        0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255]));
efx.registerRenderHook(function () {
    efx.graphics.setClearColor([0.0, 0.15, 0.0, 1]);
    efx.graphics.drawQuad(tex, 0, 0, { size: [16, 16] });
});
