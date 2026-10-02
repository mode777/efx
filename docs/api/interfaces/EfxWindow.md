[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxWindow

# Interface: EfxWindow

Read-only window metrics, in surface (framebuffer) pixels. On high-DPI
displays the surface is larger than the logical window; `dpiScale` is the
surface-to-logical ratio.

## Example

```js
const [w, h] = efx.window.size;
const logicalW = w / efx.window.dpiScale;
```

## Properties

### dpiScale

> `readonly` **dpiScale**: `number`

Surface-to-logical ratio on high-DPI displays (1.0 otherwise).

***

### height

> `readonly` **height**: `number`

Window height in surface pixels.

***

### size

> `readonly` **size**: [`Vec2`](../type-aliases/Vec2.md)

Window size `[width, height]` in surface pixels.

***

### width

> `readonly` **width**: `number`

Window width in surface pixels.
