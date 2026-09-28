// CommonJS modules (F10): this scene is split across files inside the sample's
// asset pack and composed with synchronous `require` from the resource root.
// palette.js supplies colors and Phong materials (relative specifier),
// orbit.js the motion helpers (extension-less specifier, .js fallback), and
// data/scene.json the background, camera, lights and orbit layout (JSON
// module). The entry registers update/render hooks from the composed data.

const palette = require('./lib/palette.js'); // relative module
const orbit = require('./lib/orbit'); // no extension -> deterministic .js fallback
const scene = require('./data/scene.json'); // JSON module -> parsed value

efx.setClearColor(scene.background);
efx.setCamera3D(scene.camera);
efx.setLight(0, scene.keyLight);
efx.setDirectionalLight(scene.fillLight);

const core = efx.createMesh(efx.makeCube({ size: 1.2 }));
efx.setMeshSurfaceMaterial(core, 0, palette.material(palette.colors[0]));

const moons = scene.orbits.map((entry, i) => {
    const mesh = efx.createMesh(efx.makeSphere({ radius: 0.32, segments: 12 }));
    const color = palette.colors[(i + 1) % palette.colors.length];
    efx.setMeshSurfaceMaterial(mesh, 0, palette.material(color));
    return mesh;
});

let t = 0;
efx.registerUpdateHook((dt) => {
    t += dt;
});

efx.registerRenderHook(() => {
    efx.drawMesh(core, {
        transform: efx.mat4.rotate(efx.mat4.identity(),
                                   orbit.spinDegrees(t, scene.coreSpin), [0, 1, 0]),
    });
    for (let i = 0; i < moons.length; i++) {
        efx.drawMesh(moons[i], {
            transform: efx.mat4.translate(efx.mat4.identity(),
                                           orbit.orbitPosition(t, scene.orbits[i])),
        });
    }
});
