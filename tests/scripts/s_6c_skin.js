/*
 * F6c: skinned surface attributes (joints/weights) round-trip through
 * createMeshData/createMesh on both runtimes; the rig has no script surface.
 */
var P = [0, 0, 0, 1, 0, 0, 0, 1, 0];
var J = [0, 1, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0];
var W = [1, 0, 0, 0, 0.5, 0.5, 0, 0, 1, 0, 0, 0];

var md = efx.graphics.createMeshData([{ positions: P, joints: J, weights: W, indices: [0, 1, 2] }]);
if (md.surfaceCount !== 1) {
    throw new Error('skinned surfaceCount ' + md.surfaceCount);
}
if (md.joints !== undefined || md.clips !== undefined) {
    throw new Error('rig must be opaque');
}
var mesh = efx.graphics.createMesh(md);
if (mesh.surfaceCount !== 1) {
    throw new Error('skinned mesh surfaceCount');
}
mesh.destroy();
md.destroy();

var kinds = 0;
try {
    efx.graphics.createMeshData([{ positions: P, joints: J }]);
} catch (e) {
    kinds = (e instanceof RangeError) ? 1 : 2;
}
if (kinds !== 1) {
    throw new Error('unpaired joints did not throw RangeError (' + kinds + ')');
}

efx.log('s-6c-skin-ok');
