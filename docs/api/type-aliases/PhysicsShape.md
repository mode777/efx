[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / PhysicsShape

# Type Alias: PhysicsShape

> **PhysicsShape** = [`SphereShape`](../interfaces/SphereShape.md) \| [`BoxShape`](../interfaces/BoxShape.md) \| [`CapsuleShape`](../interfaces/CapsuleShape.md) \| [`MeshShape`](../interfaces/MeshShape.md)

A collision shape accepted by bodies and by the spatial queries.

## Example

```js
const box  = { type: 'box', size: [1, 1, 1] };
const ball = { type: 'sphere', radius: 0.5 };
const hero = { type: 'capsule', radius: 0.4, height: 1.8 };
const ramp = { type: 'mesh', mesh: rampMesh };
```
