// Decodes a QR Share payload. Mirrors the writer in src/qr_share.c; the layout
// (field widths, flag and var lists) comes from data/v<N>/layout.json.

const BASE32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
const GENDERS = ["male", "female", "genderless"];
// Must match sVersionAlphabet in src/qr_share.c.
const VERSION_ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-+ ";

class BitReader {
    constructor(payload) {
        this.bits = [];
        for (const ch of payload) {
            const value = BASE32.indexOf(ch);
            if (value < 0)
                throw new Error(`Invalid character '${ch}' in code`);
            for (let i = 4; i >= 0; i--)
                this.bits.push((value >> i) & 1);
        }
        this.pos = 0;
    }

    read(count) {
        if (this.pos + count > this.bits.length)
            throw new Error("Code is too short");
        let value = 0;
        for (let i = 0; i < count; i++)
            value = value * 2 + this.bits[this.pos++];
        return value;
    }

    readBytes(lengthBits) {
        const length = this.read(lengthBits);
        const bytes = [];
        for (let i = 0; i < length; i++)
            bytes.push(this.read(8));
        return bytes;
    }
}

// Either MAJOR.MINOR.PATCH or the custom QR_SHARE_VERSION_STRING.
function readReleaseVersion(r) {
    if (!r.read(1))
        return `${r.read(8)}.${r.read(8)}.${r.read(8)}`;
    const length = r.read(4);
    let version = "";
    for (let i = 0; i < length; i++)
        version += VERSION_ALPHABET[r.read(6)] ?? "?";
    return version;
}

// Returns the payload part of a URL path such as "/P/AF4PO...", or null.
export function payloadFromPath(path) {
    const match = /\/P\/([A-Z2-7]+)\/?$/i.exec(path);
    return match ? match[1].toUpperCase() : null;
}

// The data version is the first 8 bits; used to pick which layout to load.
export function readDataVersion(payload) {
    return new BitReader(payload.slice(0, 2)).read(8);
}

// Names are returned as raw GBA charset bytes; see names.json "charset" to convert them.
export function decodePayload(payload, layout) {
    const r = new BitReader(payload);
    const bits = layout.bits;

    const data = {
        dataVersion: r.read(8),
        releaseVersion: readReleaseVersion(r),
        trainerName: r.readBytes(3),
        trainerGender: r.read(1) ? "female" : "male",
        trainerId: r.read(16),
        badges: [],
        dexSeen: 0,
        dexCaught: 0,
        playTime: { hours: 0, minutes: 0 },
        background: 0,
        party: [],
        hasDebugData: false,
        flags: {},
        vars: {},
    };
    const badgeBits = r.read(8);
    for (let i = 0; i < 8; i++)
        data.badges.push(Boolean(badgeBits & (1 << i)));
    data.dexSeen = r.read(bits.dexCount);
    data.dexCaught = r.read(bits.dexCount);
    data.playTime.hours = r.read(10);
    data.playTime.minutes = r.read(6);
    data.background = r.read(5);
    const partyCount = r.read(3);

    for (let i = 0; i < partyCount; i++) {
        const mon = {
            species: r.read(bits.species),
            level: r.read(7),
            item: r.read(bits.item),
            ability: r.read(bits.ability),
            moves: [],
        };
        for (let m = 0; m < 4; m++)
            mon.moves.push(r.read(bits.move));
        mon.nature = r.read(5);
        mon.shiny = Boolean(r.read(1));
        mon.gender = GENDERS[r.read(2)];
        mon.nickname = r.read(1) ? r.readBytes(4) : null;
        data.party.push(mon);
    }

    data.hasDebugData = Boolean(r.read(1));
    if (data.hasDebugData) {
        for (const flag of layout.flags)
            data.flags[flag] = Boolean(r.read(1));
        for (const v of layout.vars)
            data.vars[v.name] = r.read(v.bits);
    }

    return data;
}
