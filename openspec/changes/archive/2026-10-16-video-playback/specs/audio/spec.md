# audio Delta

## ADDED Requirements

### Requirement: Video playback audio

When a Video (see the `video-playback` capability) plays, the engine SHALL
attach the video's decoded audio — MPEG audio layer 2 PCM in the engine's
internal stereo float32 sample format — as exactly one streaming voice in
the fixed voice bank, consuming one of the documented streaming slots. The
attachment SHALL be implicit and engine-managed: scripts SHALL NOT receive a
playback handle, channel, or buffer for video audio, and no new audio source
kind SHALL be script-visible. Starting, pausing, resuming, looping, ending,
and destroying the video SHALL apply identically to its audio voice. A
video's `muted` property SHALL silence its audio voice without affecting
video decoding or the texture. When no streaming slot can be satisfied under
the existing documented steal policy, the video SHALL play silent rather
than failing; when the video has no audio track, playback proceeds normally
with no voice attached. Video audio SHALL NOT survive the in-place game swap
(ADR 0057) — it is released with all other voices by the existing
stop-all path.

#### Scenario: Video play is audible and mute silences it

- **WHEN** a script plays a video with an audio track, then sets
  `video.muted = true`
- **THEN** audio is audible while unmuted, silences when muted, and video
  frames keep updating while muted

#### Scenario: Playback transitions couple audio

- **WHEN** a playing video is paused, looped past its end, or destroyed
- **THEN** its audio pauses, wraps in sync with the video clock, or stops
  and releases respectively

#### Scenario: Stream-slot contention plays silent

- **WHEN** a video starts playing while all streaming slots are held by
  other streams and the documented steal policy yields none
- **THEN** the video plays with video only — no error, no audible audio —
  and `ended`/`currentTime` behavior is unaffected

#### Scenario: Video with no audio track

- **WHEN** a script plays an MPEG-1 resource that carries no audio stream
- **THEN** video playback proceeds normally with no voice allocated

#### Scenario: Game swap stops video audio

- **WHEN** an in-place game swap (ADR 0057) occurs while a video plays
- **THEN** the video and its audio voice are released with the previous
  game and nothing remains audible
