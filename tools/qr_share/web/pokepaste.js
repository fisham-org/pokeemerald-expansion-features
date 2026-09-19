// Formats a Pokemon as a set in Pokepaste / Showdown export format
// (https://pokepast.es/syntax.html). Fields are already-resolved display strings.

const GENDER_TAGS = { male: " (M)", female: " (F)", genderless: "" };

export function formatSet(set) {
    let head = set.nickname && set.nickname !== set.species ? `${set.nickname} (${set.species})` : set.species;
    head += GENDER_TAGS[set.gender] ?? "";
    if (set.item)
        head += ` @ ${set.item}`;

    const lines = [head, `Ability: ${set.ability}`];
    // Showdown assumes Level 100 when the line is missing.
    if (set.level !== 100)
        lines.push(`Level: ${set.level}`);
    if (set.shiny)
        lines.push("Shiny: Yes");
    lines.push(`${set.nature} Nature`);
    for (const move of set.moves)
        lines.push(`- ${move}`);
    return lines.join("\n");
}

// Sets are separated by a blank line.
export function formatTeam(sets) {
    return sets.map(formatSet).join("\n\n");
}
