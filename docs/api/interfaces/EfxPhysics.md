[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxPhysics

# Interface: EfxPhysics

The single physics world. The script owns stepping: call `step(dt)` each
frame and the engine never advances the world on its own.

## Example

```js
efx.physics.gravity = [0, -9.81, 0];
const ground = efx.physics.createBody(
  { type: 'box', size: [40, 1, 40] }, { position: [0, -0.5, 0] });
const crate = efx.physics.createBody(
  { type: 'box', size: [1, 1, 1] }, { dynamic: true, mass: 2, position: [0, 3, 0] });
const hero = efx.physics.createCharacter(0.4, 1.8);

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

Remove every collider and character from the world (existing handles become destroyed).

#### Returns

`void`

***

### createBody()

> **createBody**(`shape`, `opts?`): [`EfxBody`](EfxBody.md)

Create a static, dynamic, or sensor body.

#### Parameters

##### shape

[`PhysicsShape`](../type-aliases/PhysicsShape.md)

Collider shape (required, positional).

##### opts?

[`CreateBodyOptions`](CreateBodyOptions.md)

Optional kind, placement, and material options.

#### Returns

[`EfxBody`](EfxBody.md)

The new body handle.

***

### createCharacter()

> **createCharacter**(`radius`, `height`, `opts?`): [`EfxCharacter`](EfxCharacter.md)

Create a kinematic capsule character controller.

#### Parameters

##### radius

`number`

Capsule radius (must be > 0).

##### height

`number`

Total tip-to-tip capsule height; must be >= 2 * radius.

##### opts?

[`CreateCharacterOptions`](CreateCharacterOptions.md)

Optional placement, movement, and collision options.

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

> **raycast**(`origin`, `direction`, `maxDistance`, `opts?`): [`PhysicsRayHit`](PhysicsRayHit.md) \| `null`

Cast a ray and return the nearest hit.

##### Parameters

###### origin

[`Vec3`](../type-aliases/Vec3.md)

Ray origin in world units.

###### direction

[`Vec3`](../type-aliases/Vec3.md)

Ray direction (normalized by the engine).

###### maxDistance

`number`

Maximum ray distance (positive finite).

###### opts?

[`RaycastOptions`](RaycastOptions.md)

Optional mask, all-hits, and sensor options.

##### Returns

[`PhysicsRayHit`](PhysicsRayHit.md) \| `null`

The first hit, or `null` when nothing is hit.

#### Call Signature

> **raycast**(`origin`, `direction`, `maxDistance`, `opts?`): [`PhysicsRayHit`](PhysicsRayHit.md)[]

Cast a ray and return every hit sorted by distance.

##### Parameters

###### origin

[`Vec3`](../type-aliases/Vec3.md)

Ray origin in world units.

###### direction

[`Vec3`](../type-aliases/Vec3.md)

Ray direction (normalized by the engine).

###### maxDistance

`number`

Maximum ray distance (positive finite).

###### opts?

[`RaycastOptions`](RaycastOptions.md) & `object`

Query options with `all: true`.

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

A `dt` larger than the engine's maximum substep (1/60 s) is internally
simulated as several equal substeps, so a low frame rate cannot make a
body skip thin static geometry; the default 1/60 cadence is unchanged.
An accumulated `applyForce` acts over the whole `dt`.

#### Parameters

##### dt

`number`

Time step in seconds.

#### Returns

`void`
