# Spec Delta: web-gallery

## MODIFIED Requirements

### Requirement: Presentation style

The gallery shell SHALL present a PlayStation-2-era visual style — a dark
palette, glowing accents, and beveled panels — with the running application
as the visual focus. The published API reference under `/api` SHALL use the
same PlayStation-2-era palette so the reference and the gallery read as one
site.

#### Scenario: Consistent retro presentation

- **WHEN** the gallery is displayed
- **THEN** the catalog, application area, and editor share the retro styled
  shell and the running application remains the visual focus

#### Scenario: Reference shares the palette

- **WHEN** the published API reference is displayed
- **THEN** it uses the same PlayStation-2-era palette as the gallery shell

### Requirement: Static site build

The gallery SHALL build to a static bundle that includes the Emscripten web
player, the sample catalog, the editor, and the generated API reference under
`/api`, and that is suitable for serving from GitHub Pages.

#### Scenario: Built bundle is self-contained

- **WHEN** the gallery is built for deployment
- **THEN** the output contains the web player module and data, the sample
  catalog, the editor assets, and the API reference HTML under `api/` needed
  to serve the gallery statically

#### Scenario: Reference is reachable from the gallery

- **WHEN** a visitor is viewing the gallery
- **THEN** a link to the API reference under `/api` is available, and the
  reference links back to the gallery
