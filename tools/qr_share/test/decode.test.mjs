// Run: node --test tools/qr_share/test/*.test.mjs
// GOLDEN_URL comes from test/qr_share.c (sGoldenUrl); the expected values below are the
// state SetupQrShareState() creates there, so a pass means the C writer and JS reader agree.
import { test } from "node:test";
import assert from "node:assert/strict";
import { decodePayload, payloadFromPath, readDataVersion } from "../web/decode.js";

const GOLDEN_URL = "HTTPS://MYGAME.PAGES.DEV/P/AEAIAAB4PO6TTAOIGABQAQDEIFB7A65QIEAEI4MIAAK24NP53YDJSBYOAAGUEEFUAAAAAV27O6";

// Widths match the current constants (NUM_SPECIES 1573, ITEMS_COUNT 874, MOVES_COUNT 848,
// ABILITIES_COUNT 319, NATIONAL_DEX_COUNT 1025); tables match the TESTING block in src/data/qr_share.h.
const LAYOUT = {
    bits: { species: 11, item: 10, move: 10, ability: 9, dexCount: 11 },
    flags: ["FLAG_TEMP_1", "FLAG_TEMP_2", "FLAG_TEMP_3"],
    vars: [{ name: "VAR_TEMP_0", bits: 2 }, { name: "VAR_TEMP_1", bits: 16 }],
};

// GBA charset: 'A' = 0xBB ... 'Z' = 0xD4.
const gba = (s) => [...s].map((c) => 0xBB + c.charCodeAt(0) - 65);

test("golden URL decodes to the state built in test/qr_share.c", () => {
    const payload = payloadFromPath(new URL(GOLDEN_URL).pathname);
    assert.equal(readDataVersion(payload), 1);

    assert.deepEqual(decodePayload(payload, LAYOUT), {
        dataVersion: 1,
        releaseVersion: "1.0.0",
        trainerName: gba("MAY"),
        trainerGender: "female",
        trainerId: 12345,
        badges: [false, true, true, false, false, false, false, false],
        dexSeen: 3,
        dexCaught: 2,
        playTime: { hours: 12, minutes: 34 },
        background: 1,
        party: [
            { species: 252, level: 15, item: 472, ability: 65, moves: [1, 71, 98, 0],
              nature: 2, shiny: true, gender: "female", nickname: gba("LEAFY") },
            { species: 263, level: 7, item: 0, ability: 53, moves: [33, 45, 0, 0],
              nature: 0, shiny: false, gender: "female", nickname: null },
        ],
        hasDebugData: true,
        flags: { FLAG_TEMP_1: false, FLAG_TEMP_2: true, FLAG_TEMP_3: true },
        vars: { VAR_TEMP_0: 2, VAR_TEMP_1: 0xBEEF },
    });
});

test("rejects characters outside base32", () => {
    assert.throws(() => decodePayload("AF4P0", LAYOUT), /Invalid character/);
});

test("rejects truncated codes", () => {
    assert.throws(() => decodePayload("AF4PO6", LAYOUT), /too short/);
});
