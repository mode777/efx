// material_alpha_modes golden: Material.alphaMode routing. All materials are
// unlit so expected colors are exact. Panels:
//   1) deferral — a blend-mode red panel recorded BEFORE a farther opaque
//      green panel: the green panel must survive (blend surfaces never write
//      depth) and the red panel composites over it in the transparent pass.
//   2) origin sort — a far blue blend panel recorded before a near yellow
//      blend panel: playback sorts back-to-front, so yellow composites over
//      blue (record order would give the opposite).
//   3) opaque saturation — an opaque panel with diffuse alpha 0.5 renders
//      fully opaque (alpha forced to 1), not blended onto the wall.
//   4) mask — a mask-mode panel with a half-transparent alphaMask cuts out
//      on the full rendering path (the wall shows through the cut half).
efx.graphics.setClearColor([0.02, 0.02, 0.04, 1]);
efx.graphics.setCamera3D([0, 0, 5], [0, 0, 0], 50);

function quad(size, z) {
    const h = size / 2;
    return {
        positions: [-h, -h, z, h, -h, z, h, h, z, -h, h, z],
        normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
        uvs: [0, 0, 1, 0, 1, 1, 0, 1],
        indices: [0, 1, 2, 0, 2, 3],
    };
}

function unlit(mat) {
    return Object.assign({ unlit: true }, mat);
}

const wall = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(14, 0)],
        [unlit({ diffuse: { color: [0.16, 0.22, 0.34, 1] } })]));

// 1) deferral: blend red first, opaque green (farther) second
const redBlend = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(2.4, 0)],
        [unlit({ diffuse: { color: [1, 0, 0, 0.5] }, alphaMode: 'blend' })]));
const greenOpaque = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(2.4, -1)],
        [unlit({ diffuse: { color: [0, 0.8, 0.2, 1] } })]));

// 2) origin sort: far blue recorded before near yellow, both blend
const blueFar = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(2.2, -0.5)],
        [unlit({ diffuse: { color: [0, 0, 1, 0.5] }, alphaMode: 'blend' })]));
const yellowNear = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(1.6, 0.9)],
        [unlit({ diffuse: { color: [1, 1, 0, 0.5] }, alphaMode: 'blend' })]));

// 3) opaque saturation: diffuse alpha 0.5 must render opaque
const satOpaque = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(1.2, 0.3)],
        [unlit({ diffuse: { color: [1, 0.2, 0.2, 0.5] } })]));

// 4) mask cutout: alphaMask alpha 0 over the left half, 1 over the right
const maskImg = efx.graphics.createImageData(2, 1,
    [0, 0, 0, 0, 255, 255, 255, 255]);
const maskTex = efx.graphics.createTexture(maskImg);
const maskPanel = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(1.8, 0.6)],
        [unlit({ diffuse: { color: [0, 1, 1, 1] }, alphaMode: 'mask',
                 alphaMask: maskTex })]));

function move(mesh, x, y) {
    return efx.math.mat4.translate(efx.math.mat4.identity(), [x, y, 0]);
}

function update() {}

function render() {
    efx.graphics.drawMesh(wall);
    // deferral pair (blend red recorded first, opaque green after it)
    efx.graphics.drawMesh(redBlend, { transform: move(redBlend, -3.2, 1.6) });
    efx.graphics.drawMesh(greenOpaque, { transform: move(greenOpaque, -3.2, 1.6) });
    // sort pair: near yellow recorded FIRST, far blue second — playback
    // sorts back-to-front, so blue composites first and yellow lands on top
    efx.graphics.drawMesh(yellowNear, { transform: move(yellowNear, 0.9, 0.2) });
    efx.graphics.drawMesh(blueFar, { transform: move(blueFar, 0.4, -0.2) });
    // opaque saturation + mask cutout
    efx.graphics.drawMesh(satOpaque, { transform: move(satOpaque, 3.4, 1.8) });
    efx.graphics.drawMesh(maskPanel, { transform: move(maskPanel, 3.2, -0.8) });
}
