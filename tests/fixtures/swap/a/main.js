/* ADR 0057 swap smoke: script-owned GPU resources must be released on the
 * next swap (runtime finalizers + efx_render_reset). */
efx.log('swap-a-ok');
var tex = efx.graphics.createTexture(
    efx.graphics.createImageData(2, 2, [
        255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255]));
efx.registerRenderHook(function () {
    efx.graphics.setClearColor([0.15, 0.0, 0.0, 1]);
    efx.graphics.drawQuad(tex, 0, 0, { size: [16, 16] });
});
