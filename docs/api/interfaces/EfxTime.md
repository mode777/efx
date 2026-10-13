[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxTime

# Interface: EfxTime

The engine-owned script clock.

## Properties

### now

> `readonly` **now**: `number`

Seconds elapsed since the script started, snapshotted at frame start so
every read within one frame is identical. A stalled frame advances the
clock by at most the engine's maximum step, so time-driven animation
cannot teleport. Read-only.
