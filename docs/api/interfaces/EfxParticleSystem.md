[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxParticleSystem

# Interface: EfxParticleSystem

A native-backed CPU particle system.

## Properties

### count

> `readonly` **count**: `number`

Number of live particles.

***

### speedScale

> **speedScale**: `number`

Read-write simulated-time factor.

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`

***

### emit()

> **emit**(`n`): `void`

Emit a burst of particles immediately.

#### Parameters

##### n

`number`

Number of particles to emit.

#### Returns

`void`

***

### pause()

> **pause**(): `void`

Pause simulation.

#### Returns

`void`

***

### reset()

> **reset**(): `void`

Reset the system to its initial state.

#### Returns

`void`

***

### set()

> **set**(`opts`): `void`

Apply a partial options update atomically.

#### Parameters

##### opts

[`ParticleSystemSetOptions`](../type-aliases/ParticleSystemSetOptions.md)

Any subset of the creation options to change.

#### Returns

`void`

***

### start()

> **start**(): `void`

Start continuous emission.

#### Returns

`void`

***

### stop()

> **stop**(): `void`

Stop emitting (live particles keep simulating).

#### Returns

`void`
