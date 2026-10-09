# Tasks

## 1. Assets

- [x] 1.1 Extend `gallery/scripts/gen-audio-assets.py` with a `game-bot-arena` bank synthesizing `shot`, `hit`, `explosion`, and `wave` WAVs; source or author one CC0 looping music track (committed MP3, pinned-ffmpeg precedent); verify `--check` passes and all files land in `gallery/samples/curated/game-bot-arena/`
- [x] 1.2 Copy the standard Kenney `font.ttf` into the sample directory and add CREDITS rows (font, music provenance, synthesized WAVs)
- [x] 1.3 Add the `manifest.json` entry (`Games` category, description noting twin-stick controls, waves, and the light-budget combat) and regenerate the catalog; verify the sample appears in the generated catalog JSON

## 2. Arena and player

- [x] 2.1 Author `main.js` skeleton: the procedural arena (floor, perimeter walls, pillar cover) as static bodies + rendered meshes with the `createImageData` grid texture and emissive neon trim; verify the arena renders and contains a walking capsule under the player
- [x] 2.2 Implement the player as a `createCharacter` capsule with twin-stick controls: WASD move (camera-relative), mouse cursor → ground-plane raycast aim, or left stick move + right stick aim; fire on LMB/R-trigger/stick-deflect; verify both control schemes move, aim, and fire equivalently
- [x] 2.3 Implement the pooled projectile entities (fixed array, emissive stretched boxes) with sphere-distance hit tests against bots and walls; verify shots fire at a capped rate, respect walls, and the pool never reallocates

## 3. Enemies and waves

- [x] 3.1 Implement chaser bots as dynamic sphere bodies (force-driven seeking with capped speed) and shooter bots (stand-off ring positioning, wall LOS via `raycast` before firing slow bolts); verify bots jostle instead of stacking, shooters hold fire behind cover, and both apply damage with brief i-frames
- [x] 3.2 Implement knockback (`applyImpulse` on hit with visible recoil) and death (burst, score, light claim); verify crowd control emerges — shots shove bots apart and into each other
- [x] 3.3 Implement the wave state machine (INTERMISSION banner → spawn → active → cleared → next wave) with escalating counts/mix/speed from perimeter spawn points away from the player; verify waves advance endlessly and pacing reads clearly

## 4. Presentation, audio, and attract

- [x] 4.1 Implement the light-budget slot pool (explosions > muzzle flashes, steal oldest with fade) over the directional key light; verify combat lights read as intentional and never strobe
- [x] 4.2 Compose the frame: bloom post chain, particle bursts on hits/deaths, screen shake by event scale, hit-flash emissive pulses, blob shadows under actors, HUD text (score, wave, health bar); verify the arena stays readable under full load
- [x] 4.3 Wire audio: SFX bank with pan-from-screen-x and pitch jitter, streamed music with M / `[` / `]` controls, web unlock on first input; verify every combat event sounds and the loop plays
- [x] 4.4 Implement ATTRACT (patrol movement, aim/fire at nearest bot, occasional taken hits, quiet restart on death) and first-input handover; verify launch self-plays and the first input takes over mid-wave
- [x] 4.5 Implement game over (health depleted → slow-mo burst → screen → any-input restart with state reset); verify restart reuses resources without leaks across several cycles

## 5. Verification

- [x] 5.1 Run the built gallery sample through attract → several played waves → game over → restart with no console or page errors at steady frame rate on the web build; verify the gallery smoke run includes the sample and passes
- [x] 5.2 Run `npx openspec validate "game-bot-arena" --type change --strict` and confirm it passes
- [x] 5.3 Commit, push the branch, run `python3 tools/verify_remote.py all <branch>`, then dispatch the four-target gate with `gh workflow run ci.yml --ref <branch>` (ADR 0020/0023) and confirm all targets are green
