# Spec Delta

## ADDED Requirements

### Requirement: Web keyboard focus in an embed

On the web target, keyboard input SHALL reach scripts once the player's
document has focus, including when the player is embedded in an iframe on a
host page that currently holds focus. A pointer press on the player's canvas
SHALL move focus into the player's document so that subsequent key events are
delivered, and the engine SHALL NOT suppress the browser default that
performs this focus transfer. The engine SHALL continue to suppress the
browser default for game keys (so a game key does not scroll or navigate the
host page), independently of pointer-focus handling. Native (non-web) input
behavior SHALL be unchanged.

#### Scenario: Clicking the embed focuses it and keys arrive

- **WHEN** the player runs in an iframe and the host page holds focus, and the visitor presses the pointer on the player's canvas
- **THEN** the player's document gains focus and a subsequent key-down is reported by `efx.keyboard` (both the event callback and the held-state query)

#### Scenario: Game keys stay suppressed

- **WHEN** a game key such as space or an arrow is pressed while the player's canvas has focus
- **THEN** the engine suppresses the browser's default action for that key while still reporting it to scripts

#### Scenario: Desktop behavior is unchanged

- **WHEN** the player runs on a native target
- **THEN** keyboard and mouse input behave as before, with no dependency on browser focus
