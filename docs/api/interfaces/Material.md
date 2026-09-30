[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / Material

# Interface: Material

A per-surface Phong material. It is JS-managed (no native handle, no
`destroy()`) and the engine snapshots it at binding time, so later mutation
of the script object does not change the bound material.

## Example

```js
efx.setMeshSurfaceMaterial(cube, 0, {
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
