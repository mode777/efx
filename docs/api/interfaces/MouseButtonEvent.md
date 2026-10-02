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

Cursor x in surface (framebuffer) pixels, top-left origin.

***

### y

> **y**: `number`

Cursor y in surface (framebuffer) pixels, top-left origin, y down.
