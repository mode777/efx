[**EFX API**](../README.md)

***

[EFX API](../README.md) / MouseButtonEvent

# Interface: MouseButtonEvent

Payload of a mouse button event.

## Properties

### button

> **button**: [`EfxMouseButton`](../type-aliases/EfxMouseButton.md)

The button that changed state.

***

### mods

> **mods**: [`EfxMod`](../type-aliases/EfxMod.md)[]

Modifier keys held at the moment of the event.

***

### x

> **x**: `number`

Cursor x in frame pixels (surface pixels when no 2D frame was set).

***

### y

> **y**: `number`

Cursor y in frame pixels (surface pixels when no 2D frame was set).
