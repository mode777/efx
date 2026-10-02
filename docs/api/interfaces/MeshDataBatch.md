[**EFX API**](../README.md)

***

[EFX API](../README.md) / MeshDataBatch

# Interface: MeshDataBatch

The multi-surface batch form of `createMeshData`.

## Properties

### colors?

> `optional` **colors?**: `undefined`

***

### indices?

> `optional` **indices?**: `undefined`

***

### joints?

> `optional` **joints?**: `undefined`

***

### materials?

> `optional` **materials?**: ([`Material`](Material.md) \| `null`)[]

One entry per surface; `null` selects the engine default material.

***

### normals?

> `optional` **normals?**: `undefined`

***

### positions?

> `optional` **positions?**: `undefined`

Shorthand fields are forbidden in the batch form (exclusive union).

***

### surfaces

> **surfaces**: [`MeshSurfaceData`](MeshSurfaceData.md)[]

1..16 surfaces, each a Godot surface / glTF primitive.

***

### uvs?

> `optional` **uvs?**: `undefined`

***

### weights?

> `optional` **weights?**: `undefined`
