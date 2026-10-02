[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxVec3

# Interface: EfxVec3

Pure-JS 3-component vector helpers (inputs are never mutated).

## Methods

### add()

> **add**(`a`, `b`): [`Vec3`](../type-aliases/Vec3.md)

Add two vectors.

#### Parameters

##### a

[`Vec3`](../type-aliases/Vec3.md)

Left operand.

##### b

[`Vec3`](../type-aliases/Vec3.md)

Right operand.

#### Returns

[`Vec3`](../type-aliases/Vec3.md)

`a + b`.

***

### cross()

> **cross**(`a`, `b`): [`Vec3`](../type-aliases/Vec3.md)

Compute the cross product.

#### Parameters

##### a

[`Vec3`](../type-aliases/Vec3.md)

Left operand.

##### b

[`Vec3`](../type-aliases/Vec3.md)

Right operand.

#### Returns

[`Vec3`](../type-aliases/Vec3.md)

`a × b`.

***

### dot()

> **dot**(`a`, `b`): `number`

Compute the dot product.

#### Parameters

##### a

[`Vec3`](../type-aliases/Vec3.md)

Left operand.

##### b

[`Vec3`](../type-aliases/Vec3.md)

Right operand.

#### Returns

`number`

`a · b`.

***

### normalize()

> **normalize**(`v`): [`Vec3`](../type-aliases/Vec3.md)

Normalize a vector.

#### Parameters

##### v

[`Vec3`](../type-aliases/Vec3.md)

Vector to normalize.

#### Returns

[`Vec3`](../type-aliases/Vec3.md)

The unit vector along `v`.

***

### scale()

> **scale**(`v`, `s`): [`Vec3`](../type-aliases/Vec3.md)

Scale a vector.

#### Parameters

##### v

[`Vec3`](../type-aliases/Vec3.md)

Vector to scale.

##### s

`number`

Scalar factor.

#### Returns

[`Vec3`](../type-aliases/Vec3.md)

`v * s`.

***

### sub()

> **sub**(`a`, `b`): [`Vec3`](../type-aliases/Vec3.md)

Subtract two vectors.

#### Parameters

##### a

[`Vec3`](../type-aliases/Vec3.md)

Left operand.

##### b

[`Vec3`](../type-aliases/Vec3.md)

Right operand.

#### Returns

[`Vec3`](../type-aliases/Vec3.md)

`a - b`.
