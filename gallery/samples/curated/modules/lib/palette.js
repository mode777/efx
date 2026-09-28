'use strict';

// F10 sample module: the scene palette and the Phong material builder. Named
// exports (the plain CommonJS `exports` object), required by the entry with a
// relative specifier.

exports.colors = [
    [0.95, 0.52, 0.20, 1],
    [0.30, 0.68, 1.00, 1],
    [0.72, 0.38, 0.95, 1],
    [0.38, 0.88, 0.55, 1],
];

exports.shade = function (color, k) {
    return [color[0] * k, color[1] * k, color[2] * k, color[3]];
};

exports.material = function (color) {
    return {
        ambient: { color: exports.shade(color, 0.16) },
        diffuse: { color: color },
        specular: { color: [1, 1, 1, 1], shininess: 40 },
        emissive: { color: [0, 0, 0, 1] },
    };
};
