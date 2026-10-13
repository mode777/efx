[**EFX API**](../README.md)

***

[EFX API](../README.md) / Efx

# Interface: Efx

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

### noise

> `readonly` **noise**: [`EfxNoise`](EfxNoise.md)

Deterministic procedural noise.

***

### physics

> **physics**: [`EfxPhysics`](EfxPhysics.md)

The single physics world.

***

### time

> `readonly` **time**: [`EfxTime`](EfxTime.md)

Engine-owned script clock.

***

### window

> **window**: [`EfxWindow`](EfxWindow.md)

Read-only window metrics.
