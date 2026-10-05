// CommonJS modules (F10): this scene is split across files inside the sample's
// asset pack and composed with synchronous `require` from the resource root.
// palette.js supplies colors and Phong materials (relative specifier),
// orbit.js the motion helpers (extension-less specifier, .js fallback), and
// data/scene.json the background, camera, lights and orbit layout (JSON
// module). The entry registers update/render hooks from the composed data.

const palette = require('./lib/palette.js'); // relative module
const orbit = require('./lib/orbit'); // no extension -> deterministic .js fallback
const scene = require('./data/scene.json'); // JSON module -> parsed value

efx.graphics.setClearColor(scene.background);
efx.graphics.setCamera3D(scene.camera.pos, scene.camera.target, scene.camera.fov);
efx.graphics.setLight(0, scene.keyLight);
efx.graphics.setDirectionalLight(scene.fillLight);

const core = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.2 }));
core.setSurfaceMaterial(0, palette.material(palette.colors[0]));

const moons = scene.orbits.map((entry, i) => {
    const mesh = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 0.32, segments: 12 }));
    const color = palette.colors[(i + 1) % palette.colors.length];
    mesh.setSurfaceMaterial(0, palette.material(color));
    return mesh;
});

let t = 0;
efx.registerUpdateHook((dt) => {
    t += dt;
});

efx.registerRenderHook(() => {
    efx.graphics.drawMesh(core, {
        transform: efx.math.mat4.rotate(efx.math.mat4.identity(),
                                   orbit.spinDegrees(t, scene.coreSpin), [0, 1, 0]),
    });
    for (let i = 0; i < moons.length; i++) {
        efx.graphics.drawMesh(moons[i], {
            transform: efx.math.mat4.translate(efx.math.mat4.identity(),
                                           orbit.orbitPosition(t, scene.orbits[i])),
        });
    }
});
