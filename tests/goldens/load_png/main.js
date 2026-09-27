/*
 * F6a golden: decode a PNG from the resource root and draw it. Exercises the
 * provider + image decode + createTexture path end to end. The 8x8 checker
 * is drawn scaled; the captured frame is the committed golden.
 */
var tex = efx.loadTexture('sprite.png');

efx.setClearColor([0.05, 0.05, 0.08, 1]);
efx.setCamera2D({ frame: [640, 480] });

efx.registerRenderHook(function () {
    efx.drawQuad(128, 96, tex, {
        size: [384, 288],
        sourceRect: { x: 0, y: 0, w: 8, h: 8 },
    });
});
