[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxMesh

# Interface: EfxMesh

A GPU mesh uploaded from MeshData (opaque native-backed class).

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`

***

### pose()

> **pose**(`pose`): `void`

CPU-pose this skinned mesh in place. Keeps no playback state — the script
owns the clock.

#### Parameters

##### pose

[`PoseSample`](PoseSample.md) \| [`PoseSample`](PoseSample.md)[]

One pose sample, or an array of samples to blend.

#### Returns

`void`

***

### setSurfaceMaterial()

> **setSurfaceMaterial**(`surfaceIndex`, `mat`): `void`

Bind a Phong material to one surface of this mesh.

#### Parameters

##### surfaceIndex

`number`

Surface to bind (`0`-based).

##### mat

[`Material`](Material.md) \| `null`

Material object, or `null` to restore the engine default.

#### Returns

`void`

## Properties

### bounds

> `readonly` **bounds**: [`MeshBounds`](MeshBounds.md)

Axis-aligned bounds of the mesh geometry (bind pose for skinned meshes),
carried from the MeshData at upload: the box `min`/`max` and its derived
`center` and bounding-sphere `radius`. Feeds the engine's frustum
culling; read it to debug culled draws. Fresh plain data — mutating it
does not affect the engine. Throws `TypeError` when destroyed.

***

### surfaceCount

> `readonly` **surfaceCount**: `number`

Number of surfaces (1..16).
