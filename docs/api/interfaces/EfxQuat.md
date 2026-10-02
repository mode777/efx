[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxQuat

# Interface: EfxQuat

Pure-JS quaternion helpers (inputs are never mutated).

## Methods

### fromAxisAngle()

> **fromAxisAngle**(`deg`, `axis`): [`Quat`](../type-aliases/Quat.md)

Build a quaternion from an axis and an angle.

#### Parameters

##### deg

`number`

Rotation angle in degrees.

##### axis

[`Vec3`](../type-aliases/Vec3.md)

Rotation axis.

#### Returns

[`Quat`](../type-aliases/Quat.md)

The corresponding `Quat`.

***

### identity()

> **identity**(): [`Quat`](../type-aliases/Quat.md)

Build the identity quaternion.

#### Returns

[`Quat`](../type-aliases/Quat.md)

A new identity `Quat`.

***

### multiply()

> **multiply**(`a`, `b`): [`Quat`](../type-aliases/Quat.md)

Multiply two quaternions.

#### Parameters

##### a

[`Quat`](../type-aliases/Quat.md)

Left operand.

##### b

[`Quat`](../type-aliases/Quat.md)

Right operand.

#### Returns

[`Quat`](../type-aliases/Quat.md)

The product `a · b`.

***

### toMat4()

> **toMat4**(`q`): [`Mat4`](../type-aliases/Mat4.md)

Convert a quaternion to a rotation matrix.

#### Parameters

##### q

[`Quat`](../type-aliases/Quat.md)

Quaternion to convert.

#### Returns

[`Mat4`](../type-aliases/Mat4.md)

The equivalent rotation `Mat4`.
