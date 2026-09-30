// F12 physics showcase: a kinematic capsule character walks (moveAndSlide) a
// course of solid static geometry — walls, a ramp, and a step — triggers a
// sensor volume, and pushes falling/pushable dynamic props, with a raycast for
// line-of-sight. Everything is procedural; no asset pack.
efx.setClearColor([0.05, 0.06, 0.1, 1]);
efx.setCamera3D({ pos: [-7, 5.5, 9], target: [2, 1, 0], fov: 55 });
efx.setLight(0, { pos: [4, 8, 6], color: [1, 0.97, 0.9, 1], range: 60 });
efx.setDirectionalLight({ dir: [-0.3, -1, -0.2], color: [0.2, 0.22, 0.3, 1] });

efx.physics.gravity = [0, -9.81, 0];
efx.physics.clear();

// ---- static course -------------------------------------------------------
function boxBody(size, pos) {
    return efx.physics.createBody({
        shape: { type: 'box', size: size }, position: pos,
    });
}
const ground = boxBody([40, 1, 40], [0, -0.5, 0]);
const backWall = boxBody([40, 3, 0.5], [0, 1.5, -3]);
const sideWall = boxBody([0.5, 3, 8], [7, 1.5, 0]);
const step = boxBody([2, 0.3, 3], [3, 0.15, 2.2]);

// a tilted ramp mesh (rises along +x) as a static triangle-mesh collider
const rampData = efx.createMeshData({
    positions: [-2, 0, -1.6, 4, 2.4, -1.6, 4, 2.4, 1.6, -2, 0, 1.6],
    indices: [0, 2, 1, 0, 3, 2],
    normals: [-0.49, 0.87, 0, -0.49, 0.87, 0, -0.49, 0.87, 0, -0.49, 0.87, 0],
});
const rampMesh = efx.createMesh(rampData);
const ramp = efx.physics.createStaticMesh(rampMesh, { friction: 0.8 });

// a sensor trigger pad
const sensorPos = [-2, 1, 0];
const sensor = efx.physics.createBody({
    sensor: true, shape: { type: 'box', size: [2, 2, 2] }, position: sensorPos,
});

// ---- dynamic props -------------------------------------------------------
const props = [];
for (let i = 0; i < 5; i++) {
    props.push(efx.physics.createBody({
        dynamic: true, mass: 1, friction: 0.6, restitution: 0.15,
        shape: { type: 'box', size: [0.8, 0.8, 0.8] },
        position: [0.5 + i * 1.1, 3 + i * 0.6, -1],
    }));
}
const ball = efx.physics.createBody({
    dynamic: true, mass: 2, restitution: 0.5,
    shape: { type: 'sphere', radius: 0.5 }, position: [6, 4, 2],
});

// ---- the character -------------------------------------------------------
const hero = efx.physics.createCharacter({
    radius: 0.4, height: 1.8, position: [-5, 1, 0],
    floorMaxAngle: 50, stepHeight: 0.35, floorSnapLength: 0.15,
});
const heroVel = [4, 0, 0];
let triggered = false;
let sawWall = false;
let time = 0;

function update(dt) {
    time += dt;
    // drive the character with the script-supplied motion (moveAndSlide does
    // not integrate velocity); it walks +x, snapping to floors and stepping up
    heroVel[1] -= 9.81 * dt;
    const move = hero.moveAndSlide([heroVel[0] * dt, heroVel[1] * dt, 0]);
    if (move.onFloor) { heroVel[1] = 0; }
    hero.velocity = [heroVel[0], heroVel[1], 0];

    efx.physics.step(dt);

    // sensor trigger via overlap
    const hits = efx.physics.overlap({ type: 'box', size: [2, 2, 2] },
                                     { position: sensorPos });
    if (hits.indexOf(hero) >= 0) { triggered = true; }

    // line-of-sight raycast just ahead of the character
    const p = hero.position;
    const ray = efx.physics.raycast([p[0], p[1], p[2]],
                                    [Math.cos(time * 0.7), 0, Math.sin(time * 0.7)],
                                    { maxDistance: 6 });
    sawWall = !!(ray && ray.body === sideWall);
}

// ---- rendering -----------------------------------------------------------
const cube = efx.createMesh(efx.makeCube());
const sphere = efx.createMesh(efx.makeSphere());
const capsule = efx.createMesh(efx.makeCapsule({ radius: 0.4, height: 1.8 }));
efx.setMeshSurfaceMaterial(cube, 0, {
    ambient: { color: [0.12, 0.12, 0.16, 1] },
    diffuse: { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
});
efx.setMeshSurfaceMaterial(rampMesh, 0, {
    ambient: { color: [0.12, 0.12, 0.16, 1] },
    diffuse: { color: [0.9, 0.9, 0.95, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 16 },
});
efx.setMeshSurfaceMaterial(sphere, 0, {
    ambient: { color: [0.1, 0.1, 0.14, 1] },
    diffuse: { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 48 },
});
efx.setMeshSurfaceMaterial(capsule, 0, {
    ambient: { color: [0.12, 0.12, 0.16, 1] },
    diffuse: { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
});

function trs(pos, scale) {
    const t = efx.mat4.translate(efx.mat4.identity(), pos);
    return efx.mat4.scale(t, scale);
}
function drawBody(mesh, body, scale, color) {
    efx.drawMesh(mesh, { transform: trs(body.position, scale), color: color });
}

function render() {
    efx.drawMesh(cube, {
        transform: trs([0, -0.5, 0], [40, 1, 40]),
        color: [0.16, 0.18, 0.2, 1],
    });
    efx.drawMesh(cube, {
        transform: trs([0, 1.5, -3], [40, 3, 0.5]),
        color: [0.24, 0.26, 0.32, 1],
    });
    efx.drawMesh(cube, {
        transform: trs([7, 1.5, 0], [0.5, 3, 8]),
        color: [0.24, 0.26, 0.32, 1],
    });
    efx.drawMesh(cube, {
        transform: trs([3, 0.15, 2.2], [2, 0.3, 3]),
        color: [0.3, 0.34, 0.4, 1],
    });
    efx.drawMesh(rampMesh, { transform: efx.mat4.identity(),
                             color: [0.55, 0.6, 0.68, 1] });

    efx.drawMesh(cube, {
        transform: trs(sensorPos, [2, 2, 2]),
        color: triggered ? [0.2, 0.9, 0.4, 0.35] : [0.9, 0.8, 0.2, 0.25],
    });

    for (let i = 0; i < props.length; i++) {
        drawBody(cube, props[i], [0.8, 0.8, 0.8],
                 [0.85, 0.5 + i * 0.08, 0.25, 1]);
    }
    // sphere mesh has unit radius; scale by the collider radius
    drawBody(sphere, ball, [0.5, 0.5, 0.5], [0.3, 0.7, 1, 1]);

    // the character is a capsule matching its collision volume exactly
    drawBody(capsule, hero, [1, 1, 1], [0.95, 0.9, 0.3, 1]);
}
