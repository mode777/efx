// A period sky dome: a real CC0 equirectangular sky (Poly Haven
// "kloofendal_43d_clear_puresky", CC0 1.0) on an inverted, unlit sphere. The
// sky is camera-locked and drawn first with depthWrite:false, so it reads as
// an infinite background and never occludes the lit scene. The camera orbits:
// the sky turns with the view but never parallaxes.
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.95, 0.95, 1.0, 1] });

const skyTex = efx.graphics.createTexture(efx.graphics.loadImage('sky.jpg'), { wrap: 'clamp' });
const sky = efx.graphics.createMesh(
    efx.graphics.makeSphere({ radius: 100, segments: 32, inverted: true }));
sky.setSurfaceMaterial(0, {
    unlit: true,
    diffuse: { color: [1, 1, 1, 1], map: skyTex },
});

const ground = efx.graphics.createMesh(efx.graphics.makePlane({ size: 20 }));
ground.setSurfaceMaterial(0, { diffuse: { color: [0.32, 0.42, 0.26, 1] } });

const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.2 }));
cube.setSurfaceMaterial(0, {
    diffuse: { color: [0.85, 0.5, 0.28, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
});

let t = 0;
function update(dt) { t += dt; }

function render() {
    const camA = t * 0.3;
    const pos = [Math.sin(camA) * 6, 2.2, Math.cos(camA) * 6];
    const target = [0, 0.6, 0];
    efx.graphics.setCamera3D(pos, target, 55, { near: 0.1, far: 400 });

    // sky first, camera-locked, no depth write: it fills the background and
    // never occludes the scene drawn after it
    efx.graphics.drawMesh(sky, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), pos),
        depthWrite: false,
    });

    efx.graphics.drawMesh(ground);
    const spin = efx.math.mat4.rotate(efx.math.mat4.identity(), t * 40, [0, 1, 0]);
    efx.graphics.drawMesh(cube, {
        transform: efx.math.mat4.translate(spin, [0, 0.6, 0]),
    });
}
