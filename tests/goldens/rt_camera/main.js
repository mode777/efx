// F5a golden: the 2D camera frame inside render targets. The first target
// (200x200) is drawn under a frame equal to the target's extent — the same
// pixels the default camera produces (the default frame is the active
// rendering surface's extent); the second (320x240) is drawn with an
// explicit 640x480 frame stretched onto it. Camera state persists across
// frames, so both frames are set explicitly per segment (frame-stable).
// Both targets are then sampled at fixed frame positions.
efx.graphics.setClearColor([0.1, 0.1, 0.25, 1]);
const pixels = [];
for (let y = 0; y < 4; y++) {
    for (let x = 0; x < 4; x++) {
        const on = (x + y) % 2 === 0;
        pixels.push(on ? 255 : 30, 40, on ? 30 : 200, 255);
    }
}
const tex = efx.graphics.createTexture(
    efx.graphics.createImageData({ width: 4, height: 4, pixels: pixels }));
const rt = efx.graphics.createRenderTarget({ width: 200, height: 200 });
const rt2 = efx.graphics.createRenderTarget({ width: 320, height: 240 });
function update() {}
function render() {
    efx.graphics.setCamera2D({ frame: [200, 200] });
    efx.graphics.beginRenderTarget(rt);
    efx.graphics.drawQuad(10, 10, tex, { size: [90, 90] });
    efx.graphics.drawQuad(105, 105, tex, { size: [85, 85] });
    efx.graphics.endRenderTarget();
    efx.graphics.setCamera2D({ frame: [640, 480] });
    efx.graphics.beginRenderTarget(rt2);
    efx.graphics.drawQuad(60, 40, tex, { size: [200, 160], rotation: 15 });
    efx.graphics.endRenderTarget();
    efx.graphics.drawQuad(20, 20, rt, { size: [200, 200] });
    efx.graphics.drawQuad(260, 120, rt2);
}
