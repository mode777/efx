// F5a DIAGNOSTIC 2 (temporary): the same 8x8 red|blue render target shown
// three ways — quad sample (full), quad sample (left-half sourceRect), and
// mesh diffuse map. Isolates target content vs quad sampling vs mesh-map
// sampling. Camera state is set once at load: an explicit setCamera2D
// inside render() would persist into the next frame's target segment and
// shrink the fill quads (the default frame must stay the target extent).
// Expected when everything works: three panels, each a hard red|blue split
// (a ~1-texel blend at the seam from linear filtering).
efx.setClearColor([0, 1, 0, 1]);
const red = efx.createTexture(efx.createImageData({
    width: 1, height: 1, pixels: [255, 0, 0, 255],
}));
const blue = efx.createTexture(efx.createImageData({
    width: 1, height: 1, pixels: [40, 80, 255, 255],
}));
const map = efx.createRenderTarget({ width: 8, height: 8 });
const S = 2.2;
const plane = efx.createMesh(efx.createMeshData({
    positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0],
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
    uvs: [0, 0, 1, 0, 1, 1, 0, 1],
    indices: [0, 1, 2, 0, 2, 3],
}));
efx.setMeshSurfaceMaterial(plane, 0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [1, 1, 1, 1], map: map },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
efx.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 55 });
efx.setLight(0, null);
efx.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
function update() {}
function render() {
    efx.beginRenderTarget(map);
    efx.drawQuad(0, 0, red, { size: [4, 8] });
    efx.drawQuad(4, 0, blue, { size: [4, 8] });
    efx.endRenderTarget();
    efx.drawQuad(20, 120, map, { size: [180, 180] });
    efx.drawQuad(220, 120, map, {
        sourceRect: { x: 0, y: 0, w: 4, h: 8 },
        size: [180, 180],
    });
    efx.drawMesh({ mesh: plane,
                   transform: efx.mat4.translate(efx.mat4.identity(),
                                                 [1.7, 0, 0]) });
}
