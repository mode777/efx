[**EFX API**](../README.md)

***

[EFX API](../README.md) / SpecularChannel

# Interface: SpecularChannel

The specular Phong channel (adds a shininess exponent).

## Properties

### color

> **color**: [`Color`](../type-aliases/Color.md)

Specular color `[r, g, b, a]` (alpha ignored by shading).

***

### map?

> `optional` **map?**: [`EfxSample`](../type-aliases/EfxSample.md) \| `null`

Optional modulating texture (or render target); `null`/omitted means none.

***

### shininess?

> `optional` **shininess?**: `number`

Specular exponent (default 32).
