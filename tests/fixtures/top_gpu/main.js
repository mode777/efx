// Top-level GPU access: the entry script runs after the rendering surface
// exists (ADR 0016), so the engine-owned white texture is available at load
// time. This test is display-required; it proves the surface-before-entry
// ordering on a real window/GPU context.
const w = efx.graphics.whiteTexture;
if (w.width !== 1 || w.height !== 1) {
    efx.log('FAIL top-gpu-white');
    efx.quit(2);
}
efx.log('top-gpu-ok');
efx.quit(0);
