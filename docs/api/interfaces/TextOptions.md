[**EFX API**](../README.md)

***

[EFX API](../README.md) / TextOptions

# Interface: TextOptions

Options for `drawText` / `measureText`.

## Example

```js
efx.graphics.drawText(paragraph, body, 40, 168, {
  width: 560,
  align: 'justify',
  color: [0.85, 0.88, 0.95, 1],
});
```

## Properties

### align?

> `optional` **align?**: `"left"` \| `"center"` \| `"right"` \| `"justify"`

Horizontal alignment (default `'left'`); `'justify'` requires `width`.

***

### color?

> `optional` **color?**: [`Color`](../type-aliases/Color.md)

Fill color (default opaque white).

***

### lineHeight?

> `optional` **lineHeight?**: `number`

Line advance in pixels; defaults to the font's `lineHeight`.

***

### outlineColor?

> `optional` **outlineColor?**: [`Color`](../type-aliases/Color.md)

Baked-outline color (default black).

***

### rotation?

> `optional` **rotation?**: `number`

Rotation in degrees about the anchor (default 0).

***

### scale?

> `optional` **scale?**: `number`

Uniform scale (default 1).

***

### shadowColor?

> `optional` **shadowColor?**: [`Color`](../type-aliases/Color.md)

Baked-shadow color (default black).

***

### valign?

> `optional` **valign?**: `"top"` \| `"middle"` \| `"bottom"`

Vertical alignment relative to `y` (default `'top'`).

***

### width?

> `optional` **width?**: `number`

Wrap width in pixels; required for `'justify'`.
