[**EFX API**](../README.md)

***

[EFX API](../README.md) / MeshDataShorthand

# Interface: MeshDataShorthand

The single-surface shorthand form of `createMeshData`.

## Extends

- [`MeshSurfaceData`](MeshSurfaceData.md)

## Properties

### colors?

> `optional` **colors?**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Optional flat vertex colors (4 numbers per vertex).

#### Inherited from

[`MeshSurfaceData`](MeshSurfaceData.md).[`colors`](MeshSurfaceData.md#colors)

***

### indices?

> `optional` **indices?**: [`FlatIndices`](../type-aliases/FlatIndices.md)

Triangle-list indices; omitted means non-indexed (vertex count divisible by 3).

#### Inherited from

[`MeshSurfaceData`](MeshSurfaceData.md).[`indices`](MeshSurfaceData.md#indices)

***

### joints?

> `optional` **joints?**: [`FlatIndices`](../type-aliases/FlatIndices.md)

Four joint indices per vertex (glTF `JOINTS_0`); requires `weights`.

#### Inherited from

[`MeshSurfaceData`](MeshSurfaceData.md).[`joints`](MeshSurfaceData.md#joints)

***

### materials?

> `optional` **materials?**: ([`Material`](Material.md) \| `null`)[]

One entry; `null` selects the engine default material.

***

### normals?

> `optional` **normals?**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Optional flat normals (3 numbers per vertex); defaults to `(0,0,1)`.

#### Inherited from

[`MeshSurfaceData`](MeshSurfaceData.md).[`normals`](MeshSurfaceData.md#normals)

***

### positions

> **positions**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Required flat xyz positions (3 numbers per vertex).

#### Inherited from

[`MeshSurfaceData`](MeshSurfaceData.md).[`positions`](MeshSurfaceData.md#positions)

***

### surfaces?

> `optional` **surfaces?**: `undefined`

The batch field is forbidden in the shorthand form (exclusive union).

***

### uvs?

> `optional` **uvs?**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Optional flat texture coordinates (2 numbers per vertex).

#### Inherited from

[`MeshSurfaceData`](MeshSurfaceData.md).[`uvs`](MeshSurfaceData.md#uvs)

***

### weights?

> `optional` **weights?**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Four joint weights per vertex (glTF `WEIGHTS_0`); requires `joints`.

#### Inherited from

[`MeshSurfaceData`](MeshSurfaceData.md).[`weights`](MeshSurfaceData.md#weights)
