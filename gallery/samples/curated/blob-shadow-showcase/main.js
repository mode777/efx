// A blob shadow under a moving character, drawn as a ground-plane billboard
// (facing:'plane') with alpha blending and a small lift off the floor. The
// radial shadow texture is the shared pre-built image (radial.png), tinted
// dark at draw time. Orbit the camera: the shadow stays flat on the ground
// and follows the character.
efx.graphics.setClearColor([0.05, 0.06, 0.1, 1]);
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.95, 0.95, 1.0, 1] });

const shadowTex = efx.graphics.createTexture(efx.graphics.loadImage('radial.png'));

const ground = efx.graphics.createMesh(efx.graphics.makePlane({ size: 20 }));
ground.setSurfaceMaterial(0, { diffuse: { color: [0.5, 0.52, 0.44, 1] } });

const hero = efx.graphics.createMesh(efx.graphics.makeCapsule({ radius: 0.4, height: 1.8 }));
hero.setSurfaceMaterial(0, {
    diffuse: { color: [0.85, 0.5, 0.35, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 24 },
});

let t = 0;
function update(dt) { t = efx.time.now; }

function render() {
    const a = t * 0.8;
    const hx = Math.sin(a) * 3;
    const hz = Math.cos(a) * 3;
    const bob = 0.9 + Math.sin(t * 3) * 0.15;

    const camA = t * 0.35;
    efx.graphics.setCamera3D([Math.sin(camA) * 7, 3.2, Math.cos(camA) * 7],
                             [0, 0.6, 0], 55);

    efx.graphics.drawMesh(ground);

    // blob shadow: flat on the ground, lifted a hair to avoid z-fighting,
    // dark color with alpha blending (never writes depth)
    efx.graphics.drawBillboard(shadowTex, [hx, 0.02, hz], {
        facing: 'plane',
        normal: [0, 1, 0],
        size: 0.8,
        color: [0, 0, 0, 1],
        blend: 'alpha',
    });

    efx.graphics.drawMesh(hero, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [hx, bob, hz]),
    });
}
