# EFX

**EFX** is a lightweight, fixed-function 3D engine with an embedded ES6
scripting layer. It targets PS2-era graphics and is deliberately small: one
global `efx` object, no imports, no setup, and no browser or Node APIs. Game
scripts run unchanged on Windows, Linux, macOS, and the browser.

## Start with `efx`

Everything lives on the single global {@link efx}. It exposes the runtime
facilities (`log`, `quit`, `args`, and the frame hooks) and a set of domain
sub-namespaces:

- {@link EfxGraphics | efx.graphics} — 2D/3D drawing, cameras, lights,
  materials, text, post FX, and GPU resources.
- {@link EfxMath | efx.math} — pure-JS `mat4`, `vec3`, and `quat` helpers.
- {@link EfxIo | efx.io} — synchronous loaders for text and binary resources.
- {@link EfxPhysics | efx.physics} — the single physics world: bodies, a
  character controller, and queries.
- {@link EfxKeyboard | efx.keyboard}, {@link EfxMouse | efx.mouse},
  {@link EfxGamepad | efx.gamepad}, {@link EfxWindow | efx.window} — input
  queries and events.
- {@link EfxAudio | efx.audio} — streamed music and engine-mixed sound
  effects.
- {@link EfxColor | efx.color} — named color constants.

## The smallest complete scene

```js
// A lit, spinning cube on a dark stage.
efx.graphics.setClearColor([0.03, 0.04, 0.09, 1]);
efx.graphics.setCamera3D([0, 1.6, 4.2], [0, 0, 0], 60);
efx.graphics.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });

const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.4 }));
cube.setSurfaceMaterial(0, {
  ambient:  { color: [0.12, 0.12, 0.16, 1] },
  diffuse:  { color: efx.color.white },
  specular: { color: efx.color.white, shininess: 32 },
  emissive: { color: efx.color.black },
});

let t = 0;
function update(dt) { t += dt; }
function render() {
  const spin = efx.math.mat4.rotate(efx.math.mat4.identity(), t * 40, [0, 1, 0]);
  const tilt = efx.math.mat4.rotate(spin, 18, [1, 0, 0]);
  efx.graphics.drawMesh(cube, { transform: tilt, color: [0.95, 0.5, 0.2, 1] });
}
```

## Going deeper

This reference is generated from the type document
`gallery/src/api/efx.d.ts`, which is the single source of truth for every
symbol below. The conventions and design rules behind the API are documented
in the repository guidelines `docs/js-api.md`.
