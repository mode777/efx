# Showcase asset credits

Every asset shipped by a curated showcase sample is **CC0 1.0 Universal**
(public domain dedication) and is reproduced here with provenance and the
optimization recipe, so each sample's resources can be regenerated. CC0
requires no attribution; this file exists for transparency.

Each curated sample is a self-contained directory under
`gallery/samples/curated/<name>/` that is also the player's resource root
(`player <name>`); the gallery build derives that sample's mountable pack
from the directory.

| Sample directory | File | Author | Source | License |
|---|---|---|---|---|
| `texture-showcase/` | `paving_color.jpg` | ambientCG (Lennart Demes) | `PavingStones070`, https://ambientcg.com/view?id=PavingStones070 | CC0 1.0 |
| `gltf-showcase/` | `Avocado.gltf`, `Avocado.bin`, `Avocado_baseColor.png` | Microsoft | Khronos glTF-Sample-Assets, `Models/Avocado`, https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Avocado | CC0 1.0 |
| `fox-walk/` | `Fox.glb` | Quaternius (Tomás Laulhé) | Quaternius, "Ultimate Animated Animals" (https://quaternius.com/packs/ultimateanimatedanimals.html); glb mirror: https://github.com/trebeljahr/quaternius-showcase | CC0 1.0 |
| `text-showcase/` | `font.ttf` | Kenney (www.kenney.nl) | Kenney Fonts, "Kenney Future"; mirror: https://github.com/ereborstudios/kenney-fonts | CC0 1.0 |
| `gamepad-tester/` | `font.ttf` | Kenney (www.kenney.nl) | Same font as `text-showcase/` | CC0 1.0 |
| `audio-showcase/` | `font.ttf` | Kenney (www.kenney.nl) | Same font as `text-showcase/` | CC0 1.0 |
| `game-neon-pong/` | `font.ttf` | Kenney (www.kenney.nl) | Same font as `text-showcase/` | CC0 1.0 |
| `game-bloom-breakout/` | `font.ttf` | Kenney (www.kenney.nl) | Same font as `text-showcase/` | CC0 1.0 |
| `game-bloom-breakout/` | `paddle.wav`, `brick.wav`, `wall.wav`, `life.wav` | Authored in-repo | Synthesized by `gallery/scripts/gen-audio-assets.py` | CC0 1.0 |
| `game-glow-gauntlet/` | `font.ttf` | Kenney (www.kenney.nl) | Same font as `text-showcase/` | CC0 1.0 |
| `game-glow-gauntlet/` | `music.wav`, `sting.wav` | Authored in-repo | Synthesized by `gallery/scripts/gen-audio-assets.py` | CC0 1.0 |
| `game-mini-golf/` | `font.ttf` | Kenney (www.kenney.nl) | Same font as `text-showcase/` | CC0 1.0 |
| `skybox-showcase/` | `sky.jpg` | Poly Haven (Jarod Guest) | Poly Haven, `kloofendal_43d_clear_puresky`, https://polyhaven.com/a/kloofendal_43d_clear_puresky | CC0 1.0 |

License text: https://creativecommons.org/publicdomain/zero/1.0/legalcode

## Recipes

Both the texture and glTF assets were built with Python 3 + Pillow; the
files sit at the sample directory root, so the sample scripts address them
root-relative. The derived zips are deterministic (fixed entry timestamps,
stored entries, no directory records).

### `texture-showcase/`

1. Download `PavingStones070_1K-JPG.zip` from
   `https://ambientcg.com/get?file=PavingStones070_1K-JPG.zip`.
2. Extract `PavingStones070_1K-JPG_Color.jpg` (1024x1024).
3. Downscale to 512x512 (LANCZOS) and re-encode JPEG quality 88, optimized
   -> `gallery/samples/curated/texture-showcase/paving_color.jpg`.

### `gltf-showcase/`

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
   (optimized); commit all three files under
   `gallery/samples/curated/gltf-showcase/`.

### `fox-walk/`

1. Download `Fox.glb` from Quaternius' "Ultimate Animated Animals" pack
   (CC0 1.0); the committed copy came from the
   `trebeljahr/quaternius-showcase` mirror at
   `public/glb/animals_pack/Fox.glb`.
2. Edit the glb's material factors so every material is a matte dielectric
   under the engine's fixed-function Phong model: set `metallicFactor` to
   `0.0` and `roughnessFactor` to `0.6` in each
   `pbrMetallicRoughness`. (The importer binds only base-color and emissive;
   this pack has no textures.)
3. Commit `Fox.glb` under `gallery/samples/curated/fox-walk/`.

### `modules-showcase/`

This sample is **authored in-repo** (no third-party assets), so it is not
CC0 material and has no credit line. The readable CommonJS sources live
directly under `gallery/samples/curated/modules-showcase/`
(`lib/palette.js`, `lib/orbit.js`, `data/scene.json`) beside `main.js`; the
sample directory is packed as-is (no separate packer).

### `text-showcase/`

1. Download `Kenney Future.ttf` from the Kenney Fonts CC0 pack; the committed
   copy came from the `ereborstudios/kenney-fonts` mirror at the archive root
   (`https://raw.githubusercontent.com/ereborstudios/kenney-fonts/main/Kenney%20Future.ttf`).
2. Commit it under `gallery/samples/curated/text-showcase/` as `font.ttf`.

### `gamepad-tester/`

Reuses the exact `font.ttf` from `text-showcase/` (same CC0 provenance),
committed under `gallery/samples/curated/gamepad-tester/`. No new asset is
introduced.

### `audio-showcase/`

Mostly **authored in-repo** (no third-party audio): the three effect WAVs
are synthesized deterministically by the generator, and `music.mp3` is a
committed ~6 s looping arpeggio encoded once with a pinned ffmpeg
(`libmp3lame`, 64 kbps, 22.05 kHz mono) — MP3 is not byte-reproducible across
encoders, so it is a source, not generated. The only third-party asset is the
CC0 `font.ttf` (same Kenney font as `text-showcase/`). Regenerate the loose
WAVs in the sample directory deterministically with:

```sh
python3 gallery/scripts/gen-audio-assets.py
python3 gallery/scripts/gen-audio-assets.py --check   # drift check
```

The committed sources live under `gallery/samples/curated/audio-showcase/`
(`music.mp3`, `font.ttf`).

### `skybox-showcase/`

1. Download the tonemapped JPG rendition of Poly Haven's
   `kloofendal_43d_clear_puresky` (CC0 1.0, 8192x4096):
   `https://dl.polyhaven.org/file/ph-assets/HDRIs/extra/Tonemapped%20JPG/kloofendal_43d_clear_puresky.jpg`.
2. Downscale to 2048x1024 (LANCZOS) and re-encode JPEG quality 85, optimized
   -> `gallery/samples/curated/skybox-showcase/sky.jpg`. The tonemapped LDR
   JPG is used because the engine has no HDR/EXR decoder; the image is
   equirectangular, matching the inverted sphere's UVs.

### `game-neon-pong/`

Reuses the exact `font.ttf` from `text-showcase/` (same CC0 provenance),
committed under `gallery/samples/curated/game-neon-pong/`. No other asset is
introduced; all game graphics are procedurally generated quads and textures.

### `game-bloom-breakout/`

Reuses the exact `font.ttf` from `text-showcase/` (same CC0 provenance).
The four effect WAVs (`paddle.wav`, `brick.wav`, `wall.wav`, `life.wav`) are
synthesized deterministically by `gallery/scripts/gen-audio-assets.py` (its
`game-bloom-breakout` bank), committed loose in the sample directory.
Regenerate with `python3 gallery/scripts/gen-audio-assets.py`; verify with
`--check`. All game graphics are procedural quads and textures.

### `game-glow-gauntlet/`

Reuses the exact `font.ttf` from `text-showcase/` (same CC0 provenance).
`music.wav` (a ~8 s seamless loop) and `sting.wav` are synthesized
deterministically by `gallery/scripts/gen-audio-assets.py` (its
`game-glow-gauntlet` bank), committed loose in the sample directory.
Regenerate with `python3 gallery/scripts/gen-audio-assets.py`; verify with
`--check`. All game graphics are procedural quads and textures.

### `game-mini-golf/`

Reuses the exact `font.ttf` from `text-showcase/` (same CC0 provenance).
All geometry (course, ramps, ball) and the grass and shadow textures are
generated procedurally in the sample; no other asset is introduced.
