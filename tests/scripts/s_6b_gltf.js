/*
 * F6b: loadMeshData imports a glTF mesh from the resource root, createMesh
 * uploads it, and malformed/selection failures throw Error.
 */
var md = efx.loadMeshData('gltf_probe.glb');
if (md.surfaceCount !== 2) {
    throw new Error('surfaceCount ' + md.surfaceCount);
}
var mesh = efx.createMesh(md);
if (mesh.surfaceCount !== 2) {
    throw new Error('mesh surfaceCount ' + mesh.surfaceCount);
}
mesh.destroy();
md.destroy();

var byName = efx.loadMeshData('gltf_probe.glb', { mesh: 'm' });
if (byName.surfaceCount !== 2) {
    throw new Error('by-name surfaceCount');
}
byName.destroy();

var kind = 0;
try {
    efx.loadMeshData('gltf_corrupt.gltf');
} catch (e) {
    kind = (e instanceof Error) ? 1 : 2;
}
if (kind !== 1) {
    throw new Error('corrupt import did not throw Error (' + kind + ')');
}

kind = 0;
try {
    efx.loadMeshData('gltf_probe.glb', { nope: 1 });
} catch (e) {
    kind = (e instanceof TypeError) ? 1 : 2;
}
if (kind !== 1) {
    throw new Error('unknown option did not throw TypeError (' + kind + ')');
}

efx.log('s-6b-gltf-ok');
