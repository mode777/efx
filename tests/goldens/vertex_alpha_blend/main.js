// vertex_alpha_blend golden: full-alpha supports transparent vertex colors.
// Two identical planes whose vertex colors carry an alpha gradient
// (alpha 1 at the left edge to alpha 0 at the right edge) sit over a grey
// backdrop:
//   left  — alphaMode 'blend': the gradient composites (solid red fading
//           into the backdrop toward the right edge);
//   right — default 'opaque': the same vertices render fully opaque (alpha
//           saturated to 1), a uniform red panel.
// Materials are unlit so the blended colors are exact.
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

const backdrop = efx.graphics.createMesh(
    efx.graphics.createMeshData([quad(14, 0)],
        [{ unlit: true, diffuse: { color: [0.25, 0.25, 0.28, 1] } }]));

// vertex alpha gradient: 1, 2/3, 1/3, 0 across the four columns; rgb = red
function gradPlane() {
    const q = quad(2.6, 0.5);
    q.colors = [1, 0, 0, 1, 1, 0, 0, 0.666, 1, 0, 0, 0.333, 1, 0, 0, 0];
    return q;
}

const blendPanel = efx.graphics.createMesh(
    efx.graphics.createMeshData([gradPlane()],
        [{ unlit: true, diffuse: { color: [1, 1, 1, 1] },
           alphaMode: 'blend' }]));
const opaquePanel = efx.graphics.createMesh(
    efx.graphics.createMeshData([gradPlane()],
        [{ unlit: true, diffuse: { color: [1, 1, 1, 1] } }]));

function move(x, y) {
    return efx.math.mat4.translate(efx.math.mat4.identity(), [x, y, 0]);
}

function update() {}

function render() {
    efx.graphics.drawMesh(backdrop);
    efx.graphics.drawMesh(blendPanel, { transform: move(-1.6, 0) });
    efx.graphics.drawMesh(opaquePanel, { transform: move(1.6, 0) });
}
