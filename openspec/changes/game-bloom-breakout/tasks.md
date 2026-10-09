# Tasks

## 1. Assets

- [x] 1.1 Extend `gallery/scripts/gen-audio-assets.py` with a `game-bloom-breakout` bank synthesizing four short WAVs (`paddle`, `brick`, `wall`, `life`); verify `--check` passes (byte-reproducible regeneration) and the WAVs land in `gallery/samples/curated/game-bloom-breakout/`
- [x] 1.2 Create `gallery/samples/curated/game-bloom-breakout/`, copy the standard Kenney `font.ttf`, and add CREDITS rows (font + "WAVs synthesized by `gen-audio-assets.py`" per the `audio-showcase` precedent)
- [x] 1.3 Add the `manifest.json` entry (`Games` category, description noting particles/bloom/audio feedback) and regenerate the catalog; verify the sample appears in the generated catalog JSON

## 2. Core game loop

- [x] 2.1 Author `main.js` skeleton: 640×480 frame, state machine (ATTRACT/SERVE/RALLY/LIFE-LOST/GAME-OVER/WIN), three data-authored brick layouts with hit-tier colors, three lives, named tuning constants; verify boot under the player and that layout 1 renders its brick grid
- [x] 2.2 Implement paddle (mouse + arrows, last-input-wins), ball with script AABB collisions against walls/paddles/bricks, per-rally speed ramp, layout clear advancing to the next layout, life loss, game over, win screen, and any-input restart; verify one full game — clear all three layouts or lose all lives — plays and restarts
- [x] 2.3 Implement the attract AI paddle (capped-speed ball tracking with error term) and the first-input handover; verify launch self-plays and the first input takes the paddle mid-rally

## 3. Presentation and juice

- [x] 3.1 Generate the shard/glow texture with `createImageData`, create the single additive particle system, and emit a tinted burst on brick destruction; verify brick deaths visibly burst with the brick's color
- [x] 3.2 Author the post chain (bloom, optional mild color filter) over the frame per the `post-bloom-tv` pattern; verify neon bricks, ball, and bursts glow while score text stays crisp
- [x] 3.3 Fire the four WAVs as one-shots with pan-from-ball-x and ±5% pitch jitter on paddle/brick/wall/life events; verify each event sounds and web audio unlocks on first input
- [x] 3.4 Implement screen shake as a decayed offset added to all quad positions (text excluded), scaled by event; verify life loss shakes hardest and text never shakes

## 4. Verification

- [ ] 4.1 Run the built gallery sample through the full loop (attract → game → end → restart) with no console or page errors at steady frame rate on the web build; verify the gallery smoke run includes the sample and passes
- [ ] 4.2 Run `npx openspec validate "game-bloom-breakout" --type change --strict` and confirm it passes
- [ ] 4.3 Commit, push the branch, run `python3 tools/verify_remote.py all <branch>`, then dispatch the four-target gate with `gh workflow run ci.yml --ref <branch>` (ADR 0020/0023) and confirm all targets are green
