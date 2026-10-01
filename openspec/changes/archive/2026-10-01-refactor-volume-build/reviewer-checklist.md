# Reviewer checklist — AGENTS.md slimming (task 6.3)

Facts removed from the former "Current state" narrative (old L8–L441),
each sampled and traced to where it remains reachable. Checked 2026-10-01
against `openspec/changes/archive/`, `docs/decisions/`, and the roadmap spec.

| # | Removed fact (former AGENTS.md location) | Reachable at |
|---|------------------------------------------|--------------|
| 1 | F12 ADR is ADR 0040; the tunneling hardening is ADR 0045; the GC-finalizer misdiagnosis is ADR 0046 (old F12 bullet, L163–170) | Milestones list (F12 line); `docs/decisions/0040/0045/0046`; Roadmap table F12 row |
| 2 | F5b effect list: `colorFilter`, `blur`, `bloom`, per-entry `mix`, ≤ 8 entries (old F5 bullet, L221–223) | ADR 0029; archived change `openspec/changes/archive/2026-09-27-f5b-post-fx/design.md`; spec `openspec/specs/post-fx`; `docs/api/` (`setPostEffects`) |
| 3 | F14 voice count: fixed 32-voice bank, 4-stream cap (old F14 bullet, L193–194) | ADR 0042 + ADR 0047; archived `2026-09-30-f14-audio` + `2026-09-30-audio-source-model`; `docs/api/` (`efx.audio`) |
| 4 | Gate run id for F6d REPL: ci run 36367478373 (old F6 bullet, L40) | Roadmap table F6 row; archived `2026-09-28-f6d-repl/` (its gate record) |
| 5 | F7 posing semantics: clip by name/`clipN`, time wraps modulo clip length, weights normalized (old F7 bullet, L52–56) | ADR 0035; archived `2026-09-28-f7-skinning-animation/`; `docs/api/` (`poseMesh`) |
| 6 | F10 resolver restrictions: relative/root-relative, exact then `.js`, `.json`, escape rejection, no bare specifiers (old F10 bullet, L86–88) | ADR 0037; archived `2026-09-28-f10-commonjs-modules/`; spec `openspec/specs/` (modules delta); `docs/api/` |
| 7 | F9 iframe-focus follow-up: mouse bubbling so a click focuses the embed; gallery smoke asserts it (old F9 bullet, L77–81) | ADR 0043; change folder `openspec/changes/web-keyboard-focus/` (Milestones list links it) |
| 8 | F8a atlas details: RGBA8 fixed atlas, default printable Latin-1, optional baked outline/shadow (old F8a bullet, L100–101) | ADR 0038; archived `2026-09-28-f8a-font-typesetting/`; `docs/api/` (`createFont`) |
| 9 | F6e: `loadTexture` removed; `mipmaps` option with a CPU 2×2 box-filter chain (old F6 bullet, L44–47) | ADR 0034; archived `2026-09-28-f6e-texture-creation-options/`; `docs/api/` (`createTexture`) |
| 10 | F4b map set: `ambient`/`diffuse`/`specular`/`emissive` maps, binary alphaMask at 0.5, five always-bound samplers with white fallback (old F5/F4 bullet, L243–248) | ADR 0027; archived `2026-09-27-f4b-maps-alpha-masks/`; `docs/api/` (material options) |
| 11 | F12 character API surface: `moveAndSlide`, floor/wall/ceiling classification, step-up, `maxSlides`, `safeMargin`, one-way dynamic push (old F12 bullet, L152–156) | ADR 0040; archived `2026-09-29-f12-collision-physics/`; `docs/api/` (`Character`) |
| 12 | F2 run-mode capture: `--capture-frame N --capture-output file` (old run-modes bullet, L276–277) | ADR 0020 + ADR 0007; codebase map run-modes bullet (kept condensed) |
| 13 | Gallery samples zip: `gallery/scripts/pack-samples.mjs` derives the release `emotion-fx-<version>-samples.zip` (old gallery bullet, L351–354) | ADR 0044; change `openspec/changes/curated-sample-dirs/`; `gallery/scripts/` itself |
| 14 | F13 details: GUID selection with permissive fallback, half-axis/inversion/hat handling, 0.5 trigger threshold, raw fallback (old F13 bullet, L176–178) | ADR 0041; archived `2026-09-29-gamepad-input/`; `docs/api/` (`efx.gamepad`) |
| 15 | F14 autoplay unlock + no-device soft-fail (old F14 bullet, L204) | ADR 0042/0047; archived `2026-09-30-audio-source-model/`; `docs/api/` (`efx.audio`) |
