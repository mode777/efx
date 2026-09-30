[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / PoseSample

# Interface: PoseSample

One pose sample: a clip sampled at `time` (seconds) with an optional blend
`weight` (normalized engine-side across an array; a single sample ignores
it). `clip` is a clip name or an index.

## Example

```js
// cross-fade walk -> run over two seconds
efx.poseMesh(hero, [
  { clip: 'Walk', time: t, weight: 1 - k },
  { clip: 'Run',  time: t, weight: k },
]);
```

## Properties

### clip

> **clip**: `string` \| `number`

Clip name (glTF `name`, or stable `clipN`) or a non-negative clip index.

***

### time

> **time**: `number`

Sample time in seconds; wraps modulo the clip length.

***

### weight?

> `optional` **weight?**: `number`

Blend weight (>= 0); normalized across an array, ignored for a single sample.
