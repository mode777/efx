# Spec Delta

## ADDED Requirements

### Requirement: Interactive keyboard input is exercised by the smoke run

The gallery smoke run SHALL verify that keyboard input reaches a running
sample through the embed, not only that a sample boots. It SHALL move focus
into the application area (for example by clicking the runner canvas) and
send a key event, then assert that the running sample observed the key
through `efx.keyboard`. The smoke SHALL fail when keyboard input does not
reach the running sample.

#### Scenario: Keyboard reaches the sample after interaction

- **WHEN** the smoke run focuses the application area and sends a key-down to a sample that records `efx.keyboard` state
- **THEN** the sample reports the key as down and the smoke check passes

#### Scenario: Missing keyboard delivery fails the smoke

- **WHEN** the runner does not deliver keyboard input to the running sample after the smoke focuses and sends a key
- **THEN** the smoke reports a failure and exits non-zero
