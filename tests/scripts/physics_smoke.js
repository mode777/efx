/*
 * F12 portable physics smoke test. Runs unchanged on the desktop quickjs
 * runtime and the Emscripten host-engine bridge; it exercises settle, impulse,
 * sensors, the capsule character (slide + one-way push), and the spatial
 * queries, and prints one deterministic success line.
 */
function near(a, b, eps) {
    return Math.abs(a - b) <= eps;
}
function check(cond, msg) {
    if (!cond) {
        throw new Error('physics-smoke: ' + msg);
    }
}

efx.physics.clear();
check(near(efx.physics.gravity[1], -9.81, 0.001), 'default gravity');
efx.physics.gravity = [0, -10, 0];
efx.physics.iterations = 8;
check(efx.physics.iterations === 8, 'iterations');

/* a dynamic box settles on the ground and reports its contact */
var ground = efx.physics.createBody({
    shape: { type: 'box', size: [20, 1, 20] },
    position: [0, -0.5, 0],
});
var box = efx.physics.createBody({
    dynamic: true, mass: 1, friction: 0.6, restitution: 0,
    shape: { type: 'box', size: [1, 1, 1] },
    position: [0, 1, 0],
});
for (var i = 0; i < 240; i++) {
    efx.physics.step(1 / 60);
}
check(near(box.position[1], 0.5, 0.05), 'box rest y=' + box.position[1]);
check(box.contacts.length >= 1, 'box has a contact');
check(box.contacts[0].body === ground, 'contact identity');
check(box.contacts[0].normal[1] > 0.9, 'contact normal points up');

/* impulse changes velocity immediately */
var ball = efx.physics.createBody({
    dynamic: true, mass: 2,
    shape: { type: 'sphere', radius: 0.4 },
    position: [3, 5, 0],
});
ball.applyImpulse([0, 4, 0]);
check(near(ball.velocity[1], 2, 0.001), 'impulse / mass');
var y0 = ball.position[1];
efx.physics.step(1 / 60);
check(ball.position[1] > y0, 'impulse advances position');

/* a sensor is reported but does not block */
var sensor = efx.physics.createBody({
    sensor: true,
    shape: { type: 'box', size: [4, 1, 4] },
    position: [6, 2, 0],
});
var probe = efx.physics.createBody({
    dynamic: true, mass: 1,
    shape: { type: 'sphere', radius: 0.4 },
    position: [6, 4, 0],
});
var sawSensor = false;
for (var s = 0; s < 120; s++) {
    efx.physics.step(1 / 60);
    var cs = probe.contacts;
    for (var k = 0; k < cs.length; k++) {
        if (cs[k].sensor) {
            sawSensor = true;
        }
    }
}
check(sawSensor, 'sensor reported');
check(probe.position[1] < 1, 'sensor did not block');

/* the character lands on the floor and slides */
var ch = efx.physics.createCharacter({
    radius: 0.4, height: 1.8, position: [0, 1, 0],
});
var land = ch.moveAndSlide([0, -0.5, 0]);
check(land.onFloor, 'character on floor');
var beforeX = ch.position[0];
ch.moveAndSlide([1, 0, 0]);
check(ch.position[0] > beforeX, 'character moved');

/* one-way push: the character drives a crate through its script velocity */
var crate = efx.physics.createBody({
    dynamic: true, mass: 1,
    shape: { type: 'box', size: [1, 1, 1] },
    position: [2.5, 0.5, 0],
});
ch.moveAndSlide([1, 0, 0]);
ch.velocity = [3, 0, 0];
for (var p = 0; p < 30; p++) {
    efx.physics.step(1 / 60);
}
check(crate.position[0] > 2.5 || crate.velocity[0] > 0.1, 'crate pushed');

/* queries: raycast, overlap, shapeCast */
var hit = efx.physics.raycast([0, 5, 0], [0, -1, 0], { maxDistance: 20 });
check(hit !== null && hit.body !== null, 'raycast hit');
check(efx.physics.raycast([0, 5, 0], [0, 1, 0], { maxDistance: 3 }) === null,
      'raycast miss');
var ov = efx.physics.overlap({ type: 'sphere', radius: 1 },
                             { position: [0, 0.5, 0] });
check(ov.length >= 1, 'overlap');
var sc = efx.physics.shapeCast({ type: 'sphere', radius: 0.5 }, [0, 5, 0],
                               [0, -4, 0]);
check(sc !== null && sc.fraction >= 0 && sc.fraction <= 1, 'shapeCast');

/* validation */
var threw = false;
try {
    efx.physics.createBody({ shape: { type: 'sphere', radius: 0 } });
} catch (e) {
    threw = e instanceof RangeError;
}
check(threw, 'invalid radius throws RangeError');
threw = false;
try {
    efx.physics.raycast([0, 0, 0], [1, 0, 0]);
} catch (e) {
    threw = e instanceof TypeError;
}
check(threw, 'missing maxDistance throws TypeError');
threw = false;
try {
    efx.physics.createBody({ shape: { type: 'sphere', radius: 1, nope: 1 } });
} catch (e) {
    threw = e instanceof TypeError;
}
check(threw, 'unknown shape field throws TypeError');

/* physics-tunneling: a thin static mesh floor is not skipped at a large dt */
efx.physics.clear();
efx.physics.gravity = [0, -9.81, 0];
var planeMesh = efx.createMesh(efx.makePlane({ size: 10 }));
/* the Body wrapper owns the native collider: keep it referenced or the GC
 * finalizer removes the floor */
var floorBody = efx.physics.createStaticMesh(planeMesh, { friction: 0.5 });
var faller = efx.physics.createBody({
    dynamic: true, mass: 1, friction: 0.5, restitution: 0,
    shape: { type: 'box', size: [0.8, 0.8, 0.8] },
    position: [0, 4, 0],
});
for (var q = 0; q < 60; q++) {
    efx.physics.step(0.1); /* the maximum accepted dt */
}
check(near(faller.position[1], 0.4, 0.05),
      'thin mesh floor rest y=' + faller.position[1]);

efx.physics.clear();
efx.log('s-12-physics-ok');
