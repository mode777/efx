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

// the engine whiteTexture is available headless too (CPU-only, ADR 0052);
// any live texture works as the quad source
const tex = efx.graphics.createTexture(efx.graphics.createImageData(2, 2, new Uint8Array(16)));
efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { size: [4, 4] });

// lifecycle + query properties
const rt = efx.graphics.createRenderTarget(256, 128);
if (rt.width !== 256 || rt.height !== 128) { efx.log('FAIL rt size'); efx.quit(3); }
rt.destroy();
rt.destroy(); // idempotent
expectThrow('getter-after-destroy', TE, () => rt.width);
expectThrow('begin-after-destroy', TE, () => efx.graphics.beginRenderTarget(rt));

// validation matrix
expectThrow('missing-object', TE, () => efx.graphics.createRenderTarget());
expectThrow('missing-width', TE, () => efx.graphics.createRenderTarget(undefined, 8));
expectThrow('zero-size', RE, () => efx.graphics.createRenderTarget(0, 8));
expectThrow('negative-size', RE, () => efx.graphics.createRenderTarget(8, -1));
expectThrow('fraction-size', RE, () => efx.graphics.createRenderTarget(10.5, 8));
expectThrow('oversize', RE, () => efx.graphics.createRenderTarget(4097, 8));

// redirection: begin/draw/end, nesting and balance errors
const a = efx.graphics.createRenderTarget(64, 64);
efx.graphics.beginRenderTarget(a);
efx.graphics.drawQuad(tex, 0, 0, { size: [32, 32] });
expectThrow('nested-begin', TE, () => efx.graphics.beginRenderTarget(a));
expectThrow('self-sample', TE, () => efx.graphics.drawQuad(a, 0, 0));
efx.graphics.endRenderTarget();
expectThrow('unbalanced-end', TE, () => efx.graphics.endRenderTarget());

// texture coercion: an RT is accepted wherever a texture is, with the
// target's extent driving size derivation and sourceRect validation
efx.graphics.drawQuad(a, 0, 0);                                              // 64x64 from target extent
efx.graphics.drawQuad(a, 0, 0, { sourceRect: { x: 0, y: 0, w: 32, h: 16 } }); // top-left quarter
expectThrow('src-oob', RE, () => efx.graphics.drawQuad(a, 0, 0, { sourceRect: { x: 0, y: 0, w: 65, h: 8 } }));
expectThrow('destroyed-as-texture', TE, () => {
    const dead = efx.graphics.createRenderTarget(8, 8);
    dead.destroy();
    efx.graphics.drawQuad(dead, 0, 0);
});

// a mesh material map may reference a render target
const mesh = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: [0, 0, 0, 1, 0, 0, 0, 1, 0], uvs: [0, 0, 1, 0, 0, 1], indices: [0, 1, 2] }]));
efx.graphics.setCamera3D([0, 0, 5], [0, 0, 0], 60);
efx.graphics.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: a } });
efx.graphics.drawMesh(mesh);
expectThrow('mesh-map-destroyed', TE, () => {
    const dead = efx.graphics.createRenderTarget(8, 8);
    dead.destroy();
    efx.graphics.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: dead } });
});
expectThrow('map-number', TE, () => efx.graphics.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: 3 } }));

// drawing the mesh with a map onto the target it samples is rejected
efx.graphics.beginRenderTarget(a);
expectThrow('mesh-feedback', TE, () => efx.graphics.drawMesh(mesh));
efx.graphics.endRenderTarget();

// default camera frame follows the active target (record-level assertions
// live in the unit suite; here the draws must simply succeed with the
// target extent playing the window's role)
efx.graphics.setCamera2D({ frame: [640, 480] });
efx.graphics.beginRenderTarget(a);
efx.graphics.drawQuad(tex, 0, 0); // explicit frame wins
efx.graphics.endRenderTarget();
const b = efx.graphics.createRenderTarget(320, 200);
efx.graphics.beginRenderTarget(b);
efx.graphics.drawQuad(tex, 0, 0); // default frame = the target extent
efx.graphics.endRenderTarget();

a.destroy();
b.destroy();
mesh.destroy();
efx.log('s-5a-validation-ok');
