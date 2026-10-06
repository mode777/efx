[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxGamepad

# Interface: EfxGamepad

Gamepad queries and connect/disconnect events over a fixed engine-owned
bank of four pad slots, reported by index.

## Example

```js
efx.gamepad.onConnect((pad) => efx.log('pad: ' + pad.name));
efx.registerUpdateHook(() => {
  const pad = efx.gamepad.get(0);
  if (!pad) return;
  if (pad.isDown('rightTrigger')) boost = 1;
  x += pad.axis('leftX') * speed * dt;
});
```

## Methods

### get()

> **get**(`index`): [`EfxGamepadView`](EfxGamepadView.md) \| `null`

Get the pad view for a slot.

#### Parameters

##### index

`number`

Slot index (0-based).

#### Returns

[`EfxGamepadView`](EfxGamepadView.md) \| `null`

The pad view, or `null` when the slot is empty; throws `TypeError` for a non-numeric index.

***

### onConnect()

> **onConnect**(`fn`): () => `void`

Subscribe to pad-connect events.

#### Parameters

##### fn

(`pad`) => `void`

Called with the pad view when a pad connects, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`

***

### onDisconnect()

> **onDisconnect**(`fn`): () => `void`

Subscribe to pad-disconnect events.

#### Parameters

##### fn

(`pad`) => `void`

Called with the pad view when a pad disconnects, before the update hooks.

#### Returns

An idempotent unsubscribe function.

() => `void`

## Properties

### count

> `readonly` **count**: `number`

Number of currently connected pads.
