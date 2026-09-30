[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxRequire

# Interface: EfxRequire()

The module-scoped `require` function. It resolves relative (`./`, `../`) or
root-relative specifiers and loads synchronously.

## Example

```js
const palette = require('./lib/palette.js'); // relative module
const orbit = require('./lib/orbit');        // no extension -> .js fallback
const scene = require('./data/scene.json');  // JSON module -> parsed value
```

> **EfxRequire**(`specifier`): `unknown`

Load a module and return its exports.

## Parameters

### specifier

`string`

Relative (`./`, `../`) or root-relative module path.

## Returns

`unknown`

The module's `module.exports` value.

## Properties

### cache

> `readonly` **cache**: `Record`\<`string`, [`EfxModuleCacheEntry`](EfxModuleCacheEntry.md)\>

Modules cached by resolved path.

## Methods

### resolve()

> **resolve**(`specifier`): `string`

Resolve a specifier to its canonical module path.

#### Parameters

##### specifier

`string`

Specifier to resolve.

#### Returns

`string`

The root-relative resolved module path.
