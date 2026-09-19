// Run: node --test tools/qr_share/test/*.test.mjs
import { test } from "node:test";
import assert from "node:assert/strict";
import { formatSet, formatTeam } from "../web/pokepaste.js";

const PIP = {
    nickname: "Pip",
    species: "Pikachu",
    gender: "male",
    item: "Light Ball",
    ability: "Static",
    level: 42,
    shiny: true,
    nature: "Jolly",
    moves: ["Thunderbolt", "Quick Attack", "Iron Tail", "Protect"],
};

test("a full set matches the Pokepaste layout", () => {
    assert.equal(formatSet(PIP), [
        "Pip (Pikachu) (M) @ Light Ball",
        "Ability: Static",
        "Level: 42",
        "Shiny: Yes",
        "Jolly Nature",
        "- Thunderbolt",
        "- Quick Attack",
        "- Iron Tail",
        "- Protect",
    ].join("\n"));
});

test("optional parts are left out when they don't apply", () => {
    const set = { ...PIP, nickname: null, gender: "genderless", item: null, level: 100, shiny: false, moves: ["Tackle"] };
    assert.equal(formatSet(set), "Pikachu\nAbility: Static\nJolly Nature\n- Tackle");
    assert.equal(formatSet({ ...set, nickname: "Pikachu", gender: "female" }).split("\n")[0], "Pikachu (F)");
});

test("a team separates sets with a blank line", () => {
    assert.equal(formatTeam([PIP, PIP]), `${formatSet(PIP)}\n\n${formatSet(PIP)}`);
});
