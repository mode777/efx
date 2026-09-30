[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / RenderScaleOptions

# Interface: RenderScaleOptions

Options for `setRenderScale`.

## Example

```js
efx.setRenderScale(0.5, { filter: 'nearest' }); // crisp half-res pixels
efx.setRenderScale(1);                          // back to native
```

## Properties

### filter?

> `optional` **filter?**: `"nearest"` \| `"linear"`

Final blit filter (default `'linear'`).
