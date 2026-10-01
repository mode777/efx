/* Error-message characterization catalog (docs/refactoring.md P0).
 *
 * This script walks every option-bag and resource-liveness path and prints
 * one stable line per case:
 *
 *     <case>: <Kind>: <message>
 *
 * It runs unchanged through the desktop quickjs binding (player --script)
 * and the web bridge (resource-root main.js), so the committed expected
 * output pins the exact message text of both runtimes and their parity.
 *
 * Some cases are known to differ between the two bindings (they were written
 * independently before this catalog existed). Those are listed in the
 * `DIVERGENT` map below and print a canonical `<case>: KNOWN-DIVERGENCE`
 * line instead of the runtime-specific text, so the cross-runtime compare
 * stays green. The divergence list is recorded in docs/refactoring.md
 * section 4 and is not fixed here (behavior-preserving).
 *
 * Fixtures come from this script's directory (the default --script resource
 * root): resource_probe.txt/png, gltf_probe.glb, gltf_corrupt.gltf,
 * skin.gltf + skin.bin.
 */

var C = function (name, fn, divergent) {
    var kind = 'none';
    var msg = '';
    try {
        fn();
    } catch (e) {
        if (e instanceof TypeError) kind = 'TypeError';
        else if (e instanceof RangeError) kind = 'RangeError';
        else if (e instanceof Error) kind = 'Error';
        else kind = 'Other';
        msg = (e && e.message !== undefined) ? String(e.message) : String(e);
    }
    if (divergent || DIVERGENT[name]) {
        efx.log(name + ': KNOWN-DIVERGENCE');
    } else {
        efx.log(name + ': ' + kind + ': ' + msg);
    }
};

/* Cases where the desktop and web bindings already disagree (they were
 * written independently before this catalog existed). Each prints a
 * canonical `KNOWN-DIVERGENCE` line so the cross-runtime compare stays
 * green; the actual drift is recorded here and in docs/refactoring.md
 * section 4, and is not fixed by this behavior-preserving change. */
var DIVERGENT = {
    '4a.light-slot': 'desktop: light slot must be an integer 0..3 / web: light slot out of range (0..3)',
    '4a.light-slot-neg': 'same as 4a.light-slot',
    '4a.light-pos-short': 'desktop: expected 3 numbers / web: pos must hold 3 numbers',
    '11.billboard-pos': 'desktop: drawBillboard pos must be [x,y,z] / web: pos must be [x,y] or [x,y,z]',
};

/* ------------------------------------------------------------- 2D layer */

C('2d.clearColor-wrongtype', function () { efx.setClearColor('red'); });
C('2d.clearColor-short', function () { efx.setClearColor([1, 2, 3]); });
C('2d.camera-noargs', function () { efx.setCamera2D(); });
C('2d.camera-zoom', function () { efx.setCamera2D({ x: 0, y: 0, zoom: 0 }); });
C('2d.camera-frame2', function () { efx.setCamera2D({ frame: [0, 0] }); });
C('2d.camera-frame3', function () { efx.setCamera2D({ frame: [10, 10, 10] }); });
C('2d.blend-unknown', function () { efx.setBlendMode('nope'); });
C('2d.blend-missing', function () { efx.setBlendMode(); });
C('2d.imagedata-short', function () {
    efx.createImageData({ width: 2, height: 2, pixels: [1, 2, 3] });
});
C('2d.imagedata-format', function () {
    efx.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0], format: 'bgr' });
});
C('2d.imagedata-unknown', function () {
    efx.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0], pixles: 1 });
});
C('2d.imagedata-size', function () {
    efx.createImageData({ width: 0, height: 8, pixels: [] });
});

var img = efx.createImageData({ width: 8, height: 4, pixels: new Uint8Array(8 * 4 * 4) });
var tex = efx.createTexture(img);

C('2d.quad-few', function () { efx.drawQuad(0, 0); });
C('2d.quad-nontexture', function () { efx.drawQuad(0, 0, {}); });
C('2d.quad-size-zero', function () { efx.drawQuad(0, 0, tex, { size: [0, 10] }); });
C('2d.quad-size-short', function () { efx.drawQuad(0, 0, tex, { size: [10] }); });
C('2d.quad-size-type', function () { efx.drawQuad(0, 0, tex, { size: 'big' }); });
C('2d.quad-size-nan', function () { efx.drawQuad(0, 0, tex, { size: [NaN, 1] }); });
C('2d.quad-origin-nan', function () { efx.drawQuad(0, 0, tex, { origin: [NaN, 0] }); });
C('2d.quad-origin-type', function () { efx.drawQuad(0, 0, tex, { origin: 'center' }); });
C('2d.quad-src-zero', function () {
    efx.drawQuad(0, 0, tex, { sourceRect: { x: 0, y: 0, w: 0, h: 2 } });
});
C('2d.quad-src-oob', function () {
    efx.drawQuad(0, 0, tex, { sourceRect: { x: 0, y: 0, w: 9, h: 2 } });
});
C('2d.quad-unknown-opt', function () { efx.drawQuad(0, 0, tex, { colour: [1, 1, 1, 1] }); });

var deadTex = efx.createTexture(efx.createImageData({ width: 2, height: 2, pixels: new Uint8Array(16) }));
deadTex.destroy();
C('2d.quad-destroyed-texture', function () { efx.drawQuad(0, 0, deadTex); });
C('2d.texture-width-destroyed', function () { return deadTex.width; });
C('2d.texture-height-destroyed', function () { return deadTex.height; });

var deadImg = efx.createImageData({ width: 2, height: 2, pixels: new Uint8Array(16) });
deadImg.destroy();
C('2d.texture-from-destroyed-image', function () { efx.createTexture(deadImg); });
C('2d.texture-nonimage', function () { efx.createTexture(5); });
C('2d.texture-unknown-opt', function () { efx.createTexture(img, { frob: 1 }); });
C('2d.texture-wrap-type', function () { efx.createTexture(img, { wrap: 5 }); });
C('2d.texture-filter-type', function () { efx.createTexture(img, { filter: 5 }); });
C('2d.texture-mipmaps-type', function () { efx.createTexture(img, { mipmaps: 5 }); });

/* -------------------------------------------------------------- 3D core */

C('3d.cam-noargs', function () { efx.setCamera3D(); });
C('3d.cam-no-pos', function () { efx.setCamera3D({ target: [0, 0, 0], fov: 60 }); });
C('3d.cam-fov-type', function () { efx.setCamera3D({ pos: [0, 0, 1], target: [0, 0, 0], fov: 'wide' }); });
C('3d.cam-fov-inf', function () { efx.setCamera3D({ pos: [0, 0, 1], target: [0, 0, 0], fov: Infinity }); });
C('3d.cam-unknown', function () {
    efx.setCamera3D({ pos: [0, 0, 1], target: [0, 0, 0], fov: 60, frobnicate: 1 });
});
C('3d.cam-pos-short', function () { efx.setCamera3D({ pos: [0, 0], target: [0, 0, 0], fov: 60 }); });

var P = [0, 0, 0, 1, 0, 0, 0, 1, 0];
C('3d.md-none', function () { efx.createMeshData({}); });
C('3d.md-both', function () { efx.createMeshData({ surfaces: [{ positions: P }], positions: P }); });
C('3d.md-empty', function () { efx.createMeshData({ surfaces: [] }); });
C('3d.md-trunc', function () { efx.createMeshData({ positions: [0, 0, 0] }); });
C('3d.md-mult', function () { efx.createMeshData({ positions: [0, 0, 0, 1, 0] }); });
C('3d.md-idx-oob', function () { efx.createMeshData({ positions: P, indices: [0, 1, 3] }); });
C('3d.md-idx-partial', function () { efx.createMeshData({ positions: P, indices: [0, 1] }); });
C('3d.md-idx-frac', function () { efx.createMeshData({ positions: P, indices: [0, 1, 2, 0] }); });
C('3d.md-nonidx-div', function () { efx.createMeshData({ positions: [0, 0, 0, 1, 0, 0] }); });
C('3d.md-norm-short', function () { efx.createMeshData({ positions: P, normals: [0, 0, 1] }); });
C('3d.md-unknown', function () { efx.createMeshData({ positions: P, pixles: 1 }); });
C('3d.md-materials-mismatch', function () { efx.createMeshData({ positions: P, materials: [] }); });
C('3d.md-elem-type', function () {
    efx.createMeshData({ positions: ['a', 0, 0, 1, 0, 0, 0, 1, 0] });
});
C('3d.md-elem-nan', function () {
    efx.createMeshData({ positions: [NaN, 0, 0, 1, 0, 0, 0, 1, 0] });
});

var md = efx.createMeshData({ positions: P, indices: [0, 1, 2] });
var mesh = efx.createMesh(md);

C('3d.drawmesh-none', function () { efx.drawMesh(); });
C('3d.drawmesh-nonmesh', function () { efx.drawMesh({}); });
C('3d.drawmesh-null', function () { efx.drawMesh(null); });
C('3d.drawmesh-bag-nonobject', function () { efx.drawMesh(mesh, 5); });
C('3d.drawmesh-mesh-in-bag', function () { efx.drawMesh(mesh, { mesh: mesh }); });
C('3d.drawmesh-transform-short', function () {
    efx.drawMesh(mesh, { transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3] });
});
C('3d.drawmesh-transform-type', function () {
    efx.drawMesh(mesh, { transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 'x'] });
});
C('3d.drawmesh-color-short', function () { efx.drawMesh(mesh, { color: [1, 0, 1] }); });
C('3d.drawmesh-unknown', function () { efx.drawMesh(mesh, { frobnicate: 1 }); });
C('3d.createMesh-nonmeshdata', function () { efx.createMesh(5); });

var deadMesh = efx.createMesh(efx.createMeshData({ positions: P, indices: [0, 1, 2] }));
deadMesh.destroy();
C('3d.drawmesh-destroyed', function () { efx.drawMesh(deadMesh); });
C('3d.mesh-surfacecount-destroyed', function () { return deadMesh.surfaceCount; });
var deadMd = efx.createMeshData({ positions: P, indices: [0, 1, 2] });
deadMd.destroy();
C('3d.md-surfacecount-destroyed', function () { return deadMd.surfaceCount; });

/* ----------------------------------------------- lighting + materials */

C('4a.light-slot', function () { efx.setLight(4, { pos: [0, 0, 0], color: [1, 1, 1, 1] }); });
C('4a.light-slot-neg', function () { efx.setLight(-1, { pos: [0, 0, 0], color: [1, 1, 1, 1] }); });
C('4a.light-no-pos', function () { efx.setLight(0, { color: [1, 1, 1, 1] }); });
C('4a.light-no-color', function () { efx.setLight(0, { pos: [0, 0, 0] }); });
C('4a.light-pos-short', function () { efx.setLight(0, { pos: [0, 0], color: [1, 1, 1, 1] }); });
C('4a.light-range-type', function () { efx.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], range: 'far' }); });
C('4a.light-range-neg', function () { efx.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], range: -1 }); });
C('4a.light-unknown', function () { efx.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], frob: 1 }); });
C('4a.dir-no-dir', function () { efx.setDirectionalLight({ color: [1, 1, 1, 1] }); });
C('4a.dir-zero', function () { efx.setDirectionalLight({ dir: [0, 0, 0], color: [1, 1, 1, 1] }); });
C('4a.dir-unknown', function () { efx.setDirectionalLight({ dir: [0, -1, 0], color: [1, 1, 1, 1], foo: 1 }); });
C('4a.md-mat-length', function () {
    efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [] });
});
C('4a.md-mat-not-array', function () {
    efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: { diffuse: {} } });
});
C('4a.md-mat-bad-entry', function () {
    efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [42] });
});
C('4a.mat-short-color', function () {
    efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [{ diffuse: { color: [1, 1, 1] } }] });
});
C('4a.mat-no-color', function () {
    efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [{ diffuse: {} }] });
});
C('4a.mat-unknown', function () {
    efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [{ diffuse: { color: [1, 1, 1, 1] }, albedo: 1 }] });
});
var matMesh = efx.createMesh(efx.createMeshData({
    surfaces: [{ positions: P, indices: [0, 1, 2] }],
    materials: [{ diffuse: { color: [1, 1, 1, 1] } }],
}));
C('4a.smsm-nonmesh', function () { efx.setMeshSurfaceMaterial({}, 0, {}); });
C('4a.smsm-index', function () { efx.setMeshSurfaceMaterial(matMesh, 2, {}); });
C('4a.smsm-index-neg', function () { efx.setMeshSurfaceMaterial(matMesh, -1, {}); });
C('4a.smsm-bad-mat', function () { efx.setMeshSurfaceMaterial(matMesh, 0, 5); });
C('4a.smsm-shininess0', function () {
    efx.setMeshSurfaceMaterial(matMesh, 0, { specular: { color: [1, 1, 1, 1], shininess: 0 } });
});
var deadMatMesh = efx.createMesh(efx.createMeshData({ positions: P, indices: [0, 1, 2] }));
deadMatMesh.destroy();
C('4a.smsm-destroyed', function () { efx.setMeshSurfaceMaterial(deadMatMesh, 0, {}); });

C('4b.map-number', function () {
    efx.setMeshSurfaceMaterial(matMesh, 0, { diffuse: { color: [1, 1, 1, 1], map: 1 } });
});
C('4b.map-object', function () {
    efx.setMeshSurfaceMaterial(matMesh, 0, { diffuse: { color: [1, 1, 1, 1], map: {} } });
});
C('4b.alphamask-number', function () { efx.setMeshSurfaceMaterial(matMesh, 0, { alphaMask: 5 }); });
C('4b.channel-unknown', function () {
    efx.setMeshSurfaceMaterial(matMesh, 0, { diffuse: { color: [1, 1, 1, 1], frob: 1 } });
});
C('4b.material-unknown', function () { efx.setMeshSurfaceMaterial(matMesh, 0, { albedo: 1 }); });
C('4b.md-map-length', function () {
    efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [] });
});

/* ------------------------------------------------ render targets (F5a) */

C('5a.rt-missing-object', function () { efx.createRenderTarget(); });
C('5a.rt-missing-width', function () { efx.createRenderTarget({ height: 8 }); });
C('5a.rt-zero-size', function () { efx.createRenderTarget({ width: 0, height: 8 }); });
C('5a.rt-negative-size', function () { efx.createRenderTarget({ width: 8, height: -1 }); });
C('5a.rt-fraction-size', function () { efx.createRenderTarget({ width: 10.5, height: 8 }); });
C('5a.rt-oversize', function () { efx.createRenderTarget({ width: 4097, height: 8 }); });
C('5a.rt-unknown-field', function () { efx.createRenderTarget({ width: 8, height: 8, depth: 24 }); });
var deadRt = efx.createRenderTarget({ width: 8, height: 8 });
deadRt.destroy();
C('5a.rt-getter-after-destroy', function () { return deadRt.width; });
C('5a.rt-begin-after-destroy', function () { efx.beginRenderTarget(deadRt); });

var rtA = efx.createRenderTarget({ width: 64, height: 64 });
efx.beginRenderTarget(rtA);
C('5a.rt-nested-begin', function () { efx.beginRenderTarget(rtA); });
C('5a.rt-self-sample', function () { efx.drawQuad(0, 0, rtA); });
efx.endRenderTarget();
C('5a.rt-unbalanced-end', function () { efx.endRenderTarget(); });
C('5a.rt-src-oob', function () {
    efx.drawQuad(0, 0, rtA, { sourceRect: { x: 0, y: 0, w: 65, h: 8 } });
});
C('5a.rt-destroyed-as-texture', function () {
    var dead = efx.createRenderTarget({ width: 8, height: 8 });
    dead.destroy();
    efx.drawQuad(0, 0, dead);
});
var rtMesh = efx.createMesh(efx.createMeshData({ positions: P, indices: [0, 1, 2] }));
C('5a.rt-mesh-map-destroyed', function () {
    var dead = efx.createRenderTarget({ width: 8, height: 8 });
    dead.destroy();
    efx.setMeshSurfaceMaterial(rtMesh, 0, { diffuse: { color: [1, 1, 1, 1], map: dead } });
});
C('5a.rt-map-number', function () {
    efx.setMeshSurfaceMaterial(rtMesh, 0, { diffuse: { color: [1, 1, 1, 1], map: 3 } });
});
efx.beginRenderTarget(rtA);
C('5a.rt-mesh-feedback', function () { efx.drawMesh(rtMesh); });
efx.endRenderTarget();

/* ------------------------------------------------------- post FX (F5b) */

C('5b.post-string', function () { efx.setPostEffects('nope'); });
C('5b.post-entry-number', function () { efx.setPostEffects([1]); });
C('5b.post-unknown-effect', function () { efx.setPostEffects([{ effect: 'vortex' }]); });
C('5b.post-missing-effect', function () { efx.setPostEffects([{ radius: 2 }]); });
C('5b.post-unknown-field', function () { efx.setPostEffects([{ effect: 'blur', frob: 1 }]); });
C('5b.post-radius-type', function () { efx.setPostEffects([{ effect: 'blur', radius: 'x' }]); });
C('5b.post-radius-zero', function () { efx.setPostEffects([{ effect: 'blur', radius: 0 }]); });
C('5b.post-radius-big', function () { efx.setPostEffects([{ effect: 'blur', radius: 65 }]); });
C('5b.post-strength-big', function () { efx.setPostEffects([{ effect: 'bloom', strength: 1.5 }]); });
C('5b.post-tint-short', function () { efx.setPostEffects([{ effect: 'colorFilter', tint: [1, 1] }]); });
C('5b.post-mix-big', function () { efx.setPostEffects([{ effect: 'blur', mix: 2 }]); });
C('5b.post-too-many', function () { efx.setPostEffects(new Array(9).fill({ effect: 'blur' })); });
C('5b.scale-string', function () { efx.setRenderScale('x'); });
C('5b.scale-zero', function () { efx.setRenderScale(0); });
C('5b.scale-negative', function () { efx.setRenderScale(-1); });
C('5b.scale-big', function () { efx.setRenderScale(2.5); });
C('5b.scale-filter-bad', function () { efx.setRenderScale(1, { filter: 'bogus' }); });
C('5b.scale-unknown-field', function () { efx.setRenderScale(1, { frob: 1 }); });

/* ------------------------------------------- resources (F6a/F6b/F8a) */

C('6a.loadtext-type', function () { efx.loadText(5); });
C('6a.loadtext-missing', function () { efx.loadText('nope.txt'); });
C('6a.loadimage-type', function () { efx.loadImage(5); });
C('6a.loadimage-missing', function () { efx.loadImage('nope.png'); });
C('6b.loadmesh-type', function () { efx.loadMeshData(5); });
C('6b.loadmesh-missing', function () { efx.loadMeshData('nope.gltf'); });
C('6b.loadmesh-unknown-opt', function () { efx.loadMeshData('gltf_probe.glb', { nope: 1 }); });
C('6b.loadmesh-corrupt', function () { efx.loadMeshData('gltf_corrupt.gltf'); });
C('6c.md-joints-unpaired', function () {
    efx.createMeshData({ positions: P, joints: [0, 1, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0] });
});
C('8a.loadfont-type', function () { efx.loadFontData(5); });
C('8a.loadfont-missing', function () { efx.loadFontData('nope.ttf'); });
C('8a.createfont-nofontdata', function () { efx.createFont(5); });
C('8a.createfont-noopts', function () { efx.createFont(efx.loadFontData(5)); });
C('8a.drawtext-nofont', function () { efx.drawText('x', 5, 0, 0); });
C('8a.measuretext-nofont', function () { efx.measureText('x', 5); });

/* --------------------------------------------- skinning + animation (F7) */

var rigged = efx.createMesh(efx.loadMeshData('skin.gltf'));
C('7.pose-unknown-clip', function () { efx.poseMesh(rigged, { clip: 'nope', time: 0 }); });
C('7.pose-clip-index', function () { efx.poseMesh(rigged, { clip: 9, time: 0 }); });
C('7.pose-negative-weight', function () { efx.poseMesh(rigged, { clip: 'move', time: 0, weight: -1 }); });
C('7.pose-unknown-sample-field', function () { efx.poseMesh(rigged, { clip: 'move', time: 0, bogus: 1 }); });
C('7.pose-bad-clip-type', function () { efx.poseMesh(rigged, { clip: {}, time: 0 }); });
C('7.pose-rigless', function () { efx.poseMesh(mesh, { clip: 0, time: 0 }); });
C('7.drawmesh-skinned-rigless', function () { efx.drawMesh(mesh, { skinned: true }); });
rigged.destroy();

/* ------------------------------------------------------------- input (F9) */

C('9.keyboard-isdown-unknown', function () { efx.keyboard.isDown('notakey'); });
C('9.keyboard-isdown-nonstring', function () { efx.keyboard.isDown(5); });
C('9.keyboard-ispressed-unknown', function () { efx.keyboard.isPressed('notakey'); });
C('9.keyboard-isreleased-unknown', function () { efx.keyboard.isReleased('notakey'); });
C('9.keyboard-ondown-nonfn', function () { efx.keyboard.onDown(5); });
C('9.keyboard-onup-nonfn', function () { efx.keyboard.onUp('x'); });
C('9.keyboard-onchar-nonfn', function () { efx.keyboard.onChar(0); });
C('9.mouse-isdown-unknown', function () { efx.mouse.isDown('side'); });
C('9.mouse-isdown-nonstring', function () { efx.mouse.isDown(null); });
C('9.mouse-ondown-nonfn', function () { efx.mouse.onDown({}); });
C('9.mouse-onup-nonfn', function () { efx.mouse.onUp([]); });
C('9.mouse-onmove-nonfn', function () { efx.mouse.onMove(1); });
C('9.mouse-onwheel-nonfn', function () { efx.mouse.onWheel('x'); });

/* ------------------------------------------------- particles (F11) */

C('11.ps-missing-texture', function () { efx.createParticleSystem({ max: 4, lifetime: 1 }); });
C('11.ps-missing-max', function () { efx.createParticleSystem({ texture: tex, lifetime: 1 }); });
C('11.ps-max-range', function () { efx.createParticleSystem({ texture: tex, max: 0, lifetime: 1 }); });
C('11.ps-missing-lifetime', function () { efx.createParticleSystem({ texture: tex, max: 4 }); });
C('11.ps-lifetime-order', function () {
    efx.createParticleSystem({ texture: tex, max: 4, lifetime: [2, 1] });
});
C('11.ps-unknown-field', function () {
    efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1, bogus: 1 });
});
C('11.ps-bad-facing', function () {
    efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1, facing: 'sideways' });
});
C('11.ps-plane-screen', function () {
    efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1, space: 'screen', facing: 'plane' });
});
C('11.ps-set-max', function () {
    var p = efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1 });
    p.set({ max: 0 });
});
var deadPs = efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1 });
deadPs.destroy();
C('11.ps-destroyed-emit', function () { deadPs.emit(1); });
C('11.billboard-no-texture', function () { efx.drawBillboard([0, 0, 0], { size: [1, 1] }); });
C('11.billboard-pos', function () { efx.drawBillboard([0, 0], { texture: tex }); });
C('11.billboard-size', function () { efx.drawBillboard([0, 0, 0], { texture: tex, size: [0, 1] }); });
C('11.sprites-array', function () { efx.drawSprites(tex, 'nope'); });
C('11.sprite-xy', function () { efx.drawSprites(tex, [{ size: [1, 1] }]); });

/* ----------------------------------------------------------- physics (F12) */

C('12.createbody-noopts', function () { efx.physics.createBody(); });
C('12.createbody-noshape', function () { efx.physics.createBody({}); });
C('12.createcharacter-noopts', function () { efx.physics.createCharacter(); });
C('12.createcharacter-noradius', function () { efx.physics.createCharacter({ height: 2 }); });
C('12.createstaticmesh-noopts', function () { efx.physics.createStaticMesh(); });
C('12.createstaticmesh-nomesh', function () { efx.physics.createStaticMesh(5); });
C('12.applyimpulse-static', function () {
    efx.physics.createBody({ shape: { type: 'sphere', radius: 1 } }).applyImpulse([0, 1, 0]);
});
C('12.raycast-noopts', function () { efx.physics.raycast(); });
C('12.raycast-nomax', function () { efx.physics.raycast([0, 0, 0], [1, 0, 0]); });
C('12.overlap-noopts', function () { efx.physics.overlap(); });
C('12.shapecast-noopts', function () { efx.physics.shapeCast(); });
C('12.step-nodt', function () { efx.physics.step(); });
C('12.dt-nonfinite', function () { efx.physics.step(Infinity); });
/* coercion probe (docs/refactoring.md section 4.1): desktop `phys_opt_number`
 * runs JS_ToFloat64 (coerces '0.5'), web `__efxFiniteNumber` requires a number. */
C('coercion.phys-number-string', function () {
    efx.physics.createBody({ shape: { type: 'sphere', radius: 1 }, friction: '0.5' });
});
C('12.createbody-bad-radius', function () {
    efx.physics.createBody({ shape: { type: 'sphere', radius: 0 } });
});
C('12.createbody-shape-unknown', function () {
    efx.physics.createBody({ shape: { type: 'sphere', radius: 1, nope: 1 } });
});
var deadBody = efx.physics.createBody({ shape: { type: 'sphere', radius: 1 } });
deadBody.destroy();
C('12.body-destroyed', function () { return deadBody.position; });
var deadChar = efx.physics.createCharacter({ radius: 0.4, height: 1.8 });
deadChar.destroy();
C('12.character-destroyed', function () { return deadChar.position; });
efx.physics.clear();

/* ---------------------------------------------------------- gamepad (F13) */

C('13.gamepad-get-nonnumber', function () { efx.gamepad.get('x'); });
C('13.gamepad-onconnect-nonfn', function () { efx.gamepad.onConnect(5); });
C('13.gamepad-ondisconnect-nonfn', function () { efx.gamepad.onDisconnect(null); });

/* ------------------------------------------------------------ audio (F14) */

C('14.loadaudiodata-type', function () { efx.audio.loadAudioData(5); });
C('14.loadaudiodata-missing', function () { efx.audio.loadAudioData('nope.wav'); });
C('14.loadaudiostream-type', function () { efx.audio.loadAudioStream(5); });
C('14.loadaudiostream-missing', function () { efx.audio.loadAudioStream('nope.mp3'); });
C('14.playaudio-bad-source', function () { efx.audio.playAudio({}); });
C('14.playaudio-null', function () { efx.audio.playAudio(null); });
C('14.volume-negative', function () { efx.audio.volume = -1; });

tex.destroy();
img.destroy();
mesh.destroy();
md.destroy();
matMesh.destroy();
rtA.destroy();
rtMesh.destroy();

efx.log('s-error-catalog-ok');
efx.quit(0);
