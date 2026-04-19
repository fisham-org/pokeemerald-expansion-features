# Maps and Tilesets for pokeemerald-expansion

A collection of ready-to-use maps and tilesets for the [pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion) codebase. Based off RHH's pokeemerald-expansion 1.15.0.

Each map is developed on its own branch so you can pick and choose what to merge into your project.

> Note: In addition to hand-writing it, AI has been used to generate documentation & code used for these features.

## How to Use

Each map lives on its own branch under `feature/maps-and-tilesets/`:

| Branch | Description |
|--------|-------------|
| `feature/maps-and-tilesets/main` | Base branch (shared foundation) |
| `feature/maps-and-tilesets/primary` | A simple proof of concept for evaluating how to edit the general Emerald primary tileset & adding sideways/north-facing stairs |

To add a map to your project, merge the relevant branch:

```bash
git remote add maps-and-tilesets <this-repo-url>
git fetch maps-and-tilesets
git merge maps-and-tilesets/feature/maps-and-tilesets/primary
```

## How to Modify

Tilesets are compiled using [Porytiles](https://github.com/grunt-lucas/porytiles). See [notes/porytiles.md](notes/porytiles.md) for setup instructions and the **Workflow** section for the edit-compile-reload cycle.

## Maps

These maps use **triple layer metatiles**. Follow the instructions at [Triple Layer Metatiles](https://github.com/pret/pokeemerald/wiki/Triple-layer-metatiles) to set this up in your project before merging.

### Primary (Proof of Concept)

This is essentially just the standard general tileset, with a minor modification to add sideways/north-facing stairs. You probably shouldn't use this.

> Note that secondary tilesets that use this primary tileset (e.g. gTileset_Slateport) need to be recompiled as the palette indexes they use from the primary tileset have shifted.

**Tileset:**

![Primary Tileset](raw_tilesets/primary_poc/tilesetase.png)

**Prairie2 Map:**

![Primary Map](raw_tilesets/primary_poc/Primary_PoC.png)

See [raw_tilesets\primary_poc\credits.md](raw_tilesets\primary_poc\credits.md) for tileset credits.
