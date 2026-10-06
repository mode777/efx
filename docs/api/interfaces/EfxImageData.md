[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxImageData

# Interface: EfxImageData

Raw CPU pixels plus size and format (opaque native-backed class).

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`

## Properties

### height

> `readonly` **height**: `number`

Image height in pixels. Throws `TypeError` when destroyed.

***

### width

> `readonly` **width**: `number`

Image width in pixels. Throws `TypeError` when destroyed.
