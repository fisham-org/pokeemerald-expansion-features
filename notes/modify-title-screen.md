# Modifying the Title Screen

> **Note:** Line numbers referenced below are approximate and may shift as the codebase evolves.

## Prerequisites

Tools used in this guide:

- **[Tilemap Studio (TMS)](https://github.com/Rangi42/tilemap-studio)** - required for background/tilemap editing
- **[Photopea](https://www.photopea.com/)** - free browser-based Photoshop alternative (opens `.psd` files, no installation required), otherwise feel free to use Photoshop/other tools
- Pixel art editor - I use **[Aseprite](https://www.aseprite.org/)** but others are fine too

---

## Background

### Replacing the Background Graphic

Follow the guides below - they include useful screenshots so the steps aren't repeated here:

- Iriv24's guide for a) [Tilemap Studio](https://docs.google.com/document/d/1aCr4YhqSbdMkmxGo5UMIgT0wQGE7Pm7ZZCrGt1jt9JY/pub) and b)[required code changes](https://github.com/TeamAquasHideout/Team-Aquas-Asset-Repo/wiki/How-to-Change-the-Title-Screen-for-pokeemerald-(and-expansion))
  - The only change I made from these instructions was explicitly defining the palette colour (the checkbox referred to in step 3 of the Google Docs guide).
  - If the resulting graphic looks completely wrong, double-check that you used the TMS-processed "scrambled" tilemap output - **not** the original `.png` before it was loaded into TMS.
  - Complete all optional steps, including verifying the tilemap palette and increasing the overall image size to 128×288 pixels (fill the new pixels with your transparency colour).
  - The code change described in those guides is slightly different in this codebase. Instead of the change shown there, comment out these two lines in `src/title_screen.c`:

```diff
-DecompressDataWithHeaderVram(sTitleScreenCloudsGfx, (void *)(BG_CHAR_ADDR(3)));
-DecompressDataWithHeaderVram(gTitleScreenCloudsTilemap, (void *)(BG_SCREEN_ADDR(27)));
+// DecompressDataWithHeaderVram(sTitleScreenCloudsGfx, (void *)(BG_CHAR_ADDR(3)));
+// DecompressDataWithHeaderVram(gTitleScreenCloudsTilemap, (void *)(BG_SCREEN_ADDR(27)));
```

Some other notes:
- [Tilemap Studio interface walkthrough (video)](https://www.youtube.com/watch?v=P-IwbpxGSZk) - useful for understanding the TMS interface, I didn't actually use this however
- [Archived guide with sprite/cry instructions](https://web.archive.org/web/20221119023801/https://gamer2020.net/?p=369) - original page is down; useful if you want to add a sprite or cry to the screen
- A good source for placeholder images is [Spriters Resource (Mystery Dungeon)](https://www.spriters-resource.com/ds_dsi/pokemonmysterydungeonexplorersofsky/asset/232317/); location art like [HeartGold/SoulSilver backgrounds](https://www.spriters-resource.com/ds_dsi/pokemonheartgoldsoulsilver/asset/28243/) can also work well with some editing.

---

### Modifying the Rayquaza Glow

Not attempted personally, but [this guide](https://github.com/Bivurnum/decomps-resources/wiki/Title-Screen-Easy-Fade-Colors) explains how to modify the glow effect.

> **Note:** Following the background replacement steps above will remove the glow by default.

---

### Modifying the Clouds

The file `graphics/title_screen/clouds.png` defines the cloud graphic. Following the background replacement steps above will have already commented out all cloud-related code, disabling them entirely. The four lines commented out are:

**Loading the graphics** (`src/title_screen.c`, lines 606-607):

```c
// DecompressDataWithHeaderVram(sTitleScreenCloudsGfx, (void *)(BG_CHAR_ADDR(3)));
// DecompressDataWithHeaderVram(gTitleScreenCloudsTilemap, (void *)(BG_SCREEN_ADDR(27)));
```

**Configuring the BG layer** (`src/title_screen.c`, line 652):

```c
// SetGpuReg(REG_OFFSET_BG1CNT, BGCNT_PRIORITY(2) | BGCNT_CHARBASE(3) | BGCNT_SCREENBASE(27) | BGCNT_16COLOR | BGCNT_TXT256x256);
```

**Applying the scroll offset** (`src/title_screen.c`, line 567):

```c
// SetGpuReg(REG_OFFSET_BG1VOFS, gBattle_BG1_Y);
```

The cloud system is a BG1 tilemap layer that scrolls. To modify or re-enable it, uncomment those four lines and adjust as needed.

#### Changing scroll direction

The clouds originally scroll upward. To reverse this (e.g. make them descend), find `Task_TitleScreenPhase3` (~line 811) and change:

```c
// Before
gTasks[taskId].tBg1Y++;
gBattle_BG1_Y = gTasks[taskId].tBg1Y / 2;

// After
gTasks[taskId].tBg1Y--;
gBattle_BG1_Y = -(gTasks[taskId].tBg1Y / 2);
```

The tilemap wraps seamlessly, so any graphic that tiles vertically will loop infinitely.

Adjust the scroll speed by changing the `/2` divisor - smaller values scroll faster.

---

## Text

### Version Banner (`emerald_version.png`)

The file is 128×32 pixels, a 4bpp indexed PNG, maximum 16 colours (including the first colour in the palette being used for transparency).

The version banner is a sprite (not a tilemap), so no TMS work is needed. The main challenge is getting a clean export with no anti-aliasing fringe.

**Useful resources:**

- [TAH discord - chiaotzu4](https://discord.com/channels/976252009114140682/1151365435854422056/1469228928429461676) - this is the file I used.
- [pret discord - Jaizu](https://discord.com/channels/442462691542695948/722140206190559232/1278289300512964609)
- [pret discord - Yak Attack](https://discord.com/channels/442462691542695948/442465020291317760/1372745953265848321)

**Steps:**

1. Download the `.psd` file shared in the first Discord link above. Open it in [Photopea](https://www.photopea.com/) via *Open from Computer*.
2. **Install the Tondu font before editing the text** - otherwise it will revert to a default font. Download the file from [dafont.com/tondu.font](https://www.dafont.com/tondu.font). In Photopea, switch to the Text tool (`T`), click the font dropdown (top-left), click *Load Font*, and select the `.zip` directly (no need to extract it).
3. Replace the placeholder text. The canvas is 128×32 pixels - the text size can be adjusted freely, and a stroke effect is already applied (adjust thickness as needed). Colour can also be changed here.
4. I had some issues with my export including the transparent magenta blended into the text outline. To avoid anti-aliasing fringe on export, **I hid the magenta background layer** (click the eye icon in the layer panel) before saving as `.png`. This exports with a transparent background instead.
5. Open the exported `.png` in Aseprite. Add a new background layer filled with magenta (255, 0, 255), flatten to a single layer, then convert to indexed colour (*Sprite > Color Mode > Indexed*) with a new 16-colour palette generated from the image. This should result in an image that doesn't blend into the magenta (there may be a far better way to do this, but this worked for me).
6. Save the result as `graphics/title_screen/emerald_version.png` to replace the existing file in-place (feel free to rename the old file to old_emerald_version.png instead if you want to refer back to it). No further code or palette changes are needed.

---

### Press Start & Copyright (`press_start.png`)

This file is 128×24 pixels, and otherwise the same as `emerald_version.png` (i.e. 4bpp indexed PNG, maximum 16 colours)

There is no publicly available font that exactly matches as far as I can tell - in my case, I hand-drew the characters I needed to add. A quick search found this font which is similar but definitely not the same: [Press Start 2P](https://fonts.google.com/specimen/Press+Start+2P) on Google Fonts, or the characters can be traced directly from the existing graphic as a reference.

This isn't super difficult if you keep it simple. I wanted to update the copyright year to this year & include my name in place of Gamefreak. The file you want to modify is graphics\title_screen\press_start.png.
1. Open the file in your pixel art editor. Replace the text with whatever you want - ideally don't change the colours used so you don't need to modify the palette file.
2. The position of the Press Start & copyright information on the title screen is driven by a) the file src/title_screen.c, line 37: `#define START_BANNER_X 128` and b) the image itself. Modifying the #define will shift all text in the press_start.png file - I wanted to keep it simple so I only changed the .png. In my case, I shifted the `(c) 2005 ...` slightly to the right and then added the FI of my name to the right-most part of the middle line, and then the rest to the next line - it is shown as a single line. I'm not 100% sure how this works if you want to get more complex but I am pretty sure you'd update `NUM_PRESS_START_FRAMES` and add/remove the corresponding `sAnim_PressStart_*` entries/animation frames.

---

### Pokémon Logo (`pokemon_logo.png`)

Not attempted personally. The file to replace is `graphics/title_screen/pokemon_logo.png`. The modification process is likely similar to the background graphic (TMS-based tilemap replacement).

---

## Music

### Modifying the Title Screen Music

Not attempted personally. [This post on PokéCommunity](https://www.pokecommunity.com/threads/simple-modifications-directory.416647/page-5#post-10173627) appears to outline the required changes.
