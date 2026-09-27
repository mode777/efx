// F5a smoke: render-target lifecycle, redirection, texture coercion, and
// the feedback-loop guard (portable — identical on desktop and web; the
// rendered result is covered by the unit/golden suites).
function expectThrow(name, kind, fn) {
    try { fn(); efx.log('FAIL no-throw ' + name); efx.quit(1); }
    catch (e) {
        if (!(e instanceof kind)) {
            efx.log('FAIL kind ' + name + ': ' + e); efx.quit(2);
        }
    }
}
const TE = TypeError, RE = RangeError;

// script mode has no GPU sink, so the white texture is unavailable — any
// live texture works as the quad source (F2 white-texture rule is
// windowed-run only)
const tex = efx.createTexture(efx.createImageData({
    width: 2, height: 2, pixels: new Uint8Array(16),
}));

// lifecycle + query properties
const rt = efx.createRenderTarget({ width: 256, height: 128 });
if (rt.width !== 256 || rt.height !== 128) { efx.log('FAIL rt size'); efx.quit(3); }
rt.destroy();
rt.destroy(); // idempotent
expectThrow('getter-after-destroy', TE, () => rt.width);
expectThrow('begin-after-destroy', TE, () => efx.beginRenderTarget(rt));

// validation matrix
expectThrow('missing-object', TE, () => efx.createRenderTarget());
expectThrow('missing-width', TE, () => efx.createRenderTarget({ height: 8 }));
expectThrow('zero-size', RE, () => efx.createRenderTarget({ width: 0, height: 8 }));
expectThrow('negative-size', RE, () => efx.createRenderTarget({ width: 8, height: -1 }));
expectThrow('fraction-size', RE, () => efx.createRenderTarget({ width: 10.5, height: 8 }));
expectThrow('oversize', RE, () => efx.createRenderTarget({ width: 4097, height: 8 }));
expectThrow('unknown-field', TE, () => efx.createRenderTarget({ width: 8, height: 8, depth: 24 }));

// redirection: begin/draw/end, nesting and balance errors
const a = efx.createRenderTarget({ width: 64, height: 64 });
efx.beginRenderTarget(a);
efx.drawQuad(0, 0, tex, { size: [32, 32] });
expectThrow('nested-begin', TE, () => efx.beginRenderTarget(a));
expectThrow('self-sample', TE, () => efx.drawQuad(0, 0, a));
efx.endRenderTarget();
expectThrow('unbalanced-end', TE, () => efx.endRenderTarget());

// texture coercion: an RT is accepted wherever a texture is, with the
// target's extent driving size derivation and sourceRect validation
efx.drawQuad(0, 0, a);                                              // 64x64 from target extent
efx.drawQuad(0, 0, a, { sourceRect: { x: 0, y: 0, w: 32, h: 16 } }); // top-left quarter
expectThrow('src-oob', RE, () => efx.drawQuad(0, 0, a, { sourceRect: { x: 0, y: 0, w: 65, h: 8 } }));
expectThrow('destroyed-as-texture', TE, () => {
    const dead = efx.createRenderTarget({ width: 8, height: 8 });
    dead.destroy();
    efx.drawQuad(0, 0, dead);
});

// a mesh material map may reference a render target
const mesh = efx.createMesh(efx.createMeshData({
    positions: [0, 0, 0, 1, 0, 0, 0, 1, 0],
    uvs: [0, 0, 1, 0, 0, 1],
    indices: [0, 1, 2],
}));
efx.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 60 });
efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: a } });
efx.drawMesh({ mesh });
expectThrow('mesh-map-destroyed', TE, () => {
    const dead = efx.createRenderTarget({ width: 8, height: 8 });
    dead.destroy();
    efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: dead } });
});
expectThrow('map-number', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: 3 } }));

// drawing the mesh with a map onto the target it samples is rejected
efx.beginRenderTarget(a);
expectThrow('mesh-feedback', TE, () => efx.drawMesh({ mesh }));
efx.endRenderTarget();

// default camera frame follows the active target (record-level assertions
// live in the unit suite; here the draws must simply succeed with the
// target extent playing the window's role)
efx.setCamera2D({ frame: [640, 480] });
efx.beginRenderTarget(a);
efx.drawQuad(0, 0, tex); // explicit frame wins
efx.endRenderTarget();
const b = efx.createRenderTarget({ width: 320, height: 200 });
efx.beginRenderTarget(b);
efx.drawQuad(0, 0, tex); // default frame = the target extent
efx.endRenderTarget();

a.destroy();
b.destroy();
mesh.destroy();
efx.log('s-5a-validation-ok');
