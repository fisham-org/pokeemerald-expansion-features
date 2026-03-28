### Prairie

A prairie/savanna/desert mesa featuring custom tiles, wild encounters, trainers, and item pickups. Includes two example maps:

- **Prairie**
- **Prairie2**

**Tileset:**

![Prairie Tileset](raw_tilesets/prairie/tilesetase.png)

**Prairie2 Map:**

![Prairie2 Map](raw_tilesets/prairie/Prairie2.png)

See [raw_tilesets/prairie/credits.md](raw_tilesets/prairie/credits.md) for tileset credits.

## Implementation Details

This tileset includes two custom grass types that require new metatile behaviors and field effect animations:

- **Tall Grass** (`MB_PRAIRIE_TALL_GRASS`) - the regular wild encounter grass
- **Long Grass** (`MB_PRAIRIE_LONG_GRASS`) - taller, denser grass with a different walking animation

See [notes/how-to-add-new-grass-metatile-behavior.md](../../notes/how-to-add-new-grass-metatile-behavior.md) for the full step-by-step guide on implementing these.

| File | What to add |
|------|------------|
| `include/constants/metatile_behaviors.h` | Enum entry |
| `include/constants/field_effects.h` | FLDEFF, FLDEFFOBJ, and PAL_TAG constants |
| `include/event_object_movement.h` | Ground effect flag defines |
| `include/field_effect_helpers.h` | Callback and finder function declarations |
| `include/metatile_behavior.h` | Function declarations |
| `src/metatile_behavior.c` | Behavior check functions + updates to existing checks |
| `src/event_object_movement.c` | Ground effect flags, handlers, array entries, aggregator calls |
| `src/field_effect_helpers.c` | Creator + callback + finder functions |
| `data/field_effect_scripts.s` | Script table entries + script definitions |
| `src/data/field_effects/field_effect_objects.h` | Palette struct, pic tables, sprite templates |
| `src/data/field_effects/field_effect_object_template_pointers.h` | Extern declarations + array entries |
| `src/data/object_events/object_event_graphics.h` | INCBIN declarations (Option A only) |
| `spritesheet_rules.mk` | Build rules for .png to .4bpp (Option A only) |
| `graphics/field_effects/pics/` | Sprite sheet PNGs (Option A only) |
| `graphics/field_effects/palettes/` | JASC-PAL palette file (if new palette) |