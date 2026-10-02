# EmotionFX

An old-school, PS2-era 3D game engine: a fixed-function renderer, super
lightweight, scripted in ES6. Games are plain folders (or zips) of assets plus
a `main.js`, run by a single portable `player` binary — no IDE, no build step
for your game code.

```js
// main.js
efx.graphics.setCamera3D({ pos: [0, 1.6, 4.4], target: [0, 0, 0], fov: 60 });
efx.graphics.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });

const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.5 }));

let t = 0;
function update(dt) { t += dt; }
function render() {
    efx.graphics.drawMesh(cube, {
        transform: efx.math.mat4.rotate(efx.math.mat4.identity(), t * 35, [0, 1, 0]),
        color: [0.95, 0.5, 0.2, 1],
    });
}
```

## Try it in the browser

The [sample gallery](https://mode777.github.io/emotion-fx/) runs examples live
in the browser with an editable source pane, so you can see the API in action
without installing anything. The generated API reference is served alongside it
at [`/api`](https://mode777.github.io/emotion-fx/api/).

## Capabilities

- **Rendering** — fixed-function pipeline (scripts never see shaders), meshes
  with multiple surfaces, vertex colors, 2D quads and sprites, world-space
  billboards, particles, render-to-texture, and additive/subtractive blending.
- **Lighting** — exactly 4 point lights + 1 directional light, with a simple
  Phong material: ambient, diffuse, specular, and emissive channels, each with
  an optional map, plus alpha masks.
- **Post-processing** — a declarative full-screen effect chain (color filter,
  blur, bloom) at a configurable render scale.
- **Assets** — load everything from a resource root (folder or zip), including
  glTF 2.0 meshes, materials, textures, skinned rigs, and animation clips.
- **Animation** — CPU skinning and script-driven clip sampling/posing.
- **Text** — baked-font atlas drawing with wrapping, alignment, and
  measurement.
- **Input** — keyboard, mouse, and gamepad, with both polling queries and
  event callbacks.
- **Audio** — WAV/MP3 playback with engine-owned mixing: streamed sources and
  one-shot effects, no channels or voices in your code.
- **Physics** — sphere/box/capsule/triangle-mesh colliders, one script-stepped
  world with impulse dynamics, a capsule character controller, and
  raycast/overlap/shape-cast queries.
- **Scripting** — CommonJS modules loaded synchronously from the root;
  TypeScript is supported as an authoring language (`import`/`export` compiled
  to CommonJS). Scripts are pure ES6 with zero browser or Node dependencies.

## Get EmotionFX

- **In your browser:** open the [sample gallery](https://mode777.github.io/emotion-fx/).
- **Native player:** download the prebuilt archive for your platform from the
  [Releases page](https://github.com/mode777/emotion-fx/releases), unpack it,
  and run the `player` binary.
- **From source:** see [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Your first game

A game is a **resource root**: a folder (or a zip archive) containing a
`main.js` entry script plus any asset files it needs. The player loads the root
and runs `main.js`.

Create a folder with a `main.js`:

```js
let frames = 0;

// Called once per frame with the elapsed time in seconds.
function update(dt) {
    frames++;
    if (frames >= 60) {
        efx.quit(0);
    }
}

// Record draw calls here; the renderer replays them.
function render() {
    efx.graphics.setClearColor([0.05, 0.06, 0.1, 1]);
}
```

Run it by pointing the player at the resource root:

```sh
player path/to/your-game
```

A source build uses `build/player` in place of `player`.

Defining global `update`/`render` functions is load-time sugar; you can also
register hooks explicitly with `efx.registerUpdateHook(fn)` /
`efx.registerRenderHook(fn)`, which return unsubscribe functions. See
[`examples/hooks`](examples/hooks) for that form.

### Modules

Script files are CommonJS modules: `require('./thing')` returns a module's
`module.exports`, with module caching, circular-require support, `__esModule`
interop, and `.json` modules. There is no Node/npm compatibility — no
`node_modules`, no Node built-ins. TypeScript authors write `import`/`export`
and compile to CommonJS before packaging. See
[`docs/js-api.md`](docs/js-api.md) for the module model.

## The `efx` API

Everything is reached through one global `efx` object — no imports or setup.

- **Runtime** — `efx.log`, `efx.quit`, `efx.args`, and the hook registrars.
- **`efx.graphics`** — drawing, state, and resources: `setCamera2D` /
  `setCamera3D`, `drawQuad`, `drawMesh`, `drawBillboard`, `drawSprites`,
  `drawText`, `drawParticles`, `createMesh`, `createTexture`, `createFont`,
  render targets, lights, post effects, and more.
- **`efx.math`** — pure-JS `mat4`, `vec3`, and `quat` helpers.
- **`efx.io`** — synchronous resource loaders.
- **`efx.color`** — named color constants such as `white` and `black`.
- **`efx.keyboard` / `efx.mouse` / `efx.window` / `efx.gamepad`** — input and
  window metrics.
- **`efx.physics`** — the single collision world, bodies, the character
  controller, and spatial queries.
- **`efx.audio`** — load and play streamed or static sources.

## Run modes

```sh
player <resource-root>                 # windowed: run a game
player --script <file> [args…]         # headless: run one script and exit with its code
player --repl [<root>]                 # interactive console against the efx API
```

## Learn more

- [Sample gallery](https://mode777.github.io/emotion-fx/) — runnable,
  editable examples. Their sources are the curated sample directories under
  [`gallery/samples/curated/`](gallery/samples/curated).
- [`examples/hello`](examples/hello), [`examples/hooks`](examples/hooks), and
  [`examples/browser`](examples/browser) — minimal resource roots to start from.
- [`docs/api/`](docs/api/) — the generated per-symbol API reference.
- [`docs/js-api.md`](docs/js-api.md) — API design guidelines and conventions.
- [`vision.md`](vision.md) — the product vision behind the engine.

## Platform support

| Platform | Status |
|---|---|
| Windows | Native player |
| Linux | Native player |
| macOS | Native player |
| Web (Emscripten) | Browser player + sample gallery |

## Building from source / Contributing

Build instructions, the test and golden-image harness, the four-target CI gate,
and the repository/architecture guide live in
[`CONTRIBUTING.md`](CONTRIBUTING.md). Working conventions for agents and
maintainers are in [`AGENTS.md`](AGENTS.md), and required behavior is pinned by
the specs under [`openspec/`](openspec/).
