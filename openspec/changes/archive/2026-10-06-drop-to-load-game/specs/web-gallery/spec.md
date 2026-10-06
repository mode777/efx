# Spec Delta

## ADDED Requirements

### Requirement: Dropped archive runs as a sample

The gallery runner SHALL accept a zip archive dropped onto the application
area as the active sample, loading it under the same resource-root contract as
a selected sample, and SHALL suppress the browser's default handling of the
dropped file so the page is not navigated away. A dropped file that is not a
usable sample archive SHALL surface through the sample error channel without
breaking the gallery shell.

#### Scenario: Dropped archive replaces the running sample

- **WHEN** the visitor drops a zip archive containing `main.js` onto the
  application area
- **THEN** that archive is mounted as the resource root and its entry script
  runs in place of the previously selected sample

#### Scenario: Browser default is suppressed

- **WHEN** the visitor drops a file onto the application area
- **THEN** the browser does not open or download the file, and the gallery
  shell remains displayed

#### Scenario: Unusable drop is surfaced

- **WHEN** the visitor drops a file that is not a usable sample archive
- **THEN** the gallery reports the failure through the sample error channel and
  the rest of the gallery remains usable
