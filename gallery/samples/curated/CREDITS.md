# Showcase asset credits

Every asset shipped by a curated showcase sample is **CC0 1.0 Universal**
(public domain dedication) and is reproduced here with provenance and the
optimization recipe, so each pack can be regenerated. CC0 requires no
attribution; this file exists for transparency.

| Pack | File | Author | Source | License |
|---|---|---|---|---|
| `texture-showcase.zip` | `paving_color.jpg` | ambientCG (Lennart Demes) | `PavingStones070`, https://ambientcg.com/view?id=PavingStones070 | CC0 1.0 |
| `gltf-showcase.zip` | `Avocado.gltf`, `Avocado.bin`, `Avocado_baseColor.png` | Microsoft | Khronos glTF-Sample-Assets, `Models/Avocado`, https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Avocado | CC0 1.0 |
| `fox-walk.zip` | `Fox.glb` | Quaternius (Tomás Laulhé) | Quaternius, "Ultimate Animated Animals" (https://quaternius.com/packs/ultimateanimatedanimals.html); glb mirror: https://github.com/trebeljahr/quaternius-showcase | CC0 1.0 |
| `text-showcase.zip` | `font.ttf` | Kenney (www.kenney.nl) | Kenney Fonts, "Kenney Future"; mirror: https://github.com/ereborstudios/kenney-fonts | CC0 1.0 |
| `gamepad-tester.zip` | `font.ttf` | Kenney (www.kenney.nl) | Same font as `text-showcase.zip` | CC0 1.0 |
| `audio-showcase.zip` | `font.ttf` | Kenney (www.kenney.nl) | Same font as `text-showcase.zip` | CC0 1.0 |

License text: https://creativecommons.org/publicdomain/zero/1.0/legalcode

## Recipes

Both packs were built with Python 3 + Pillow. The zips are deterministic
(fixed entry timestamps, no directory entries) and contain the files at
their root, so the sample scripts address them root-relative.

### `texture-showcase.zip`

1. Download `PavingStones070_1K-JPG.zip` from
   `https://ambientcg.com/get?file=PavingStones070_1K-JPG.zip`.
2. Extract `PavingStones070_1K-JPG_Color.jpg` (1024x1024).
3. Downscale to 512x512 (LANCZOS) and re-encode JPEG quality 88, optimized
   -> `paving_color.jpg`.
4. Zip `paving_color.jpg` at the archive root.

### `gltf-showcase.zip`

1. Download `Avocado.gltf`, `Avocado.bin`, and `Avocado_baseColor.png`
   from
   `https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/Avocado/glTF/`.
2. Edit `Avocado.gltf`: keep only the base-color image/texture and drop the
   `normalTexture` and `metallicRoughnessTexture` references. The importer
   (`src/resource/gltf.c`) binds only the base-color and emissive textures;
   normal, occlusion, and metallic-roughness maps are never decoded, so the
   asset does not ship them. `Avocado.bin` (geometry) is unchanged.
3. Set `pbrMetallicRoughness.metallicFactor` to `0.0` and
   `roughnessFactor` to `0.5`. The engine has no PBR material model
   (ADR 0032); these factors give a matte dielectric Phong material
   (specular color from metallic, shininess from roughness) that reads well
   under the fixed-function lighting.
4. Downscale `Avocado_baseColor.png` from 2048x2048 to 512x512 PNG
   (optimized) -> `Avocado_baseColor.png`.
5. Zip `Avocado.gltf`, `Avocado.bin`, and `Avocado_baseColor.png` at the
   archive root.

### `fox-walk.zip`

1. Download `Fox.glb` from Quaternius' "Ultimate Animated Animals" pack
   (CC0 1.0); the committed copy came from the
   `trebeljahr/quaternius-showcase` mirror at
   `public/glb/animals_pack/Fox.glb`.
2. Edit the glb's material factors so every material is a matte dielectric
   under the engine's fixed-function Phong model: set `metallicFactor` to
   `0.0` and `roughnessFactor` to `0.6` in each
   `pbrMetallicRoughness`. (The importer binds only base-color and emissive;
   this pack has no textures.)
3. Zip `Fox.glb` at the archive root.
### `modules-showcase.zip`

This pack is **authored in-repo** (no third-party assets), so it is not CC0
material and has no credit line. The readable CommonJS sources live under
`gallery/samples/curated/modules/`; the pack is regenerated deterministically
(fixed entry timestamps, stored entries) with:

```sh
python3 gallery/scripts/pack-curated-modules.py
python3 gallery/scripts/pack-curated-modules.py --check   # drift check
```

### `text-showcase.zip`

1. Download `Kenney Future.ttf` from the Kenney Fonts CC0 pack; the committed
   copy came from the `ereborstudios/kenney-fonts` mirror at the archive root
   (`https://raw.githubusercontent.com/ereborstudios/kenney-fonts/main/Kenney%20Future.ttf`).
2. Zip it at the archive root as `font.ttf`, deterministically (fixed entry
   timestamp, no directory entries), e.g. with Python's `zipfile` using a
   fixed `ZipInfo.date_time`.

### `gamepad-tester.zip`

Reuses the exact `font.ttf` from `text-showcase.zip` (same CC0 provenance),
re-zipped at the archive root with a fixed entry timestamp. It exists only so
the sample mounts its own resource root; no new asset is introduced.

### `audio-showcase.zip`

Mostly **authored in-repo** (no third-party audio): the three effect WAVs are
synthesized deterministically by the pack script, and `music.mp3` is a
committed ~6 s looping arpeggio encoded once with a pinned ffmpeg
(`libmp3lame`, 64 kbps, 22.05 kHz mono) — MP3 is not byte-reproducible across
encoders, so it is a source, not generated. The only third-party asset is the
CC0 `font.ttf` (same Kenney font as `text-showcase.zip`). Regenerate the pack
deterministically with:

```sh
python3 gallery/scripts/pack-curated-audio.py
python3 gallery/scripts/pack-curated-audio.py --check   # drift check
```

The committed pack sources live under `gallery/samples/curated/audio/`
(`music.mp3`, `font.ttf`).
