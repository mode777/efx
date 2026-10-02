[**EFX API**](../README.md)

***

[EFX API](../README.md) / Camera2DOptions

# Interface: Camera2DOptions

Options for `setCamera2D`.

## Example

```js
efx.graphics.setCamera2D({ frame: [640, 480] });          // virtual 640x480 frame
efx.graphics.setCamera2D({ frame: [640, 480], zoom: 2 }); // 2x zoom about the center
```

## Properties

### frame?

> `optional` **frame?**: [`Vec2`](../type-aliases/Vec2.md)

Virtual resolution `[width, height]`; defaults to the current window size.

***

### rotation?

> `optional` **rotation?**: `number`

Rotation in degrees around the frame center (default 0).

***

### x?

> `optional` **x?**: `number`

World x shown at the frame center (default: frame center).

***

### y?

> `optional` **y?**: `number`

World y shown at the frame center (default: frame center).

***

### zoom?

> `optional` **zoom?**: `number`

Zoom factor around the frame center (default 1, must be > 0).
