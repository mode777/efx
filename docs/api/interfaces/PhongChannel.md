[**EFX API**](../README.md)

***

[EFX API](../README.md) / PhongChannel

# Interface: PhongChannel

An ambient/diffuse/emissive Phong channel.

## Properties

### color

> **color**: [`Color`](../type-aliases/Color.md)

Channel color `[r, g, b, a]` (alpha ignored by shading).

***

### map?

> `optional` **map?**: [`EfxSample`](../type-aliases/EfxSample.md) \| `null`

Optional modulating texture (or render target); `null`/omitted means none.
