[**EFX API**](../README.md)

***

[EFX API](../README.md) / MeshSurfaceData

# Interface: MeshSurfaceData

One mesh surface's attribute arrays (a Godot surface / glTF primitive).

## Properties

### colors?

> `optional` **colors?**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Optional flat vertex colors (4 numbers per vertex).

***

### indices?

> `optional` **indices?**: [`FlatIndices`](../type-aliases/FlatIndices.md)

Triangle-list indices; omitted means non-indexed (vertex count divisible by 3).

***

### joints?

> `optional` **joints?**: [`FlatIndices`](../type-aliases/FlatIndices.md)

Four joint indices per vertex (glTF `JOINTS_0`); requires `weights`.

***

### normals?

> `optional` **normals?**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Optional flat normals (3 numbers per vertex); defaults to `(0,0,1)`.

***

### positions

> **positions**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Required flat xyz positions (3 numbers per vertex).

***

### uvs?

> `optional` **uvs?**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Optional flat texture coordinates (2 numbers per vertex).

***

### weights?

> `optional` **weights?**: [`FlatNumbers`](../type-aliases/FlatNumbers.md)

Four joint weights per vertex (glTF `WEIGHTS_0`); requires `joints`.
