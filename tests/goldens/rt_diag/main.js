// F5a DIAGNOSTIC (temporary, not a committed golden): distinguishes
// render-into-target from target sampling.
// Expected if everything works: green background, red 64x64 square at the
// top-left (target filled red, then sampled 1:1).
// - red square missing but black square present: render-into-target works,
//   sampling broken
// - all green: the sample shows the frame, not the target
efx.setClearColor([0, 1, 0, 1]);
const red = efx.createTexture(efx.createImageData({
    width: 1, height: 1, pixels: [255, 0, 0, 255],
}));
const rt = efx.createRenderTarget({ width: 64, height: 64 });
function update() {}
function render() {
    efx.setClearColor([0, 0, 0, 1]);
    efx.beginRenderTarget(rt);           // snapshot: black
    efx.drawQuad(0, 0, red, { size: [64, 64] });
    efx.endRenderTarget();
    efx.setClearColor([0, 1, 0, 1]);     // frame bg green again
    efx.drawQuad(0, 0, rt);              // sample full 64x64
}
