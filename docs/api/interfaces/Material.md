[**EFX API**](../README.md)

***

[EFX API](../README.md) / Material

# Interface: Material

A per-surface Phong material. It is JS-managed (no native handle, no
`destroy()`) and the engine snapshots it at binding time, so later mutation
of the script object does not change the bound material.

## Example

```js
cube.setSurfaceMaterial(0, {
  ambient:  { color: [0.12, 0.12, 0.16, 1] },
  diffuse:  { color: [1, 1, 1, 1] },
  specular: { color: [1, 1, 1, 1], shininess: 32 },
  emissive: { color: [0, 0, 0, 1] },
});
```

## Properties

### alphaMask?

> `optional` **alphaMask?**: [`EfxSample`](../type-aliases/EfxSample.md) \| `null`

Material-level binary cutout; fragments sampling alpha < 0.5 are discarded.

***

### ambient?

> `optional` **ambient?**: [`PhongChannel`](PhongChannel.md)

Ambient channel (default black).

***

### blend?

> `optional` **blend?**: [`EfxBlendMode`](../type-aliases/EfxBlendMode.md) \| `null`

Blend mode for surfaces bound to this material; `null`/absent uses the frame's blend state.

***

### diffuse?

> `optional` **diffuse?**: [`PhongChannel`](PhongChannel.md)

Diffuse channel (default white).

***

### emissive?

> `optional` **emissive?**: [`PhongChannel`](PhongChannel.md)

Emissive channel (default black).

***

### specular?

> `optional` **specular?**: [`SpecularChannel`](SpecularChannel.md)

Specular channel (default black, shininess 32).

***

### unlit?

> `optional` **unlit?**: `boolean`

Bypass the lighting equation (default `false`). When `true`, the shaded
color is the `diffuse` channel color × its `diffuse` map × the albedo, with
no light contribution; `ambient`, `specular`, `emissive`, and all lights are
ignored. `alphaMask` and `blend` still apply. Use for skies, UI, and other
surfaces that should show their texture exactly as authored.
