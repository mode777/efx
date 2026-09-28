# Showcase asset credits

Every asset shipped by a curated showcase sample is **CC0 1.0 Universal**
(public domain dedication) and is reproduced here with provenance and the
optimization recipe, so each pack can be regenerated. CC0 requires no
attribution; this file exists for transparency.

| Pack | File | Author | Source | License |
|---|---|---|---|---|
| `texture-showcase.zip` | `paving_color.jpg` | ambientCG (Lennart Demes) | `PavingStones070`, https://ambientcg.com/view?id=PavingStones070 | CC0 1.0 |
| `gltf-showcase.zip` | `Avocado.gltf`, `Avocado.bin`, `Avocado_baseColor.png` | Microsoft | Khronos glTF-Sample-Assets, `Models/Avocado`, https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Avocado | CC0 1.0 |

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
