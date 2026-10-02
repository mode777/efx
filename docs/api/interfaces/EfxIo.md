[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxIo

# Interface: EfxIo

Synchronous resource loaders, reached as `efx.io`. Paths are relative to
the resource root (directory or zip) and obey its escape rules; a non-string
path throws `TypeError` and a missing, unreadable, or escaping path throws
a standard `Error`.

## Methods

### loadData()

> **loadData**(`path`): `Uint8Array`

Read a resource as raw bytes.

#### Parameters

##### path

`string`

Resource-root-relative path.

#### Returns

`Uint8Array`

A fresh `Uint8Array` copy of the file's bytes, with no decoding applied.

***

### loadText()

> **loadText**(`path`): `string`

Read a UTF-8 text resource.

#### Parameters

##### path

`string`

Resource-root-relative path.

#### Returns

`string`

The decoded text.
