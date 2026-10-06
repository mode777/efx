[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxRenderTarget

# Interface: EfxRenderTarget

A GPU render target with color and depth attachments (opaque native-backed class).

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`

## Properties

### height

> `readonly` **height**: `number`

Target height in pixels. Throws `TypeError` when destroyed.

***

### width

> `readonly` **width**: `number`

Target width in pixels. Throws `TypeError` when destroyed.
