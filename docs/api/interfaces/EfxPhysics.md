[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxPhysics

# Interface: EfxPhysics

The single physics world. The script owns stepping: call `step(dt)` each
frame and the engine never advances the world on its own.

## Example

```js
efx.physics.gravity = [0, -9.81, 0];
const ground = efx.physics.createBody({
  shape: { type: 'box', size: [40, 1, 40] }, position: [0, -0.5, 0] });
const crate = efx.physics.createBody({
  dynamic: true, mass: 2,
  shape: { type: 'box', size: [1, 1, 1] }, position: [0, 3, 0] });
const hero = efx.physics.createCharacter({ radius: 0.4, height: 1.8 });

efx.registerUpdateHook((dt) => {
  efx.physics.step(dt);
  hero.moveAndSlide([1.5 * dt, -9.81 * dt, 0]);
});
```

## Properties

### gravity

> **gravity**: [`Vec3`](../type-aliases/Vec3.md)

World gravity `[x, y, z]` (read-write; default `[0, -9.81, 0]`).

***

### iterations

> **iterations**: `number`

Solver iteration count (read-write positive integer; default 8).

## Methods

### clear()

> **clear**(): `void`

Remove every collider from the world.

#### Returns

`void`

***

### createBody()

> **createBody**(`opts`): [`EfxBody`](EfxBody.md)

Create a static, dynamic, or sensor body.

#### Parameters

##### opts

[`CreateBodyOptions`](CreateBodyOptions.md)

Body options; `shape` is required.

#### Returns

[`EfxBody`](EfxBody.md)

The new body handle.

***

### createCharacter()

> **createCharacter**(`opts`): [`EfxCharacter`](EfxCharacter.md)

Create a kinematic capsule character controller.

#### Parameters

##### opts

[`CreateCharacterOptions`](CreateCharacterOptions.md)

Character options; `radius` and `height` are required.

#### Returns

[`EfxCharacter`](EfxCharacter.md)

The new character handle.

***

### createStaticMesh()

> **createStaticMesh**(`mesh`, `opts?`): [`EfxBody`](EfxBody.md)

Create a static triangle-mesh collider from a live Mesh.

#### Parameters

##### mesh

[`EfxMesh`](EfxMesh.md)

Source mesh (arbitrary surface count).

##### opts?

[`CreateStaticMeshOptions`](CreateStaticMeshOptions.md)

Optional placement and material options.

#### Returns

[`EfxBody`](EfxBody.md)

The new static body handle.

***

### overlap()

> **overlap**(`shape`, `opts?`): ([`EfxBody`](EfxBody.md) \| [`EfxCharacter`](EfxCharacter.md))[]

Find bodies and characters intersecting a shape (including sensors).

#### Parameters

##### shape

[`PhysicsShape`](../type-aliases/PhysicsShape.md)

Query shape.

##### opts?

[`OverlapOptions`](OverlapOptions.md)

Optional query position and mask.

#### Returns

([`EfxBody`](EfxBody.md) \| [`EfxCharacter`](EfxCharacter.md))[]

The live handles that intersect `shape`.

***

### raycast()

#### Call Signature

> **raycast**(`origin`, `direction`, `opts`): [`PhysicsRayHit`](PhysicsRayHit.md) \| `null`

Cast a ray and return the nearest hit.

##### Parameters

###### origin

[`Vec3`](../type-aliases/Vec3.md)

Ray origin in world units.

###### direction

[`Vec3`](../type-aliases/Vec3.md)

Ray direction (normalized by the engine).

###### opts

[`RaycastOptions`](RaycastOptions.md)

Query options; `maxDistance` is required.

##### Returns

[`PhysicsRayHit`](PhysicsRayHit.md) \| `null`

The first hit, or `null` when nothing is hit.

#### Call Signature

> **raycast**(`origin`, `direction`, `opts`): [`PhysicsRayHit`](PhysicsRayHit.md)[]

Cast a ray and return every hit sorted by distance.

##### Parameters

###### origin

[`Vec3`](../type-aliases/Vec3.md)

Ray origin in world units.

###### direction

[`Vec3`](../type-aliases/Vec3.md)

Ray direction (normalized by the engine).

###### opts

[`RaycastOptions`](RaycastOptions.md) & `object`

Query options with `all: true`; `maxDistance` is required.

##### Returns

[`PhysicsRayHit`](PhysicsRayHit.md)[]

Every hit sorted by distance.

***

### shapeCast()

> **shapeCast**(`shape`, `from`, `motion`, `opts?`): [`PhysicsShapeHit`](PhysicsShapeHit.md) \| `null`

Sweep a shape and return the first hit.

#### Parameters

##### shape

[`PhysicsShape`](../type-aliases/PhysicsShape.md)

Shape to sweep.

##### from

[`Vec3`](../type-aliases/Vec3.md)

Sweep start in world units.

##### motion

[`Vec3`](../type-aliases/Vec3.md)

Sweep displacement in world units.

##### opts?

[`ShapeCastOptions`](ShapeCastOptions.md)

Optional mask and sensor inclusion.

#### Returns

[`PhysicsShapeHit`](PhysicsShapeHit.md) \| `null`

The first hit, or `null` when nothing is hit.

***

### step()

> **step**(`dt`): `void`

Advance the world.

#### Parameters

##### dt

`number`

Time step in seconds.

#### Returns

`void`
