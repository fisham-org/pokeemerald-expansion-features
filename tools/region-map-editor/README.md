# Region Map Editor

A browser-based editor for designing GBA-style region maps (240×160px, 30×20 tiles). Built for the pokeemerald-expansion project but usable for any GBA region map work.

## What it does

- Load a source PNG (indexed-color region map base image)
- Paint tile regions and assign them to named locations (towns, cities, routes, etc.)
- Apply terrain palettes: recolors grass tiles to route/desert/snow/etc., water tiles to surf/dive/no-access patterns
- Routes automatically recolor embedded water tiles using a "land route" alternating-line pattern
- Drag locations to move them; click locations in the list or on the map to edit inline
- Export the composited map as a PNG, or export location/palette data as YAML or JSON
- Generate Lua scripts for Aseprite to apply the same edits to a source .ase file
- Analyze tile and color counts against GBA limits (256 unique 8×8 tiles, 16 colors)
- Save/load full editor state as `config.json`

## Running

```bash
cd tools/region-map-editor
python3 serve.py          # starts at http://localhost:8080
python3 serve.py 9000     # custom port
```

## Architecture

The app is a static single-page application with no build step. All JS runs in global scope; files are loaded via ordered `<script>` tags at the bottom of `index.html`. No ES modules, no bundler.

### File overview

```
index.html          HTML structure only — no inline JS or CSS
css/editor.css      All styles
serve.py            Zero-dependency Python HTTP server
js/
  constants.js      Read-only constants: TILE size, GBA limits, PRESETS palette data, BORDER_DATA_URL
  state.js          All mutable global state (declared as let, assigned in init.js)
  helpers.js        Pure utility functions: color conversion, slugify, download, isWaterPixel
  render.js         Canvas rendering: render(), renderColorReplacedMap(), renderIcons()
  panels.js         UI logic: edit panel reparenting, location/icon CRUD, tab switching, dropdowns
  palettes.js       Palette and source-greens management
  config.js         Save/load full editor state as config.json
  export.js         YAML/JSON/PNG export, YAML/JSON import, Aseprite Lua script, tile analysis
  mouse.js          All canvas/window/document event listeners (wrapped in initMouseHandlers())
  init.js           Assigns DOM refs, defines setZoom()/updateStatus(), wires file input, runs startup
```

### Script load order

Scripts must load in this order (each depends on those above it):

```
constants.js → state.js → helpers.js → render.js → panels.js →
palettes.js → config.js → export.js → mouse.js → init.js
```

`init.js` runs last and calls `initMouseHandlers()`, `initDefaultPalettes()`, `refreshBiomeSelect()`, and `updateSourceGreensDisplay()` to boot the app.

### Key data structures

**Location**
```js
{
  name: string,
  tiles: Set<"col,row">,   // e.g. new Set(["3,5", "4,5"])
  type: "town" | "city" | "route" | "landmark" | "dungeon" | "other",
  palette: string,          // palette name, or "" for default grass
  icon: string,             // icon label, or ""
  biome: string,
  description: string,
  notes: string,
  color: "#rrggbb"          // random highlight color for the overlay
}
```

**Palette**
```js
{
  name: string,
  colors: string[],         // 5 hex colors (land) or 5 with 2 alternating (water)
  waterType: "surf" | "dive" | "noaccess" | "landroute" | null
}
```

**Icon**
```js
{
  label: string,
  img: HTMLImageElement,
  widthTiles: number,
  heightTiles: number,
  dataURL: string           // stored for config.json serialization
}
```

### Rendering pipeline

`render()` composites three canvases:

1. **bg-canvas** — raw source image, drawn once on zoom/load
2. **render-canvas** — color-replaced image + icons + border overlay
3. **overlay-canvas** — selection highlights, hover, active borders, grid (redrawn every frame)

`renderColorReplacedMap()` builds a `tileColorMap` (one entry per tile, last location wins for overlaps) then pixel-walks each tile:
- **Water palettes**: non-green pixels → alternating line pattern (2 colors)
- **Land palettes**: green pixels → mapped 1:1 by source shade; non-green land pixels (e.g. junction tiles) → nearest shade by luminance
- **Route type**: land pixels use luminance mapping; water-dominant pixels (`b > r*1.2 && b > g*1.1`) → `water_landroute` alternating lines

### Edit panel reparenting

There is a single `#edit-panel` DOM element that gets physically moved between two locations:
- **Sidebar (list mode)**: appended to a `#list-panel-slot` div inserted after the active list item
- **Floating (map mode)**: appended to `#float-edit-container`, positioned near the click point

`refreshLocList()` detaches the panel before clearing `innerHTML` to avoid destroying it. `showEditPanel(mode, event)` handles the reparenting logic.

### Drag-to-move

On `mousedown` over a location tile, the app enters drag-prep mode (`dragLocIdx`, `dragStartTile`). `isDragging` only becomes `true` once the pointer moves to a different tile — this lets a pure click still open the edit panel. On `mousemove`, tile offsets are applied in real time. On `mouseup`, if dragging: finalize + open float panel at drop position; if not dragging: open float panel at click position. Escape cancels and restores `dragOrigTiles`.

## GBA constraints

- **256 unique 8×8 tiles** — exceeded tiles will break the tileset
- **16 colors per palette** — applies per tileset palette slot
- Use the Tile & Color Analysis tool (Export tab) to check before finalizing
