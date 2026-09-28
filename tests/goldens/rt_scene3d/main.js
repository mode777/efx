// F5a golden: a lit 3D scene (point + directional light, ground plane and
// cube) rendered into a square 400x400 render target and sampled centered
// on the 640x480 frame. The square target proves the 3D projection aspect
// derives from the active rendering surface: the cube must stay
// undistorted inside the sampled area.
efx.setClearColor([0.05, 0.05, 0.1, 1]);
efx.setCamera3D({ pos: [3, 2.2, 4], target: [0, 0.4, 0], fov: 55 });
efx.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });
const cube = efx.createMesh(efx.makeCube({ size: 1 }));
const ground = efx.createMesh(efx.makePlane({ size: 4 }));
const rt = efx.createRenderTarget({ width: 400, height: 400 });
function update() {}
function render() {
    efx.beginRenderTarget(rt);
    efx.drawMesh(ground, {
                   transform: efx.mat4.translate(efx.mat4.identity(),
                                                 [0, -0.5, 0]) });
    efx.drawMesh(cube, {
                   transform: efx.mat4.translate(efx.mat4.identity(),
                                                 [0, 0.2, 0]) });
    efx.endRenderTarget();
    efx.setCamera2D({ frame: [640, 480] });
    efx.drawQuad(120, 40, rt, { size: [400, 400] });
}
