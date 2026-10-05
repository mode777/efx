// Four colored point lights orbiting a Phong sphere in real time.
efx.graphics.setClearColor([0.01, 0.01, 0.02, 1]);
efx.graphics.setCamera3D([0, 0, 5], [0, 0, 0], 55);

const ball = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 1.6, segments: 32 }));
efx.graphics.setMeshSurfaceMaterial(ball, 0, {
    ambient:  { color: [0.02, 0.02, 0.03, 1] },
    diffuse:  { color: [0.6, 0.6, 0.65, 1] },
    specular: { color: efx.color.white, shininess: 48 },
    emissive: { color: efx.color.black },
});

const colors = [
    [1, 0.2, 0.2, 1],
    [0.2, 1, 0.3, 1],
    [0.3, 0.4, 1, 1],
    [1, 0.9, 0.3, 1],
];

let t = 0;
function update(dt) { t += dt; }
function render() {
    for (let i = 0; i < 4; i++) {
        const a = t * 0.9 + i * (Math.PI / 2);
        efx.graphics.setLight(i, {
            pos: [Math.cos(a) * 3, Math.sin(a * 1.3) * 1.5, 2.5],
            color: colors[i],
            range: 9,
        });
    }
    efx.graphics.drawMesh(ball);
}
