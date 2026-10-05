/*
 * F7: Mesh.pose + the skinned draw option through the --script run mode. The
 * root is the glTF fixture directory, so skin.gltf loads its rig; a
 * script-built mesh must reject both posing and skinned drawing.
 */
function kind(fn) {
    try {
        fn();
    } catch (e) {
        if (e instanceof TypeError) return 'TypeError';
        if (e instanceof RangeError) return 'RangeError';
        if (e instanceof Error) return 'Error';
        return 'other';
    }
    return 'none';
}

var mesh = efx.graphics.createMesh(efx.graphics.loadMeshData('skin.gltf'));

mesh.pose({ clip: 'move', time: 0.25 });
mesh.pose({ clip: 0, time: 0.5 });
mesh.pose([{ clip: 'move', time: 0.1, weight: 1 },
                    { clip: 'turn', time: 0.6, weight: 2 }]);
mesh.pose({ clip: 'move', time: 5.5 });
efx.graphics.drawMesh(mesh, { skinned: true });
efx.graphics.drawMesh(mesh);

if (kind(function () { mesh.pose({ clip: 'nope', time: 0 }); }) !== 'Error') {
    throw new Error('unknown clip name did not throw Error');
}
if (kind(function () { mesh.pose({ clip: 9, time: 0 }); }) !== 'RangeError') {
    throw new Error('clip index did not throw RangeError');
}
if (kind(function () { mesh.pose({ clip: 'move', time: 0, weight: -1 }); }) !== 'RangeError') {
    throw new Error('negative weight did not throw RangeError');
}
if (kind(function () { mesh.pose({ clip: 'move', time: 0, bogus: 1 }); }) !== 'TypeError') {
    throw new Error('unknown sample field did not throw TypeError');
}

var plain = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: [0, 0, 0, 1, 0, 0, 0, 1, 0], indices: [0, 1, 2] }]));
if (kind(function () { plain.pose({ clip: 0, time: 0 }); }) !== 'TypeError') {
    throw new Error('rig-less pose did not throw TypeError');
}
if (kind(function () { efx.graphics.drawMesh(plain, { skinned: true }); }) !== 'TypeError') {
    throw new Error('rig-less skinned draw did not throw TypeError');
}

plain.destroy();
mesh.destroy();
efx.log('s-7-skin-pose-ok');
