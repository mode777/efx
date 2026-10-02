[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxTexture

# Interface: EfxTexture

A GPU texture (opaque native-backed class).

## Properties

### height

> `readonly` **height**: `number`

Texture height in pixels. Throws `TypeError` when destroyed.

***

### width

> `readonly` **width**: `number`

Texture width in pixels. Throws `TypeError` when destroyed.

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`
