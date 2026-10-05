[**EFX API**](../README.md)

***

[EFX API](../README.md) / Efx

# Interface: Efx

The engine-provided script surface; the only global scripts use.

## Example

```js
// the smallest complete 3D scene (the "Hello Cube" sample)
efx.graphics.setClearColor([0.03, 0.04, 0.09, 1]);
efx.graphics.setCamera3D([0, 1.6, 4.2], [0, 0, 0], 60);
efx.graphics.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });

const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.4 }));
cube.setSurfaceMaterial(0, {
  ambient:  { color: [0.12, 0.12, 0.16, 1] },
  diffuse:  { color: [1, 1, 1, 1] },
  specular: { color: [1, 1, 1, 1], shininess: 32 },
  emissive: { color: [0, 0, 0, 1] },
});

let t = 0;
function update(dt) { t += dt; }
function render() {
  const model = efx.math.mat4.rotate(efx.math.mat4.identity(), t * 40, [0, 1, 0]);
  efx.graphics.drawMesh(cube, { transform: model, color: [0.95, 0.5, 0.2, 1] });
}
```

## Properties

### args

> `readonly` **args**: `string`[]

The host arguments passed to the script run (read-only). A fresh array is
returned on every access, so mutating it never affects the engine.

#### Example

```js
const argv = efx.args; // e.g. ['one', 'two'] for --script main.js one two
```

***

### audio

> **audio**: [`EfxAudio`](EfxAudio.md)

Streamed background music and sound effects.

***

### color

> `readonly` **color**: [`EfxColor`](EfxColor.md)

Named color constants.

***

### gamepad

> **gamepad**: [`EfxGamepad`](EfxGamepad.md)

Gamepad bank queries and connect/disconnect events.

***

### graphics

> `readonly` **graphics**: [`EfxGraphics`](EfxGraphics.md)

Graphics drawing, state, and resources.

***

### io

> `readonly` **io**: [`EfxIo`](EfxIo.md)

Synchronous resource loaders.

***

### keyboard

> **keyboard**: [`EfxKeyboard`](EfxKeyboard.md)

Keyboard queries and events.

***

### math

> `readonly` **math**: [`EfxMath`](EfxMath.md)

Pure-JS math helpers.

***

### mouse

> **mouse**: [`EfxMouse`](EfxMouse.md)

Mouse queries and events.

***

### physics

> **physics**: [`EfxPhysics`](EfxPhysics.md)

The single physics world.

***

### window

> **window**: [`EfxWindow`](EfxWindow.md)

Read-only window metrics.

## Methods

### log()

> **log**(`msg?`): `void`

Print a message to stdout followed by a newline and flush.

#### Parameters

##### msg?

`unknown`

Value to print; non-strings use their standard string representation, and omitting it prints an empty line.

#### Returns

`void`

***

### quit()

> **quit**(`code?`): `never`

Request engine termination with an exit code.

#### Parameters

##### code?

`number`

Exit code (default 0).

#### Returns

`never`

Never returns normally: the engine unwinds and exits with `code`.

***

### registerRenderHook()

> **registerRenderHook**(`fn`): () => `void`

Register a per-frame render hook.

#### Parameters

##### fn

() => `void`

Called once per frame after update hooks; takes no arguments.

#### Returns

An idempotent unsubscribe function.

() => `void`

***

### registerUpdateHook()

> **registerUpdateHook**(`fn`): () => `void`

Register a per-frame update hook.

#### Parameters

##### fn

(`dt`) => `void`

Called once per frame with `dt` seconds since the previous frame (0 on the first).

#### Returns

An idempotent unsubscribe function.

() => `void`
