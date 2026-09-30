'use strict';

// F10 sample module: pure motion helpers. Required extension-lessly
// (`require('./lib/orbit')`) to show the deterministic `.js` fallback.

exports.spinDegrees = function (t, degPerSecond) {
    return t * degPerSecond;
};

exports.orbitPosition = function (t, orbit) {
    var a = orbit.phase + t * orbit.speed;
    return [Math.cos(a) * orbit.radius, orbit.height, Math.sin(a) * orbit.radius];
};
