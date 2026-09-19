// Run: node --test tools/qr_share/test/*.test.mjs
import { test } from "node:test";
import assert from "node:assert/strict";
import { spriteSpecies } from "../web/render.js";

const NAMES = {
    species: [
        { name: "?" },
        { name: "Charizard", mega: [[10, 2], [11, 3]] },
        { name: "Charizard Mega X" },
        { name: "Charizard Mega Y" },
    ],
};

test("a Pokemon holding its Mega Stone uses the Mega form's sprites", () => {
    assert.equal(spriteSpecies({ species: 1, item: 10 }, NAMES).name, "Charizard Mega X");
    assert.equal(spriteSpecies({ species: 1, item: 11 }, NAMES).name, "Charizard Mega Y");
});

test("any other item, or data without Mega forms, uses the species itself", () => {
    assert.equal(spriteSpecies({ species: 1, item: 0 }, NAMES).name, "Charizard");
    assert.equal(spriteSpecies({ species: 2, item: 10 }, NAMES).name, "Charizard Mega X");
    assert.equal(spriteSpecies({ species: 1, item: 10 }, { species: [{ name: "?" }, { name: "Charizard" }] }).name, "Charizard");
    assert.equal(spriteSpecies({ species: 99, item: 0 }, NAMES).name, "?");
});
