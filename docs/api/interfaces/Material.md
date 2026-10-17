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

### alphaMode?

> `optional` **alphaMode?**: `"opaque"` \| `"mask"` \| `"blend"` \| `null`

Transparency mode selecting the render path (default `'opaque'`).

- `'opaque'`: the full rendering path — depth-tested, depth-writing,
  drawn in record order. Fully opaque: fragment alpha is forced to `1`
  regardless of vertex, tint, or channel alpha.
- `'mask'`: the alpha-scissor path — same full path with depth writing;
  fragments sampling `alphaMask` alpha below `0.5` are discarded.
- `'blend'`: the transparent pass — drawn after every opaque/mask
  surface of the pass, sorted back-to-front by draw origin, depth-tested
  but never depth-writing. Fragment alpha is the product of the
  `diffuse` channel alpha × vertex-color alpha × tint alpha; a bound
  `alphaMask` does not discard in this mode.

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
ignored. `alphaMask`, `alphaMode`, and `blend` still apply. Use for skies,
UI, and other surfaces that should show their texture exactly as authored.
