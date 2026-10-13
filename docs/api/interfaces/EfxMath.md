[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxMath

# Interface: EfxMath

Pure-JS math helpers, reached as `efx.math`. Each helper is a pure
function that never mutates its arguments and returns plain JS data
(column-major `Mat4` arrays, `Vec3` arrays, `Quat` arrays).

## Methods

### clamp()

> **clamp**(`v`, `lo`, `hi`): `number`

Clamp `v` into `[lo, hi]`.

#### Parameters

##### v

`number`

Value to clamp.

##### lo

`number`

Lower bound.

##### hi

`number`

Upper bound.

#### Returns

`number`

The clamped value.

***

### lerp()

> **lerp**(`a`, `b`, `t`): `number`

Linearly interpolate from `a` to `b` by `t`.

#### Parameters

##### a

`number`

Value at `t = 0`.

##### b

`number`

Value at `t = 1`.

##### t

`number`

Interpolation factor.

#### Returns

`number`

The interpolated value.

## Properties

### mat4

> **mat4**: [`EfxMat4`](EfxMat4.md)

Pure-JS 4×4 matrix helpers.

***

### quat

> **quat**: [`EfxQuat`](EfxQuat.md)

Pure-JS quaternion helpers.

***

### vec3

> **vec3**: [`EfxVec3`](EfxVec3.md)

Pure-JS 3-component vector helpers.
