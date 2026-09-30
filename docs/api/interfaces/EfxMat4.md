[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxMat4

# Interface: EfxMat4

Pure-JS 4×4 matrix helpers (inputs are never mutated).

## Methods

### identity()

> **identity**(): [`Mat4`](../type-aliases/Mat4.md)

Build the identity matrix.

#### Returns

[`Mat4`](../type-aliases/Mat4.md)

A new identity `Mat4`.

***

### multiply()

> **multiply**(`a`, `b`): [`Mat4`](../type-aliases/Mat4.md)

Multiply two matrices.

#### Parameters

##### a

[`Mat4`](../type-aliases/Mat4.md)

Left-hand matrix.

##### b

[`Mat4`](../type-aliases/Mat4.md)

Right-hand matrix.

#### Returns

[`Mat4`](../type-aliases/Mat4.md)

The product `a · b` (b applies to a vector first).

***

### ortho()

> **ortho**(`w`, `h`, `near`, `far`): [`Mat4`](../type-aliases/Mat4.md)

Build an orthographic projection.

#### Parameters

##### w

`number`

View width in world units.

##### h

`number`

View height in world units.

##### near

`number`

Near plane distance.

##### far

`number`

Far plane distance.

#### Returns

[`Mat4`](../type-aliases/Mat4.md)

A new orthographic `Mat4`.

***

### perspective()

> **perspective**(`fovY`, `aspect`, `near`, `far`): [`Mat4`](../type-aliases/Mat4.md)

Build a perspective projection (column-major, GL convention).

#### Parameters

##### fovY

`number`

Vertical field of view in degrees.

##### aspect

`number`

Viewport aspect ratio (width / height).

##### near

`number`

Near plane distance.

##### far

`number`

Far plane distance.

#### Returns

[`Mat4`](../type-aliases/Mat4.md)

A new projection `Mat4`.

***

### rotate()

> **rotate**(`m`, `deg`, `axis`): [`Mat4`](../type-aliases/Mat4.md)

Rotate a matrix about an axis.

#### Parameters

##### m

[`Mat4`](../type-aliases/Mat4.md)

Source matrix.

##### deg

`number`

Rotation angle in degrees.

##### axis

[`Vec3`](../type-aliases/Vec3.md)

Rotation axis.

#### Returns

[`Mat4`](../type-aliases/Mat4.md)

A new rotated `Mat4`.

***

### scale()

> **scale**(`m`, `v`): [`Mat4`](../type-aliases/Mat4.md)

Scale a matrix.

#### Parameters

##### m

[`Mat4`](../type-aliases/Mat4.md)

Source matrix.

##### v

[`Vec3`](../type-aliases/Vec3.md)

Scale factors `[x, y, z]`.

#### Returns

[`Mat4`](../type-aliases/Mat4.md)

A new scaled `Mat4`.

***

### translate()

> **translate**(`m`, `v`): [`Mat4`](../type-aliases/Mat4.md)

Translate a matrix.

#### Parameters

##### m

[`Mat4`](../type-aliases/Mat4.md)

Source matrix.

##### v

[`Vec3`](../type-aliases/Vec3.md)

Translation `[x, y, z]`.

#### Returns

[`Mat4`](../type-aliases/Mat4.md)

A new translated `Mat4`.
