[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxGamepadView

# Interface: EfxGamepadView

A pad slot view: plain data plus query methods. Not a resource — there is
nothing to create or destroy.

## Properties

### connected

> `readonly` **connected**: `boolean`

`true` while a pad occupies this slot.

***

### index

> `readonly` **index**: `number`

Slot index this view reports (0-based).

***

### mapped

> `readonly` **mapped**: `boolean`

`true` when a semantic mapping was found (else only `rawButton`/`rawAxis`).

***

### name

> `readonly` **name**: `string`

Device name string reported by the platform.

## Methods

### axis()

> **axis**(`axis`): `number`

Read a normalized axis value.

#### Parameters

##### axis

[`EfxGamepadAxis`](../type-aliases/EfxGamepadAxis.md)

Semantic axis name.

#### Returns

`number`

Stick axes in `-1..1` and trigger axes in `0..1`; throws `TypeError` for an unknown axis.

***

### isDown()

> **isDown**(`button`): `boolean`

Test whether a semantic button is currently held.

#### Parameters

##### button

[`EfxGamepadButton`](../type-aliases/EfxGamepadButton.md)

Semantic button name.

#### Returns

`boolean`

`true` while `button` is held; throws `TypeError` for an unknown button.

***

### isPressed()

> **isPressed**(`button`): `boolean`

Test whether a semantic button transitioned down this frame.

#### Parameters

##### button

[`EfxGamepadButton`](../type-aliases/EfxGamepadButton.md)

Semantic button name.

#### Returns

`boolean`

`true` on the frame `button` transitioned down.

***

### isReleased()

> **isReleased**(`button`): `boolean`

Test whether a semantic button transitioned up this frame.

#### Parameters

##### button

[`EfxGamepadButton`](../type-aliases/EfxGamepadButton.md)

Semantic button name.

#### Returns

`boolean`

`true` on the frame `button` transitioned up.

***

### rawAxis()

> **rawAxis**(`index`): `number`

Read a raw device axis value by index (for unmapped pads).

#### Parameters

##### index

`number`

Raw axis index.

#### Returns

`number`

The raw device value (0 when out of range).

***

### rawButton()

> **rawButton**(`index`): `number`

Read a raw device button value by index (for unmapped pads).

#### Parameters

##### index

`number`

Raw button index.

#### Returns

`number`

The raw device value (0 when out of range).
