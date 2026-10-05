[**EFX API**](../README.md)

***

[EFX API](../README.md) / DrawSpritesOptions

# Interface: DrawSpritesOptions

Options for `drawSprites`.

## Example

```js
efx.graphics.drawSprites(spark, sprites, { blend: 'additive' });
```

## Properties

### blend?

> `optional` **blend?**: [`EfxBlendMode`](../type-aliases/EfxBlendMode.md)

Blend mode applied to every sprite in the batch; overrides the frame's blend state.
