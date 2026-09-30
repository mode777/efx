[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / CreateCharacterOptions

# Interface: CreateCharacterOptions

Options for `physics.createCharacter`.

## Example

```js
const hero = efx.physics.createCharacter({
  radius: 0.4, height: 1.8, position: [-5, 1, 0],
  floorMaxAngle: 50, stepHeight: 0.35, floorSnapLength: 0.15,
});
```

## Properties

### floorMaxAngle?

> `optional` **floorMaxAngle?**: `number`

Maximum walkable floor angle in degrees (default 45).

***

### floorSnapLength?

> `optional` **floorSnapLength?**: `number`

Floor snap distance (default 0.1).

***

### height

> **height**: `number`

Total tip-to-tip capsule height; must be >= 2 * radius; required.

***

### layer?

> `optional` **layer?**: `number`

Collision layer bitmask (32-bit; default all bits).

***

### mask?

> `optional` **mask?**: `number`

Collision mask bitmask (32-bit; default all bits).

***

### maxSlides?

> `optional` **maxSlides?**: `number`

Maximum slide iterations per move (positive integer, default 6).

***

### position?

> `optional` **position?**: [`Vec3`](../type-aliases/Vec3.md)

Initial position in world units (default `[0, 0, 0]`).

***

### radius

> **radius**: `number`

Capsule radius (must be > 0); required.

***

### safeMargin?

> `optional` **safeMargin?**: `number`

Collision safe margin (default 0.001).

***

### stepHeight?

> `optional` **stepHeight?**: `number`

Step-up height; `0` disables step-up (default 0.3).

***

### up?

> `optional` **up?**: [`Vec3`](../type-aliases/Vec3.md)

Up direction (default `[0, 1, 0]`, must be non-zero).
