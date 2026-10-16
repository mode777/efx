# 0063 — MPEG-1 video via vendored pl_mpeg, with implicit engine-managed audio

Status: Accepted

## Context

The engine had no video playback: FMV intros, cutscenes, attract loops, and
in-world screens were impossible. The consumer API is fixed-function with a
PS2-era aesthetic; the script layer must stay free of host APIs and host
codecs, and the four targets (Windows/Linux/macOS/Emscripten) must behave
identically. The dynamic-texture seam (ADR 0061) provides the frame→texture
path video needs. Recorded for the `video-playback` change.

## Decision

Video is decoded in software from an in-memory MPEG-1 resource by the
vendored **pl_mpeg** single header (MIT via SPDX notice; `vendor/pl_mpeg`),
behind the pure-C wrapper `src/video/efx_video.{h,c}` (no Sokol/quickjs —
ADR 0003 module walls). The script surface is `efx.video.openVideo(path,
opts?)` returning an opaque Video class (GC-finalized + `destroy()`,
ADR 0011/0012) that exposes a read-only `texture` (an engine-owned dynamic
texture, ADR 0061) and the playback state machine (`play`/`pause`,
`currentTime`/`duration`/`width`/`height`/`ended`, `loop`/`muted`). Drawing
uses the existing draw paths — no video-specific draw API.

Playback is engine-owned and clock-mastered: the engine advances every live
video once per frame by the frame delta (before script update hooks, on both
bindings), decodes the frames due in the elapsed window with bounded
catch-up, and uploads each produced frame to the video's texture. Video
audio (MP2) is decoded in lockstep into a fixed ring and consumed through a
new generic **pull-voice** in the F14 audio core
(`efx_audio_play_pull`), which keeps the audio module video-agnostic. Audio
attachment is implicit — scripts never see a voice, channel, or buffer — and
pause/loop/ended/destroy and the ADR 0057 game swap all couple engine-side;
`muted` silences the voice without touching decoding, and stream-slot
contention falls back to silent video rather than failing.

## Consequences

One vendored header (~0.1 MB wasm) buys MPEG-1 video + audio with no build
graph, no network, and no platform-divergent code paths; the "dated" codec
reads as an intentional aesthetic. The texture seam (ADR 0061) is reused
rather than extended, and the audio core gains one source kind that any
future streaming producer can use. Costs accepted: MPEG-1 file sizes are
2–5× H.264 at equal quality (asset guidelines pin 480p-class clips), decode
is CPU-side on all targets (no hardware decode), there is no seeking in v1,
and the video texture is engine-owned (script `destroy()` is refused).
VP8/WebM remains the pre-planned escalation if author-side sizes hurt; the
script surface would not change.

## Rejected alternatives

- **FFmpeg** — LGPL static-link obligations (fails the F14 libmpg123
  precedent and the single-binary vision) and multi-MB binary growth.
- **Platform hardware decode** (MediaFoundation / VideoToolbox / VAAPI +
  `<video>`/WebCodecs on web) — divergent APIs, no unified Linux surface,
  and different format support per target; violates platform parity.
- **libvpx VP8 + WebM** — BSD and royalty-free, but real build integration,
  an EBML container parser, and Vorbis/Opus audio dependencies for half the
  file size.
- **dav1d / AV1** — best compression but a meson+nasm build that does not
  fit the vendor pattern, heavy wasm decode without SIMD, and overkill for
  the aesthetic.
- **MJPEG in AVI** — zero new decode dependencies but ~10× file sizes and no
  in-band audio.
- **Exposing video audio as a script-held stream/handle** — no positional
  audio exists to use it, and it would push channel/lifecycle management
  into scripts, contradicting the engine-owns-mixing rule (ADR 0042).
