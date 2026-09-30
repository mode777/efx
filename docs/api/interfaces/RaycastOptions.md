[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / RaycastOptions

# Interface: RaycastOptions

Options for `physics.raycast`.

## Example

```js
const hit = efx.physics.raycast(hero.position, [1, 0, 0], { maxDistance: 6 });
if (hit) efx.log('hit at ' + hit.distance.toFixed(2));
```

## Properties

### all?

> `optional` **all?**: `boolean`

Return every hit sorted by distance instead of the first.

***

### mask?

> `optional` **mask?**: `number`

Collision mask bitmask filter.

***

### maxDistance

> **maxDistance**: `number`

Maximum ray distance (positive finite); required.

***

### sensors?

> `optional` **sensors?**: `boolean`

Include sensors (excluded by default).
