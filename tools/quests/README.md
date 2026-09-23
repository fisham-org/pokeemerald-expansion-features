# Quest tools

| File | Purpose |
|------|---------|
| `quests_to_header.py` | Turns `src/data/quests/*.json` into `include/constants/quests.h` and `src/data/quests.h`. `make` runs it; `--check` only validates |
| `quest_log_bg.py` | Builds the quest log background (`graphics/quest_log/bg_tiles.png` and the `.bin` tilemaps) from `graphics/quest_log/screens/` |
| `quest_editor.py`, `quest_editor.html` | The quest editor, below |

## Quest editor

A local web app for creating and editing quest files without hand-writing JSON. It reads and writes
`src/data/quests/*.json` and `order.json` directly, and checks every save with `quests_to_header.py --check`.

### Quick start

```bash
python3 tools/quests/quest_editor.py            # from anywhere; it finds the repo from its own location
python3 tools/quests/quest_editor.py --port 9000
```

Then open <http://localhost:8765>. It only listens on `127.0.0.1`. Stop it with Ctrl+C.

**Requirements:** Python 3 (standard library only).

After saving, run `make` to rebuild. The editor doesn't build the ROM, and it doesn't touch map scripts.

### Layout

- **Top bar:** **New quest**, **New task**, the validation status, and **Validate**. Click the status to see the
  generator's messages; click a message to open the quest it names.
- **Left panel:** every quest, grouped as *Main quests (QUESTS tab)* and *Side quests (TASKS tab)*, with each task
  indented under its parent. The search box filters by name or id. A `•` marks unsaved changes.
- **Centre:** three tabs: **Editor**, **Chains** and **Help**.

### Editing a quest

Choose a quest on the left, or press **New quest** / **New task**. The editor shows these cards:

- **Quest / Task:** name, id, category (or, for a task, its parent quest), tags, summary, giver name and icon.
  - For a new quest the id fills itself in from the name (`Lab Errand` becomes `QUEST_LAB_ERRAND`) until you edit it.
  - **Use giver's sprite** sets the icon to the first giver's overworld sprite. Use overworld sprites for every
    quest; the editor warns otherwise, because mixed sprite sizes glitch for a frame when paging in the log.
- **Givers:** the NPCs that give the quest. Pick a map, then an NPC from that map's object events (local ids come
  from each `map.json`).
- **Stages:** each stage has an id, a title (the heading above its objectives in the log), the paths it's on, its
  objectives and its turn-in NPCs.
  - **↑ / ↓** reorder, **Delete** removes a stage, and **Retire** replaces it with `{ "removed": "STAGE_X" }` so
    later stage numbers stay the same.
  - Each objective has its text, **Optional**, a target (an NPC, or a whole map), **Completes when** (its
    condition) and **Hidden until** (its reveal condition). Pick a condition type from the list and the form changes
    to match. **Completes when: None** means a script ticks it with `completeobjective`. **Show x/y** controls the
    progress count where the type supports it.
  - Up to 6 objectives per stage.
- **Outcomes:** up to 4, each with an id, the summary the log shows once complete, and the quests it closes.
- **Chain:** the quest's links to other quests (conditions, tasks, closes), plus **+ Follow-up quest** (a new quest
  whose first objective waits for this one to complete) and **+ Task for this quest** (a new task with this quest as
  parent, tagged with the tag this quest counts).

A task opens in the **short form**: one objective and a turn-in, which the generator turns into one stage.
**Use full stages** switches it to normal stages. Tasks get one outcome, "Done.", unless you press
**Customise outcomes**.

Datalists suggest maps, flags, vars, items, species, types, abilities, natures, sprites and existing tags as you
type. They're read from the repo's headers once when the editor starts, so restart it after adding new constants.

### Saving, renaming and deleting

- **Save** (or Ctrl+S) writes the quest's file and `order.json`, then validates. Before saving it asks about blank
  fields that pass validation but break the build or show nothing in game (name, icon, giver, stage titles,
  objective text, outcome summaries).
- A **new quest** is appended to `order.json` and saved as `<id without QUEST_, lower case>.json`.
- **Renaming** an id keeps its place in `order.json` (its save slot) and updates the id in every other quest file.
  Map scripts that use the old id must be changed by hand.
- **Delete** removes the file and turns its `order.json` entry into `{ "removed": "QUEST_X" }`, so the slot is
  never reused. It first lists any quests that refer to it.
- **Revert** reloads the saved version. **Discard** throws away a quest that was never saved.

Quest ids are save slots, and stage and objective positions are stored in saves too. Reorder and delete freely
before release; after release, only append quests and retire stages.

### Chains tab

A graph of every link between quests, laid out left to right in the order they unlock:

- **task:** a task and its parent
- **req:** a quest whose condition reads another quest's state
- **count:** a quest counted by a *Quests with a tag complete* objective
- **close:** an outcome that closes another quest (dashed)

Click a box to open that quest. **Show unlinked quests** adds quests with no links.

### Validation

**Validate** runs `quests_to_header.py --check` over all quest files, not just the open one. The status reads
**Valid**, or the number of errors. The messages are the same ones `make` prints, so a clean validation means the
generator step of the build will pass.
