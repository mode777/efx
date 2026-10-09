# Tasks

## 1. Assets

- [x] 1.1 Copy the CC0 `Fox.glb` (with the matte-dielectric material edit from the `fox-walk` recipe) and the CC0 equirectangular sky (the `skybox-showcase` downscale recipe) into `gallery/samples/curated/game-sky-steps/`; add CREDITS rows for fox, sky, and the standard Kenney `font.ttf` (also copied in)
- [x] 1.2 Add the `manifest.json` entry (`Games` category, description noting the character-controller platforming, collectibles, and the skinned fox) and regenerate the catalog; verify the sample appears in the generated catalog JSON

## 2. Character and camera

- [x] 2.1 Author `main.js` skeleton: `createCharacter` capsule with the showcase's proven options, script gravity, camera-relative movement, coyote-time + jump-buffer jump, named tuning constants; verify run/jump feel on a flat test platform under the player
- [x] 2.2 Implement the third-person follow camera with LMB-drag orbit (yaw free, pitch clamped) and ease-back behind the velocity heading; verify orbiting never inverts through the ground and release re-centers smoothly
- [x] 2.3 Load and draw the fox (`loadMeshData`, `skinned: true`) scaled to the capsule, with `mesh.pose` selecting Survey/Walk/Run by ground speed and holding Run mid-stride when airborne; verify clip choice reads correctly for idle, run, jump, and land
- [x] 2.4 Implement keyboard controls and the equivalent gamepad mapping (stick move, button jump); verify both schemes play equivalently

## 3. Course and game loop

- [x] 3.1 Author the course: static box platforms and ramp meshes over a void, star collectibles and checkpoints as sensor bodies with `overlap` detection, checkpoint respawn on the kill plane, finish sensor gated on all stars (locked/unlocked beacon color); verify a full route — collect, die, respawn at checkpoint, finish — plays correctly
- [x] 3.2 Implement the HUD (timer, stars collected count), WIN screen with time result and any-input restart; verify the finish only triggers with every star and the restart resets cleanly
- [x] 3.3 Compose the look: directional key light + ≤2 point accents, procedural radial blob shadow under the fox, inverted-sphere unlit `depthWrite: false` sky following the camera; verify the shadow stays flat through jumps and the sky never occludes the course

## 4. Attract mode

- [x] 4.1 Implement ATTRACT: scripted movement toward the nearest uncollected star with the jump heuristic and instant first-input handover; verify launch self-plays (respawning gracefully on misses) and the first input takes over

## 5. Verification

- [ ] 5.1 Run the built gallery sample through attract → a played round → win/restart with no console or page errors; verify the gallery smoke run includes the sample and passes
- [ ] 5.2 Run `npx openspec validate "game-sky-steps" --type change --strict` and confirm it passes
- [ ] 5.3 Commit, push the branch, run `python3 tools/verify_remote.py all <branch>`, then dispatch the four-target gate with `gh workflow run ci.yml --ref <branch>` (ADR 0020/0023) and confirm all targets are green
