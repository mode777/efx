// material_alpha_modes golden: Material.alphaMode routing. All materials are
// unlit so expected colors are exact. The backdrop wall sits at z=-2, behind
// every panel. Panels:
//   1) deferral — a blend-mode red panel recorded BEFORE a farther opaque
//      green panel: the green panel must survive (blend surfaces never write
//      depth) and the red panel composites over it in the transparent pass
//      (olive where they overlap, bare green at the border).
//   2) origin sort — a near yellow blend panel recorded BEFORE a far blue
//      blend panel: playback sorts back-to-front, so blue composites first
//      and yellow lands on top (record order would give the opposite).
//   3) opaque saturation — an opaque panel with diffuse alpha 0.5 renders
//      fully opaque (alpha forced to 1), not blended onto the wall.
//   4) mask — a mask-mode panel with a half-transparent alphaMask cuts out
//      on the full rendering path: the wall shows through the discarded left
//      half, the kept right half is opaque cyan.
efx.graphics.setClearColor([0.02, 0.02, 0.04, 1]);
efx.graphics.setCamera3D([0, 0, 5], [0, 0, 0], 50);

function quad(size) {
    const h = size / 2;
    return {
        positions: [-h, -h, 0, h, -h, 0, h, h, 0, -h, h, 0],
        normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
        uvs: [0, 0, 1, 0, 1, 1, 0, 1],
        indices: [0, 1, 2, 0, 2, 3],
    };
}

function unlit(mat) {
    return Object.assign({ unlit: true }, mat);
}

const wall = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(14)],
        [unlit({ diffuse: { color: [0.16, 0.22, 0.34, 1] } })]));

// 1) deferral: blend red first, opaque green (farther) second
const redBlend = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(2.4)],
        [unlit({ diffuse: { color: [1, 0, 0, 0.5] }, alphaMode: 'blend' })]));
const greenOpaque = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(2.4)],
        [unlit({ diffuse: { color: [0, 0.8, 0.2, 1] } })]));

// 2) origin sort: near yellow recorded before far blue, both blend
const yellowNear = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(1.6)],
        [unlit({ diffuse: { color: [1, 1, 0, 0.5] }, alphaMode: 'blend' })]));
const blueFar = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(2.2)],
        [unlit({ diffuse: { color: [0, 0, 1, 0.5] }, alphaMode: 'blend' })]));

// 3) opaque saturation: diffuse alpha 0.5 must render opaque
const satOpaque = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(1.2)],
        [unlit({ diffuse: { color: [1, 0.2, 0.2, 0.5] } })]));

// 4) mask cutout: alphaMask alpha 0 over the left half, 1 over the right
const maskImg = efx.graphics.createImageData(2, 1,
    [0, 0, 0, 0, 255, 255, 255, 255]);
const maskTex = efx.graphics.createTexture(maskImg);
const maskPanel = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(1.8)],
        [unlit({ diffuse: { color: [0, 1, 1, 1] }, alphaMode: 'mask',
                 alphaMask: maskTex })]));

function move(x, y, z) {
    return efx.math.mat4.translate(efx.math.mat4.identity(), [x, y, z]);
}

function update() {}

function render() {
    efx.graphics.drawMesh(wall, { transform: move(0, 0, -2) });
    // deferral pair (blend red recorded first, opaque green after it)
    efx.graphics.drawMesh(redBlend, { transform: move(-1.8, 1.2, 0) });
    efx.graphics.drawMesh(greenOpaque, { transform: move(-1.8, 1.2, -1) });
    // sort pair (near yellow recorded before far blue)
    efx.graphics.drawMesh(yellowNear, { transform: move(0.9, 0.2, 0.9) });
    efx.graphics.drawMesh(blueFar, { transform: move(0.4, -0.2, -0.5) });
    // opaque saturation + mask cutout
    efx.graphics.drawMesh(satOpaque, { transform: move(2.4, 1.6, 0.3) });
    efx.graphics.drawMesh(maskPanel, { transform: move(1.7, -1.2, 0.6) });
}
