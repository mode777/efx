# video-playback Specification

## Purpose
TBD - created by archiving change video-playback. Update Purpose after archive.

## Requirements

### Requirement: MPEG-1 video loading and texture streaming

The engine SHALL expose `efx.video.openVideo(path, opts?)` loading an MPEG-1
resource (video and, when present, MPEG audio layer 2) from the resource
provider (folder or zip, F6a) as an opaque Video object. `opts` SHALL accept
`loop` (boolean, `false` default) and `muted` (boolean, `false` default); an
unknown field or wrongly-typed value SHALL throw `TypeError`. Opening SHALL
validate the container/codec: bytes that are not a parseable MPEG-1 program
stream SHALL throw. A live Video SHALL expose a read-only `texture` property:
an engine-owned dynamic Texture sized to the video's frame size that receives
decoded frames as they are produced. The texture SHALL be usable in any
script-facing draw or map slot that accepts a Texture, with no
video-specific drawing surface; `destroy()` on the video SHALL destroy the
texture. Bytes SHALL decode deterministically: decoding the same resource
SHALL produce identical pixels on every target (golden-scene contract,
ADR 0020). In a run mode without a rendering surface, opening a video and
advancing it SHALL be accepted — frames decode and time advances, but
nothing is uploaded or rendered.

#### Scenario: Open and draw a decoded frame

- **WHEN** a script opens a committed test video, calls `play()`, advances
  the engine by one video frame period, and draws `video.texture`
  full-frame
- **THEN** the draw is accepted and the rendered frame matches the golden
  image captured from the same decode

#### Scenario: Non-MPEG-1 bytes are rejected

- **WHEN** `openVideo` is given a resource that is not a parseable MPEG-1
  stream (for example a PNG or a truncated file)
- **THEN** the call throws and no Video object is created

#### Scenario: Options are validated

- **WHEN** `openVideo` receives an unknown option field, a non-boolean
  `loop`/`muted`, or a non-string path
- **THEN** the call throws `TypeError` and creates no Video

#### Scenario: Video texture works in existing draw paths

- **WHEN** a script binds `video.texture` as a material `diffuseMap` and
  draws a lit mesh
- **THEN** the mesh samples the video's current frame like any Texture

#### Scenario: Video advances without a rendering surface

- **WHEN** a `--script` run opens a video and repeatedly advances while
  playing
- **THEN** `currentTime` advances through the duration without error and
  nothing is rendered

### Requirement: Video playback state machine

A Video SHALL start paused at time 0. `play()` SHALL start advancement;
`pause()` SHALL hold the current time. While playing, video time SHALL
advance with the engine frame delta, and the decoder SHALL produce exactly
the frames needed to cover the elapsed time (no unbounded decode-ahead).
`currentTime`, `duration`, `width`, `height` SHALL be read-only;
`loop` and `muted` SHALL be read-write. A non-looping video SHALL stop at
its end: `ended` SHALL become `true`, the last frame SHALL remain the
texture contents, and further advancement SHALL NOT occur; `play()` after
ending SHALL restart from the beginning. A looping video SHALL wrap to its
beginning and continue. `destroy()` SHALL release the video and its texture
and stop any attached audio; it SHALL be idempotent, and every other member
of a destroyed Video SHALL throw `TypeError`. Videos SHALL NOT survive the
in-place game swap (ADR 0057): all videos SHALL be released with the rest of
the game state.

#### Scenario: Play advances and pause holds

- **WHEN** a script plays a video, advances the engine across several
  frames, then pauses and advances again
- **THEN** `currentTime` advanced while playing and holds exactly at the
  pause point afterwards

#### Scenario: Non-looping video ends and restarts

- **WHEN** a non-looping video is advanced past its duration
- **THEN** `ended` is `true`, `currentTime` equals `duration`, the texture
  holds the final frame, and a following `play()` restarts from 0

#### Scenario: Looping video wraps

- **WHEN** a video with `loop: true` is advanced past its duration
- **THEN** `currentTime` wraps and playback continues from the beginning
  with no gap in texture updates

#### Scenario: Destroy lifecycle

- **WHEN** a script calls `destroy()` on a video and then calls `play()` or
  reads `currentTime` or `texture`
- **THEN** the calls throw `TypeError`, a second `destroy()` is a no-op, and
  no attached audio remains audible

### Requirement: Deterministic decode fixtures and golden coverage

The repository SHALL carry a committed procedural MPEG-1 test fixture
(generated at authoring time with an external encoder; the encoder is a
generation-time tool, never linked into the player) used by unit tests and a
golden scene that draws a decoded frame. The golden scene SHALL be gated on
all four targets per ADR 0020, proving desktop/web decode parity.

#### Scenario: Golden parity across targets

- **WHEN** the video golden scene runs on the four-target gate
- **THEN** each target's captured frame matches the committed golden image
