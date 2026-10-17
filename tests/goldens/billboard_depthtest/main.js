// playback-quick-wins golden: a depthTest:false billboard draws over opaque
// geometry that fully occludes its depthTest:true control. The two occluded
// billboards and the two depthTest:false billboards each form one batched
// run (same texture/blend/depth-test), so the scene also pins run batching
// and the run split on the depth-test key. Static camera, static geometry,
// static burst-free frame — stable as a golden.

function makeGlow(size) {
    const px = new Uint8Array(size * size * 4);
    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const dx = ((x + 0.5) / size) * 2 - 1;
            const dy = ((y + 0.5) / size) * 2 - 1;
            const d = Math.min(1, Math.sqrt(dx * dx + dy * dy));
            const a = Math.round((1 - d) * (1 - d) * 255);
            const i = (y * size + x) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = a;
        }
    }
    return px;
}

const glow = efx.graphics.createTexture(
    efx.graphics.createImageData(16, 16, makeGlow(16)),
);

efx.graphics.setClearColor([0.04, 0.06, 0.09, 1]);
efx.graphics.setCamera3D([0, 0, 6], [0, 0, 0], 55);
efx.graphics.setLight(0, { pos: [2.5, 3.5, 4.0], color: [1, 0.95, 0.9, 1], range: 30 });

// unlit wall slab so the occluder is a flat deterministic color
const wall = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1 }));
wall.setSurfaceMaterial(0, {
    diffuse: { color: [0.85, 0.85, 0.9, 1] },
    unlit: true,
});

function update() {}
function render() {
    // opaque occluder at the origin, writes depth
    efx.graphics.drawMesh(wall, {
        transform: efx.math.mat4.translate(
            efx.math.mat4.scale(efx.math.mat4.identity(), [3.2, 2.4, 0.25]),
            [0, 0, 0]),
        color: [1, 1, 1, 1],
    });
    // behind the wall: depth-tested pair (occluded, one batched run)
    efx.graphics.drawBillboard(glow, [-0.8, 0.75, -1.5], { size: 0.8, color: [1, 0.25, 0.2, 1] });
    efx.graphics.drawBillboard(glow, [-0.8, 0.75, -1.6], { size: 0.5, color: [1, 0.5, 0.2, 1] });
    // depthTest:false pair (drawn over the wall, one batched run; the later
    // record blends over the earlier one — neither writes depth)
    efx.graphics.drawBillboard(glow, [-0.8, -0.75, -1.5], { size: 0.8, color: [0.2, 1, 0.4, 1], depthTest: false });
    efx.graphics.drawBillboard(glow, [-0.8, -0.75, -1.6], { size: 0.5, color: [0.2, 0.6, 1, 1], depthTest: false });
}
