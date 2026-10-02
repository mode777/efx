[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxKeyboard

# Interface: EfxKeyboard

Keyboard queries and event subscriptions. Queries report current frame
state; `isPressed`/`isReleased` are one-frame edges. Each event
registration returns an idempotent unsubscribe function.

## Example

```js
efx.keyboard.onDown((e) => {
  if (e.repeat) return;
  if (e.key === 'space') nova(pointerX, pointerY);
});

efx.registerUpdateHook(() => {
  if (efx.keyboard.isDown('left') || efx.keyboard.isDown('a')) wx -= 240;
});
```

## Methods

### isDown()

> **isDown**(`key`): `boolean`

Test whether a key is currently held.

#### Parameters

##### key

[`EfxKey`](../type-aliases/EfxKey.md)

Key name to test.

#### Returns

`boolean`

`true` while `key` is held; throws `TypeError` for an unknown key name.

***

### isPressed()

> **isPressed**(`key`): `boolean`

Test whether a key transitioned down this frame.

#### Parameters

##### key

[`EfxKey`](../type-aliases/EfxKey.md)

Key name to test.

#### Returns

`boolean`

`true` on the frame `key` transitioned down; throws `TypeError` for an unknown key.

***

### isReleased()

> **isReleased**(`key`): `boolean`

Test whether a key transitioned up this frame.

#### Parameters

##### key

[`EfxKey`](../type-aliases/EfxKey.md)

Key name to test.

#### Returns

`boolean`

`true` on the frame `key` transitioned up; throws `TypeError` for an unknown key.

***

### onChar()

> **onChar**(`fn`): () => `void`

Subscribe to decoded text-input events.

#### Parameters

##### fn

(`e`) => `void`

Called with each character event, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`

***

### onDown()

> **onDown**(`fn`): () => `void`

Subscribe to key-down events.

#### Parameters

##### fn

(`e`) => `void`

Called with each key-down event, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`

***

### onUp()

> **onUp**(`fn`): () => `void`

Subscribe to key-up events.

#### Parameters

##### fn

(`e`) => `void`

Called with each key-up event, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`
