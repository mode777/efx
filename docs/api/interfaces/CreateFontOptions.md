[**EFX API**](../README.md)

***

[EFX API](../README.md) / CreateFontOptions

# Interface: CreateFontOptions

Options for `createFont`.

## Example

```js
const title = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 44, {
  outline: { width: 2 },
  shadow: { blur: 3, offset: [2, 2] },
});
const body = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 24);
```

## Properties

### filter?

> `optional` **filter?**: `"nearest"` \| `"linear"`

Atlas sampler filter (default `'linear'`).

***

### glyphs?

> `optional` **glyphs?**: `string`

Codepoints to bake; defaults to the printable Latin-1 set.

***

### outline?

> `optional` **outline?**: [`FontOutline`](FontOutline.md) \| `null`

Baked outline ring; `null`/omitted bakes none.

***

### padding?

> `optional` **padding?**: `number`

Atlas gutter in pixels (default 1).

***

### shadow?

> `optional` **shadow?**: [`FontShadow`](FontShadow.md) \| `null`

Baked blurred shadow; `null`/omitted bakes none.
