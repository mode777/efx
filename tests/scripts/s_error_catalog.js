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
};

/* ------------------------------------------------------------- 2D layer */

C('2d.clearColor-wrongtype', function () { efx.graphics.setClearColor('red'); });
C('2d.clearColor-short', function () { efx.graphics.setClearColor([1, 2, 3]); });
C('2d.camera-noargs', function () { efx.graphics.setCamera2D(); });
C('2d.camera-zoom', function () { efx.graphics.setCamera2D({ x: 0, y: 0, zoom: 0 }); });
C('2d.camera-frame2', function () { efx.graphics.setCamera2D({ frame: [0, 0] }); });
C('2d.camera-frame3', function () { efx.graphics.setCamera2D({ frame: [10, 10, 10] }); });
C('2d.blend-unknown', function () { efx.graphics.setBlendMode('nope'); });
C('2d.blend-missing', function () { efx.graphics.setBlendMode(); });
C('2d.imagedata-short', function () {
    efx.graphics.createImageData(2, 2, [1, 2, 3]);
});
C('2d.imagedata-format', function () {
    efx.graphics.createImageData(1, 1, [0, 0, 0, 0], { format: 'bgr' });
});
C('2d.imagedata-unknown', function () {
    efx.graphics.createImageData(1, 1, [0, 0, 0, 0], { pixles: 1 });
});
C('2d.imagedata-size', function () {
    efx.graphics.createImageData(0, 8, []);
});

var img = efx.graphics.createImageData(8, 4, new Uint8Array(8 * 4 * 4));
var tex = efx.graphics.createTexture(img);

C('2d.quad-few', function () { efx.graphics.drawQuad(tex, 0); });
C('2d.quad-nontexture', function () { efx.graphics.drawQuad({}, 0, 0); });
C('2d.quad-size-zero', function () { efx.graphics.drawQuad(tex, 0, 0, { size: [0, 10] }); });
C('2d.quad-size-short', function () { efx.graphics.drawQuad(tex, 0, 0, { size: [10] }); });
C('2d.quad-size-type', function () { efx.graphics.drawQuad(tex, 0, 0, { size: 'big' }); });
C('2d.quad-size-nan', function () { efx.graphics.drawQuad(tex, 0, 0, { size: [NaN, 1] }); });
C('2d.quad-origin-nan', function () { efx.graphics.drawQuad(tex, 0, 0, { origin: [NaN, 0] }); });
C('2d.quad-origin-type', function () { efx.graphics.drawQuad(tex, 0, 0, { origin: 'center' }); });
C('2d.quad-src-zero', function () {
    efx.graphics.drawQuad(tex, 0, 0, { sourceRect: { x: 0, y: 0, w: 0, h: 2 } });
});
C('2d.quad-src-oob', function () {
    efx.graphics.drawQuad(tex, 0, 0, { sourceRect: { x: 0, y: 0, w: 9, h: 2 } });
});
C('2d.quad-unknown-opt', function () { efx.graphics.drawQuad(tex, 0, 0, { colour: [1, 1, 1, 1] }); });

var deadTex = efx.graphics.createTexture(efx.graphics.createImageData(2, 2, new Uint8Array(16)));
deadTex.destroy();
C('2d.quad-destroyed-texture', function () { efx.graphics.drawQuad(deadTex, 0, 0); });
C('2d.texture-width-destroyed', function () { return deadTex.width; });
C('2d.texture-height-destroyed', function () { return deadTex.height; });

var deadImg = efx.graphics.createImageData(2, 2, new Uint8Array(16));
deadImg.destroy();
C('2d.texture-from-destroyed-image', function () { efx.graphics.createTexture(deadImg); });
C('2d.texture-nonimage', function () { efx.graphics.createTexture(5); });
C('2d.texture-unknown-opt', function () { efx.graphics.createTexture(img, { frob: 1 }); });
C('2d.texture-wrap-type', function () { efx.graphics.createTexture(img, { wrap: 5 }); });
C('2d.texture-filter-type', function () { efx.graphics.createTexture(img, { filter: 5 }); });
C('2d.texture-mipmaps-type', function () { efx.graphics.createTexture(img, { mipmaps: 5 }); });

/* -------------------------------------------------------------- 3D core */

C('3d.cam-noargs', function () { efx.graphics.setCamera3D(); });
C('3d.cam-no-pos', function () { efx.graphics.setCamera3D(undefined, [0, 0, 0], 60); });
C('3d.cam-fov-type', function () { efx.graphics.setCamera3D([0, 0, 1], [0, 0, 0], 'wide'); });
C('3d.cam-fov-inf', function () { efx.graphics.setCamera3D([0, 0, 1], [0, 0, 0], Infinity); });
C('3d.cam-unknown', function () {
    efx.graphics.setCamera3D([0, 0, 1], [0, 0, 0], 60, { frobnicate: 1 });
});
C('3d.cam-pos-short', function () { efx.graphics.setCamera3D([0, 0], [0, 0, 0], 60); });

var P = [0, 0, 0, 1, 0, 0, 0, 1, 0];
C('3d.md-none', function () { efx.graphics.createMeshData(); });
C('3d.md-notarray', function () { efx.graphics.createMeshData({ positions: P }); });
C('3d.md-empty', function () { efx.graphics.createMeshData([]); });
C('3d.md-trunc', function () { efx.graphics.createMeshData([{ positions: [0, 0, 0] }]); });
C('3d.md-mult', function () { efx.graphics.createMeshData([{ positions: [0, 0, 0, 1, 0] }]); });
C('3d.md-idx-oob', function () { efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 3] }]); });
C('3d.md-idx-partial', function () { efx.graphics.createMeshData([{ positions: P, indices: [0, 1] }]); });
C('3d.md-idx-frac', function () { efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2, 0] }]); });
C('3d.md-nonidx-div', function () { efx.graphics.createMeshData([{ positions: [0, 0, 0, 1, 0, 0] }]); });
C('3d.md-norm-short', function () { efx.graphics.createMeshData([{ positions: P, normals: [0, 0, 1] }]); });
C('3d.md-unknown', function () { efx.graphics.createMeshData([{ positions: P, pixles: 1 }]); });
C('3d.md-materials-mismatch', function () { efx.graphics.createMeshData([{ positions: P }], []); });
C('3d.md-elem-type', function () {
    efx.graphics.createMeshData([{ positions: ['a', 0, 0, 1, 0, 0, 0, 1, 0] }]);
});
C('3d.md-elem-nan', function () {
    efx.graphics.createMeshData([{ positions: [NaN, 0, 0, 1, 0, 0, 0, 1, 0] }]);
});

var md = efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }]);
var mesh = efx.graphics.createMesh(md);

C('3d.drawmesh-none', function () { efx.graphics.drawMesh(); });
C('3d.drawmesh-nonmesh', function () { efx.graphics.drawMesh({}); });
C('3d.drawmesh-null', function () { efx.graphics.drawMesh(null); });
C('3d.drawmesh-bag-nonobject', function () { efx.graphics.drawMesh(mesh, 5); });
C('3d.drawmesh-mesh-in-bag', function () { efx.graphics.drawMesh(mesh, { mesh: mesh }); });
C('3d.drawmesh-transform-short', function () {
    efx.graphics.drawMesh(mesh, { transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3] });
});
C('3d.drawmesh-transform-type', function () {
    efx.graphics.drawMesh(mesh, { transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 'x'] });
});
C('3d.drawmesh-color-short', function () { efx.graphics.drawMesh(mesh, { color: [1, 0, 1] }); });
C('3d.drawmesh-unknown', function () { efx.graphics.drawMesh(mesh, { frobnicate: 1 }); });
C('3d.createMesh-nonmeshdata', function () { efx.graphics.createMesh(5); });

var deadMesh = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }]));
deadMesh.destroy();
C('3d.drawmesh-destroyed', function () { efx.graphics.drawMesh(deadMesh); });
C('3d.mesh-surfacecount-destroyed', function () { return deadMesh.surfaceCount; });
var deadMd = efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }]);
deadMd.destroy();
C('3d.md-surfacecount-destroyed', function () { return deadMd.surfaceCount; });

/* ----------------------------------------------- lighting + materials */

C('4a.light-slot', function () { efx.graphics.setLight(4, { pos: [0, 0, 0], color: [1, 1, 1, 1] }); });
C('4a.light-slot-neg', function () { efx.graphics.setLight(-1, { pos: [0, 0, 0], color: [1, 1, 1, 1] }); });
C('4a.light-no-pos', function () { efx.graphics.setLight(0, { color: [1, 1, 1, 1] }); });
C('4a.light-no-color', function () { efx.graphics.setLight(0, { pos: [0, 0, 0] }); });
C('4a.light-pos-short', function () { efx.graphics.setLight(0, { pos: [0, 0], color: [1, 1, 1, 1] }); });
C('4a.light-range-type', function () { efx.graphics.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], range: 'far' }); });
C('4a.light-range-neg', function () { efx.graphics.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], range: -1 }); });
C('4a.light-unknown', function () { efx.graphics.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], frob: 1 }); });
C('4a.dir-no-dir', function () { efx.graphics.setDirectionalLight({ color: [1, 1, 1, 1] }); });
C('4a.dir-zero', function () { efx.graphics.setDirectionalLight({ dir: [0, 0, 0], color: [1, 1, 1, 1] }); });
C('4a.dir-unknown', function () { efx.graphics.setDirectionalLight({ dir: [0, -1, 0], color: [1, 1, 1, 1], foo: 1 }); });
C('4a.md-mat-length', function () {
    efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }], []);
});
C('4a.md-mat-not-array', function () {
    efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }], { diffuse: {} });
});
C('4a.md-mat-bad-entry', function () {
    efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }], [42]);
});
C('4a.mat-short-color', function () {
    efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }], [{ diffuse: { color: [1, 1, 1] } }]);
});
C('4a.mat-no-color', function () {
    efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }], [{ diffuse: {} }]);
});
C('4a.mat-unknown', function () {
    efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }], [{ diffuse: { color: [1, 1, 1, 1] }, albedo: 1 }]);
});
var matMesh = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }], [{ diffuse: { color: [1, 1, 1, 1] } }]));
C('4a.smsm-nonmesh', function () { efx.graphics.setMeshSurfaceMaterial({}, 0, {}); });
C('4a.smsm-index', function () { efx.graphics.setMeshSurfaceMaterial(matMesh, 2, {}); });
C('4a.smsm-index-neg', function () { efx.graphics.setMeshSurfaceMaterial(matMesh, -1, {}); });
C('4a.smsm-bad-mat', function () { efx.graphics.setMeshSurfaceMaterial(matMesh, 0, 5); });
C('4a.smsm-shininess0', function () {
    efx.graphics.setMeshSurfaceMaterial(matMesh, 0, { specular: { color: [1, 1, 1, 1], shininess: 0 } });
});
var deadMatMesh = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }]));
deadMatMesh.destroy();
C('4a.smsm-destroyed', function () { efx.graphics.setMeshSurfaceMaterial(deadMatMesh, 0, {}); });

C('4b.map-number', function () {
    efx.graphics.setMeshSurfaceMaterial(matMesh, 0, { diffuse: { color: [1, 1, 1, 1], map: 1 } });
});
C('4b.map-object', function () {
    efx.graphics.setMeshSurfaceMaterial(matMesh, 0, { diffuse: { color: [1, 1, 1, 1], map: {} } });
});
C('4b.alphamask-number', function () { efx.graphics.setMeshSurfaceMaterial(matMesh, 0, { alphaMask: 5 }); });
C('4b.channel-unknown', function () {
    efx.graphics.setMeshSurfaceMaterial(matMesh, 0, { diffuse: { color: [1, 1, 1, 1], frob: 1 } });
});
C('4b.material-unknown', function () { efx.graphics.setMeshSurfaceMaterial(matMesh, 0, { albedo: 1 }); });
C('4b.md-map-length', function () {
    efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }], []);
});

/* ------------------------------------------------ render targets (F5a) */

C('5a.rt-missing-object', function () { efx.graphics.createRenderTarget(); });
C('5a.rt-missing-width', function () { efx.graphics.createRenderTarget(undefined, 8); });
C('5a.rt-zero-size', function () { efx.graphics.createRenderTarget(0, 8); });
C('5a.rt-negative-size', function () { efx.graphics.createRenderTarget(8, -1); });
C('5a.rt-fraction-size', function () { efx.graphics.createRenderTarget(10.5, 8); });
C('5a.rt-oversize', function () { efx.graphics.createRenderTarget(4097, 8); });
C('5a.rt-nonnumber', function () { efx.graphics.createRenderTarget('8', 8); });
var deadRt = efx.graphics.createRenderTarget(8, 8);
deadRt.destroy();
C('5a.rt-getter-after-destroy', function () { return deadRt.width; });
C('5a.rt-begin-after-destroy', function () { efx.graphics.beginRenderTarget(deadRt); });

var rtA = efx.graphics.createRenderTarget(64, 64);
efx.graphics.beginRenderTarget(rtA);
C('5a.rt-nested-begin', function () { efx.graphics.beginRenderTarget(rtA); });
C('5a.rt-self-sample', function () { efx.graphics.drawQuad(rtA, 0, 0); });
efx.graphics.endRenderTarget();
C('5a.rt-unbalanced-end', function () { efx.graphics.endRenderTarget(); });
C('5a.rt-src-oob', function () {
    efx.graphics.drawQuad(rtA, 0, 0, { sourceRect: { x: 0, y: 0, w: 65, h: 8 } });
});
C('5a.rt-destroyed-as-texture', function () {
    var dead = efx.graphics.createRenderTarget(8, 8);
    dead.destroy();
    efx.graphics.drawQuad(dead, 0, 0);
});
var rtMesh = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }]));
C('5a.rt-mesh-map-destroyed', function () {
    var dead = efx.graphics.createRenderTarget(8, 8);
    dead.destroy();
    efx.graphics.setMeshSurfaceMaterial(rtMesh, 0, { diffuse: { color: [1, 1, 1, 1], map: dead } });
});
C('5a.rt-map-number', function () {
    efx.graphics.setMeshSurfaceMaterial(rtMesh, 0, { diffuse: { color: [1, 1, 1, 1], map: 3 } });
});
efx.graphics.beginRenderTarget(rtA);
C('5a.rt-mesh-feedback', function () { efx.graphics.drawMesh(rtMesh); });
efx.graphics.endRenderTarget();

/* ------------------------------------------------------- post FX (F5b) */

C('5b.post-string', function () { efx.graphics.setPostEffects('nope'); });
C('5b.post-entry-number', function () { efx.graphics.setPostEffects([1]); });
C('5b.post-unknown-effect', function () { efx.graphics.setPostEffects([{ effect: 'vortex' }]); });
C('5b.post-missing-effect', function () { efx.graphics.setPostEffects([{ radius: 2 }]); });
C('5b.post-unknown-field', function () { efx.graphics.setPostEffects([{ effect: 'blur', frob: 1 }]); });
C('5b.post-radius-type', function () { efx.graphics.setPostEffects([{ effect: 'blur', radius: 'x' }]); });
C('5b.post-radius-zero', function () { efx.graphics.setPostEffects([{ effect: 'blur', radius: 0 }]); });
C('5b.post-radius-big', function () { efx.graphics.setPostEffects([{ effect: 'blur', radius: 65 }]); });
C('5b.post-strength-big', function () { efx.graphics.setPostEffects([{ effect: 'bloom', strength: 1.5 }]); });
C('5b.post-tint-short', function () { efx.graphics.setPostEffects([{ effect: 'colorFilter', tint: [1, 1] }]); });
C('5b.post-mix-big', function () { efx.graphics.setPostEffects([{ effect: 'blur', mix: 2 }]); });
C('5b.post-too-many', function () { efx.graphics.setPostEffects(new Array(9).fill({ effect: 'blur' })); });
C('5b.scale-string', function () { efx.graphics.setRenderScale('x'); });
C('5b.scale-zero', function () { efx.graphics.setRenderScale(0); });
C('5b.scale-negative', function () { efx.graphics.setRenderScale(-1); });
C('5b.scale-big', function () { efx.graphics.setRenderScale(2.5); });
C('5b.scale-filter-bad', function () { efx.graphics.setRenderScale(1, { filter: 'bogus' }); });
C('5b.scale-unknown-field', function () { efx.graphics.setRenderScale(1, { frob: 1 }); });

/* ------------------------------------------- resources (F6a/F6b/F8a) */

C('6a.loadtext-type', function () { efx.io.loadText(5); });
C('6a.loadtext-missing', function () { efx.io.loadText('nope.txt'); });
C('6a.loaddata-type', function () { efx.io.loadData(5); });
C('6a.loaddata-missing', function () { efx.io.loadData('nope.bin'); });
C('6a.loadimage-type', function () { efx.graphics.loadImage(5); });
C('6a.loadimage-missing', function () { efx.graphics.loadImage('nope.png'); });
C('6b.loadmesh-type', function () { efx.graphics.loadMeshData(5); });
C('6b.loadmesh-missing', function () { efx.graphics.loadMeshData('nope.gltf'); });
C('6b.loadmesh-unknown-opt', function () { efx.graphics.loadMeshData('gltf_probe.glb', { nope: 1 }); });
C('6b.loadmesh-corrupt', function () { efx.graphics.loadMeshData('gltf_corrupt.gltf'); });
C('6c.md-joints-unpaired', function () {
    efx.graphics.createMeshData([{ positions: P, joints: [0, 1, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0] }]);
});
C('8a.loadfont-type', function () { efx.graphics.loadFontData(5); });
C('8a.loadfont-missing', function () { efx.graphics.loadFontData('nope.ttf'); });
C('8a.createfont-nofontdata', function () { efx.graphics.createFont(5); });
C('8a.createfont-noopts', function () { efx.graphics.createFont(efx.graphics.loadFontData(5)); });
C('8a.drawtext-nofont', function () { efx.graphics.drawText('x', 5, 0, 0); });
C('8a.measuretext-nofont', function () { efx.graphics.measureText('x', 5); });

/* --------------------------------------------- skinning + animation (F7) */

var rigged = efx.graphics.createMesh(efx.graphics.loadMeshData('skin.gltf'));
C('7.pose-unknown-clip', function () { efx.graphics.poseMesh(rigged, { clip: 'nope', time: 0 }); });
C('7.pose-clip-index', function () { efx.graphics.poseMesh(rigged, { clip: 9, time: 0 }); });
C('7.pose-negative-weight', function () { efx.graphics.poseMesh(rigged, { clip: 'move', time: 0, weight: -1 }); });
C('7.pose-unknown-sample-field', function () { efx.graphics.poseMesh(rigged, { clip: 'move', time: 0, bogus: 1 }); });
C('7.pose-bad-clip-type', function () { efx.graphics.poseMesh(rigged, { clip: {}, time: 0 }); });
C('7.pose-rigless', function () { efx.graphics.poseMesh(mesh, { clip: 0, time: 0 }); });
C('7.drawmesh-skinned-rigless', function () { efx.graphics.drawMesh(mesh, { skinned: true }); });
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

C('11.ps-missing-texture', function () { efx.graphics.createParticleSystem(undefined, 4, 1); });
C('11.ps-missing-max', function () { efx.graphics.createParticleSystem(tex, undefined, 1); });
C('11.ps-max-range', function () { efx.graphics.createParticleSystem(tex, 0, 1); });
C('11.ps-missing-lifetime', function () { efx.graphics.createParticleSystem(tex, 4); });
C('11.ps-lifetime-order', function () {
    efx.graphics.createParticleSystem(tex, 4, [2, 1]);
});
C('11.ps-unknown-field', function () {
    efx.graphics.createParticleSystem(tex, 4, 1, { bogus: 1 });
});
C('11.ps-bad-facing', function () {
    efx.graphics.createParticleSystem(tex, 4, 1, { facing: 'sideways' });
});
C('11.ps-plane-screen', function () {
    efx.graphics.createParticleSystem(tex, 4, 1, { space: 'screen', facing: 'plane' });
});
C('11.ps-set-max', function () {
    var p = efx.graphics.createParticleSystem(tex, 4, 1);
    p.set({ max: 0 });
});
var deadPs = efx.graphics.createParticleSystem(tex, 4, 1);
deadPs.destroy();
C('11.ps-destroyed-emit', function () { deadPs.emit(1); });
C('11.billboard-no-texture', function () { efx.graphics.drawBillboard(undefined, [0, 0, 0], { size: [1, 1] }); });
C('11.billboard-pos', function () { efx.graphics.drawBillboard(tex, [0, 0]); });
C('11.billboard-size', function () { efx.graphics.drawBillboard(tex, [0, 0, 0], { size: [0, 1] }); });
C('11.sprites-array', function () { efx.graphics.drawSprites(tex, 'nope'); });
C('11.sprite-xy', function () { efx.graphics.drawSprites(tex, [{ size: [1, 1] }]); });

/* ----------------------------------------------------------- physics (F12) */

C('12.createbody-noopts', function () { efx.physics.createBody(); });
C('12.createbody-noshape', function () { efx.physics.createBody({}); });
C('12.createcharacter-noopts', function () { efx.physics.createCharacter(0.4, 1.8, 5); });
C('12.createcharacter-noradius', function () { efx.physics.createCharacter(undefined, 2); });
C('12.createstaticmesh-noopts', function () { efx.physics.createStaticMesh(); });
C('12.createstaticmesh-nomesh', function () { efx.physics.createStaticMesh(5); });
C('12.applyimpulse-static', function () {
    efx.physics.createBody({ type: 'sphere', radius: 1 }).applyImpulse([0, 1, 0]);
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
    efx.physics.createBody({ type: 'sphere', radius: 1 }, { friction: '0.5' });
});
C('12.createbody-bad-radius', function () {
    efx.physics.createBody({ type: 'sphere', radius: 0 });
});
C('12.createbody-shape-unknown', function () {
    efx.physics.createBody({ type: 'sphere', radius: 1, nope: 1 });
});
var deadBody = efx.physics.createBody({ type: 'sphere', radius: 1 });
deadBody.destroy();
C('12.body-destroyed', function () { return deadBody.position; });
var deadChar = efx.physics.createCharacter(0.4, 1.8);
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
