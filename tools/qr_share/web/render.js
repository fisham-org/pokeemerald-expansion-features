// Draws the share image: the chosen background with the player and party placed on
// tile coordinates from backgrounds.json. Sprites stand bottom-centre on their tile.

const TILE = 16;
const SCALE = 3;

function loadImage(src) {
    return new Promise((resolve, reject) => {
        const image = new Image();
        image.onload = () => resolve(image);
        image.onerror = () => reject(new Error(`Could not load ${src}`));
        image.src = src;
    });
}

export async function drawScene(canvas, background, sprites) {
    const backgroundImage = await loadImage(background.image);
    const placed = await Promise.all(sprites.map(async ({ src, tile }) => ({ image: await loadImage(src), tile })));

    canvas.width = backgroundImage.width * SCALE;
    canvas.height = backgroundImage.height * SCALE;
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.scale(SCALE, SCALE);
    ctx.drawImage(backgroundImage, 0, 0);

    // Lower rows are drawn last so they overlap the ones behind them.
    placed.sort((a, b) => a.tile[1] - b.tile[1]);
    for (const { image, tile } of placed) {
        const x = tile[0] * TILE + TILE / 2 - image.width / 2;
        const y = (tile[1] + 1) * TILE - image.height;
        ctx.drawImage(image, x, y);
    }
}

// The species whose sprites to draw: its Mega form if it holds that Mega Stone and the data
// was exported with QR_SHARE_SHOW_MEGA_FORMS ("mega": [[item, species], ...]), otherwise itself.
export function spriteSpecies(mon, names) {
    const mega = names.species[mon.species]?.mega?.find(([item]) => item === mon.item);
    return names.species[mega ? mega[1] : mon.species] ?? names.species[0];
}

// The sprites for a decoded code: the player, then each party member on its slot's tile.
export function sceneSprites(data, names, background) {
    const sprites = [{ src: names.player[data.trainerGender], tile: background.player }];
    data.party.forEach((mon, i) => {
        const species = spriteSpecies(mon, names);
        sprites.push({ src: mon.shiny ? species.shinySprite : species.sprite, tile: background.party[i] });
    });
    return sprites.filter((sprite) => sprite.src && sprite.tile);
}
