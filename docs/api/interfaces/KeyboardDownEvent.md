[**EFX API**](../README.md)

***

[EFX API](../README.md) / KeyboardDownEvent

# Interface: KeyboardDownEvent

Payload of a key-down event.

## Properties

### key

> **key**: [`EfxKey`](../type-aliases/EfxKey.md)

The key that went down.

***

### mods

> **mods**: [`EfxMod`](../type-aliases/EfxMod.md)[]

Modifier keys held at the moment of the event.

***

### repeat

> **repeat**: `boolean`

`true` when this is an auto-repeat rather than the initial press.
