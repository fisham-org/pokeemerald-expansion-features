import { decodePayload, payloadFromPath, readDataVersion } from "./decode.js";
import { formatSet, formatTeam } from "./pokepaste.js";
import { drawScene, sceneSprites, spriteSpecies } from "./render.js";

const $ = (id) => document.getElementById(id);

function show(id) {
    $(id).hidden = false;
}

function setStatus(message) {
    $("status").textContent = message;
    $("status").hidden = !message;
}

async function fetchJson(url) {
    const response = await fetch(url);
    if (!response.ok)
        throw new Error(`${url}: ${response.status}`);
    return response.json();
}

function text(bytes, charset) {
    return bytes.map((b) => charset[b] ?? "").join("");
}

function renderTrainer(data, names) {
    $("trainer-name").textContent = `${text(data.trainerName, names.charset)} ${data.trainerGender === "female" ? "♀" : "♂"}`;
    $("trainer-id").textContent = String(data.trainerId).padStart(5, "0");
    $("play-time").textContent = `${data.playTime.hours}:${String(data.playTime.minutes).padStart(2, "0")}`;
    $("dex").textContent = `${data.dexCaught}\u00a0caught · ${data.dexSeen}\u00a0seen`;
    $("badges").textContent = `${data.badges.filter(Boolean).length} / ${data.badges.length}`;
    $("release-version").textContent = data.releaseVersion;
    show("trainer");
}

function renderParty(data, names) {
    const template = $("mon-template");
    const sets = [];
    for (const mon of data.party) {
        const card = template.content.cloneNode(true);
        const species = names.species[mon.species] ?? { name: `Species ${mon.species}` };

        const nickname = mon.nickname ? text(mon.nickname, names.charset) : null;

        const sprite = spriteSpecies(mon, names);
        // Data exported before front sprites existed only has overworld sprites.
        card.querySelector(".sprite").src = (mon.shiny ? sprite.shinyFront ?? sprite.shinySprite : sprite.front ?? sprite.sprite) ?? "";
        card.querySelector(".nickname").textContent = nickname ?? species.name;
        card.querySelector(".gender").textContent = { male: "♂", female: "♀", genderless: "" }[mon.gender];
        card.querySelector(".gender").classList.add(mon.gender);
        card.querySelector(".level").textContent = `Lv. ${mon.level}`;
        card.querySelector(".species").textContent = mon.nickname ? species.name : "";
        card.querySelector(".shiny").hidden = !mon.shiny;
        const set = {
            nickname,
            species: species.name,
            gender: mon.gender,
            item: mon.item ? names.items[mon.item] ?? "?" : null,
            ability: names.abilities[mon.ability] ?? "?",
            level: mon.level,
            shiny: mon.shiny,
            nature: names.natures[mon.nature] ?? "?",
            moves: mon.moves.filter(Boolean).map((move) => names.moves[move] ?? `Move ${move}`),
        };
        card.querySelector(".set").textContent = formatSet(set);
        sets.push(set);
        // Cards go before the buttons so the buttons stay at the bottom.
        $("party-buttons").before(card);
    }

    $("copy-team").addEventListener("click", async () => {
        const button = $("copy-team");
        try {
            await navigator.clipboard.writeText(formatTeam(sets));
            button.textContent = "Copied!";
        } catch {
            button.textContent = "Copy failed";
        }
        setTimeout(() => (button.textContent = "Copy team as Pokepaste"), 2000);
    });
    show("party");
}

function renderDebug(data) {
    const rows = $("debug-rows");
    const add = (name, value) => {
        const tr = document.createElement("tr");
        for (const cell of [name, value]) {
            const td = document.createElement("td");
            td.textContent = cell;
            tr.append(td);
        }
        rows.append(tr);
    };
    add("Data version", data.dataVersion);
    add("Background", data.background);
    if (!data.hasDebugData)
        add("Flags and vars", "not included in this code");
    for (const [name, value] of Object.entries(data.flags))
        add(name, value ? "SET" : "clear");
    for (const [name, value] of Object.entries(data.vars))
        add(name, value);
    show("debug");
}

async function renderShare(data, names, backgrounds) {
    const background = backgrounds.find((b) => b.id === data.background) ?? backgrounds[0];
    const canvas = $("scene");
    await drawScene(canvas, background, sceneSprites(data, names, background));
    show("share");

    const fileName = `${text(data.trainerName, names.charset) || "trainer"}.png`;
    $("download").addEventListener("click", () => {
        const link = document.createElement("a");
        link.download = fileName;
        link.href = canvas.toDataURL("image/png");
        link.click();
    });

    const blob = await new Promise((resolve) => canvas.toBlob(resolve, "image/png"));
    const file = new File([blob], fileName, { type: "image/png" });
    if (navigator.canShare?.({ files: [file] })) {
        show("share-button");
        $("share-button").addEventListener("click", () => navigator.share({ files: [file] }).catch(() => {}));
    }
}

async function main() {
    const payload = payloadFromPath(location.pathname);
    if (!payload) {
        setStatus("Open this page by scanning a code from the game.");
        return;
    }

    let version;
    try {
        version = readDataVersion(payload);
    } catch {
        setStatus("This code looks damaged. Try scanning it again.");
        return;
    }

    let layout, names, backgrounds;
    try {
        [layout, names, backgrounds] = await Promise.all(
            ["layout", "names", "backgrounds"].map((file) => fetchJson(`data/v${version}/${file}.json`)));
    } catch {
        setStatus(`This code is from a game version this site doesn't know about yet (data version ${version}).`);
        return;
    }

    let data;
    try {
        data = decodePayload(payload, layout);
    } catch {
        setStatus("This code looks damaged. Try scanning it again.");
        return;
    }

    setStatus("");
    renderTrainer(data, names);
    renderParty(data, names);
    if (new URLSearchParams(location.search).has("debug"))
        renderDebug(data);
    await renderShare(data, names, backgrounds).catch((error) => console.error(error));
}

main();
