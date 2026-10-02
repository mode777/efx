[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxMouse

# Interface: EfxMouse

Mouse queries and event subscriptions. Queries report current frame state;
`isPressed`/`isReleased` are one-frame edges. Each event registration
returns an idempotent unsubscribe function. All coordinates are surface
(framebuffer) pixels with a top-left origin and y down — the same space as
`drawQuad` and the 2D frame.

## Example

```js
efx.mouse.onMove((e) => { pointerX = e.x; pointerY = e.y; });
efx.mouse.onWheel((e) => { brush *= (1 - e.dy * 0.09); });

efx.registerUpdateHook(() => {
  if (efx.mouse.isDown('left')) well = 1;
});
```

## Properties

### delta

> `readonly` **delta**: [`Vec2`](../type-aliases/Vec2.md)

Cursor movement `[dx, dy]` for the current frame, in surface pixels.

***

### position

> `readonly` **position**: [`Vec2`](../type-aliases/Vec2.md)

Cursor position `[x, y]` in surface pixels.

***

### wheel

> `readonly` **wheel**: [`Vec2`](../type-aliases/Vec2.md)

Wheel delta `[dx, dy]` for the current frame.

***

### x

> `readonly` **x**: `number`

Cursor x in surface pixels.

***

### y

> `readonly` **y**: `number`

Cursor y in surface pixels.

## Methods

### isDown()

> **isDown**(`button`): `boolean`

Test whether a mouse button is currently held.

#### Parameters

##### button

[`EfxMouseButton`](../type-aliases/EfxMouseButton.md)

Button name to test.

#### Returns

`boolean`

`true` while `button` is held; throws `TypeError` for an unknown button.

***

### isPressed()

> **isPressed**(`button`): `boolean`

Test whether a mouse button transitioned down this frame.

#### Parameters

##### button

[`EfxMouseButton`](../type-aliases/EfxMouseButton.md)

Button name to test.

#### Returns

`boolean`

`true` on the frame `button` transitioned down.

***

### isReleased()

> **isReleased**(`button`): `boolean`

Test whether a mouse button transitioned up this frame.

#### Parameters

##### button

[`EfxMouseButton`](../type-aliases/EfxMouseButton.md)

Button name to test.

#### Returns

`boolean`

`true` on the frame `button` transitioned up.

***

### onDown()

> **onDown**(`fn`): () => `void`

Subscribe to button-down events.

#### Parameters

##### fn

(`e`) => `void`

Called with each button-down event, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`

***

### onMove()

> **onMove**(`fn`): () => `void`

Subscribe to move events.

#### Parameters

##### fn

(`e`) => `void`

Called with each move event, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`

***

### onUp()

> **onUp**(`fn`): () => `void`

Subscribe to button-up events.

#### Parameters

##### fn

(`e`) => `void`

Called with each button-up event, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`

***

### onWheel()

> **onWheel**(`fn`): () => `void`

Subscribe to wheel events.

#### Parameters

##### fn

(`e`) => `void`

Called with each wheel event, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`
