#!/usr/bin/env python3
"""
Generates quest constants and data tables from src/data/quests/*.json.

Quest ids come from src/data/quests/order.json (position = id = save slot).
Stage ids are the array position inside each quest file.

Outputs:
  include/constants/quests.h  QUEST_*, STAGE_*, OUTCOME_*, QUEST_COUNT
  src/data/quests.h           const quest tables and strings
"""

import glob
import json
import os
import re
import sys

DATA_DIR = "src/data/quests"
ORDER_FILE = os.path.join(DATA_DIR, "order.json")
TYPES_HEADER = "include/constants/quest_types.h"
CONSTANTS_OUT = "include/constants/quests.h"
DATA_OUT = "src/data/quests.h"

CATEGORIES = ("STORY", "POKEMON", "SIDE")
ICON_TYPES = ("OBJECT", "ITEM", "PKMN")
CONDITION_KEYS = ("flag", "var", "item", "party", "dex")
VAR_OPS = ("ge", "eq", "lt")
PARTY_KEYS = ("species", "type", "ability", "nature")
DEX_STATES = ("seen", "caught")

QUEST_KEYS = {"id", "category", "name", "icon", "giver", "lead", "summary", "rewards", "stages", "outcomes"}
STAGE_KEYS = {"id", "title", "journal", "objectives", "turn_in"}
OBJECTIVE_KEYS = {"text", "target", "condition", "optional", "reveal"}
OUTCOME_KEYS = {"id", "summary", "closes"}

errors = []


def error(msg):
    errors.append(msg)


def read_define(header_text, name):
    m = re.search(r"#define\s+" + name + r"\s+(\d+)", header_text)
    if not m:
        sys.exit(f"quests_to_header.py: {name} not found in {TYPES_HEADER}")
    return int(m.group(1))


def c_string(text):
    text = text.replace('"', '\\"').replace("\n", "\\n")
    return f'COMPOUND_STRING("{text}")'


def c_string_or_null(text):
    return c_string(text) if text is not None else "NULL"


def check_keys(obj, allowed, where):
    for key in obj:
        if key not in allowed:
            error(f"{where}: unknown key '{key}'")


def map_ref(obj, where):
    """Returns (map, localid) C expressions for a {map, localid} object."""
    if obj is None:
        return "MAP_UNDEFINED", "LOCALID_NONE"
    if "map" not in obj:
        error(f"{where}: missing 'map'")
        return "MAP_UNDEFINED", "LOCALID_NONE"
    for key in obj:
        if key not in ("map", "localid"):
            error(f"{where}: unknown key '{key}'")
    return obj["map"], obj.get("localid", "LOCALID_NONE")


def condition(cond, where):
    """Returns a C initializer for struct QuestCondition."""
    if cond is None:
        return "{ .type = QUEST_COND_NONE }"
    keys = [k for k in cond if k in CONDITION_KEYS]
    if len(keys) != 1:
        error(f"{where}: condition needs exactly one of {', '.join(CONDITION_KEYS)}")
        return "{ .type = QUEST_COND_NONE }"
    kind = keys[0]

    if kind == "flag":
        check_keys(cond, {"flag"}, where)
        return f"{{ .type = QUEST_COND_FLAG, .id = {cond['flag']} }}"

    if kind == "var":
        check_keys(cond, {"var", "progress"} | set(VAR_OPS), where)
        ops = [op for op in VAR_OPS if op in cond]
        if len(ops) != 1:
            error(f"{where}: var condition needs exactly one of {', '.join(VAR_OPS)}")
            return "{ .type = QUEST_COND_NONE }"
        op = ops[0]
        progress = "TRUE" if cond.get("progress", False) else "FALSE"
        return (f"{{ .type = QUEST_COND_VAR, .op = QUEST_VAR_{op.upper()}, .id = {cond['var']}, "
                f".value = {cond[op]}, .progress = {progress} }}")

    if kind == "item":
        check_keys(cond, {"item", "count"}, where)
        return (f"{{ .type = QUEST_COND_ITEM, .id = {cond['item']}, "
                f".value = {cond.get('count', 1)}, .progress = TRUE }}")

    if kind == "party":
        check_keys(cond, {"party"}, where)
        party = cond["party"]
        if not isinstance(party, dict) or len(party) != 1 or next(iter(party)) not in PARTY_KEYS:
            error(f"{where}: party condition needs exactly one of {', '.join(PARTY_KEYS)}")
            return "{ .type = QUEST_COND_NONE }"
        key, value = next(iter(party.items()))
        return f"{{ .type = QUEST_COND_PARTY, .op = PARTY_COND_{key.upper()}, .id = {value} }}"

    # dex
    check_keys(cond, {"dex", "state"}, where)
    state = cond.get("state", "caught")
    if state not in DEX_STATES:
        error(f"{where}: unknown dex state '{state}'")
        state = "caught"
    return f"{{ .type = QUEST_COND_DEX, .op = QUEST_DEX_{state.upper()}, .id = {cond['dex']} }}"


def main():
    with open(TYPES_HEADER) as f:
        types_text = f.read()
    quest_max = read_define(types_text, "QUEST_MAX")
    max_objectives = read_define(types_text, "QUEST_MAX_OBJECTIVES")
    max_outcomes = read_define(types_text, "QUEST_MAX_OUTCOMES")

    with open(ORDER_FILE) as f:
        order = json.load(f)

    slots = []  # quest id or None (removed)
    for i, entry in enumerate(order):
        if isinstance(entry, str):
            slots.append(entry)
        elif isinstance(entry, dict) and "removed" in entry:
            slots.append(None)
        else:
            error(f"{ORDER_FILE}: entry {i} must be a QUEST_* string or {{ \"removed\": \"QUEST_*\" }}")
            slots.append(None)

    if len(slots) > quest_max:
        error(f"{ORDER_FILE}: {len(slots)} quests exceeds QUEST_MAX ({quest_max})")

    seen_order = set()
    for entry in order:
        name = entry if isinstance(entry, str) else entry.get("removed") if isinstance(entry, dict) else None
        if name in seen_order:
            error(f"{ORDER_FILE}: duplicate entry {name}")
        seen_order.add(name)

    quests = {}
    for path in sorted(glob.glob(os.path.join(DATA_DIR, "*.json"))):
        if os.path.abspath(path) == os.path.abspath(ORDER_FILE):
            continue
        with open(path, encoding="utf-8") as f:
            try:
                quest = json.load(f)
            except json.JSONDecodeError as e:
                error(f"{path}: invalid JSON: {e}")
                continue
        quest["_path"] = path
        qid = quest.get("id")
        if qid is None:
            error(f"{path}: missing 'id'")
            continue
        if qid in quests:
            error(f"{path}: duplicate quest id {qid} (also in {quests[qid]['_path']})")
            continue
        if qid not in slots:
            error(f"{path}: {qid} is not listed in {ORDER_FILE}")
        quests[qid] = quest

    for qid in slots:
        if qid is not None and qid not in quests:
            error(f"{ORDER_FILE}: {qid} has no quest file (mark it {{ \"removed\": \"{qid}\" }} if deleted)")

    # Shared id namespace check (QUEST_*, STAGE_*, OUTCOME_*)
    all_ids = {}
    for name in seen_order:
        if name:
            all_ids[name] = ORDER_FILE

    def claim(name, where):
        if not isinstance(name, str) or not re.fullmatch(r"[A-Z_][A-Z0-9_]*", name):
            error(f"{where}: invalid id {name!r}")
            return
        if name in all_ids:
            error(f"{where}: duplicate id {name} (also in {all_ids[name]})")
        else:
            all_ids[name] = where

    constants = []
    data = []
    quest_inits = []

    for qid in slots:
        if qid is None or qid not in quests:
            continue
        q = quests[qid]
        where = q["_path"]
        check_keys(q, QUEST_KEYS | {"_path"}, where)
        for req in ("category", "name", "icon", "summary", "stages", "outcomes"):
            if req not in q:
                error(f"{where}: missing '{req}'")
        if any(req not in q for req in ("category", "name", "icon", "summary", "stages", "outcomes")):
            continue

        if q["category"] not in CATEGORIES:
            error(f"{where}: unknown category '{q['category']}'")
        icon = q["icon"]
        if icon.get("type") not in ICON_TYPES or "value" not in icon:
            error(f"{where}: icon needs type ({', '.join(ICON_TYPES)}) and value")

        stages = q["stages"]
        if len(stages) == 0:
            error(f"{where}: needs at least one stage")
        outcomes = q["outcomes"]
        if len(outcomes) == 0:
            error(f"{where}: needs at least one outcome")
        if len(outcomes) > max_outcomes:
            error(f"{where}: {len(outcomes)} outcomes exceeds QUEST_MAX_OUTCOMES ({max_outcomes})")

        lead = q.get("lead")
        if lead is not None:
            check_keys(lead, {"hint", "map"}, f"{where}: lead")
            if "hint" not in lead:
                error(f"{where}: lead quest has no lead.hint")

        tag = qid.lower()
        constants.append(f"// {qid}")

        # Stages
        stage_inits = []
        for s_index, stage in enumerate(stages):
            s_where = f"{where}: stage {s_index}"
            if "removed" in stage:
                claim(stage["removed"], s_where)
                stage_inits.append(f"    [{s_index}] = {{ .title = NULL }}, // removed {stage['removed']}")
                continue
            check_keys(stage, STAGE_KEYS, s_where)
            for req in ("id", "title", "journal", "objectives"):
                if req not in stage:
                    error(f"{s_where}: missing '{req}'")
            if any(req not in stage for req in ("id", "title", "journal", "objectives")):
                continue
            claim(stage["id"], s_where)
            constants.append(f"#define {stage['id']} {s_index}")

            objectives = stage["objectives"]
            if len(objectives) > max_objectives:
                error(f"{s_where}: {len(objectives)} objectives exceeds QUEST_MAX_OBJECTIVES ({max_objectives})")

            objective_inits = []
            for o_index, obj in enumerate(objectives):
                o_where = f"{s_where} objective {o_index}"
                check_keys(obj, OBJECTIVE_KEYS, o_where)
                if "text" not in obj:
                    error(f"{o_where}: missing 'text'")
                    continue
                t_map, t_local = map_ref(obj.get("target"), f"{o_where} target")
                objective_inits.append(
                    "    {\n"
                    f"        .text = {c_string(obj['text'])},\n"
                    f"        .targetMap = {t_map},\n"
                    f"        .targetLocalId = {t_local},\n"
                    f"        .optional = {'TRUE' if obj.get('optional', False) else 'FALSE'},\n"
                    f"        .condition = {condition(obj.get('condition'), o_where)},\n"
                    f"        .reveal = {condition(obj.get('reveal'), o_where + ' reveal')},\n"
                    "    },")

            obj_array = f"sQuestObjectives_{tag}_{s_index}"
            if objective_inits:
                data.append(f"static const struct QuestObjective {obj_array}[] =\n{{\n" + "\n".join(objective_inits) + "\n};\n")
            ti_map, ti_local = map_ref(stage.get("turn_in"), f"{s_where} turn_in")
            stage_inits.append(
                f"    [{s_index}] =\n"
                "    {\n"
                f"        .title = {c_string(stage['title'])},\n"
                f"        .journal = {c_string(stage['journal'])},\n"
                f"        .objectives = {obj_array if objective_inits else 'NULL'},\n"
                f"        .objectiveCount = {len(objective_inits)},\n"
                f"        .turnInMap = {ti_map},\n"
                f"        .turnInLocalId = {ti_local},\n"
                "    },")

        data.append(f"static const struct QuestStage sQuestStages_{tag}[] =\n{{\n" + "\n".join(stage_inits) + "\n};\n")

        # Outcomes
        outcome_inits = []
        for o_index, outcome in enumerate(outcomes):
            o_where = f"{where}: outcome {o_index}"
            check_keys(outcome, OUTCOME_KEYS, o_where)
            if "id" not in outcome or "summary" not in outcome:
                error(f"{o_where}: needs 'id' and 'summary'")
                continue
            claim(outcome["id"], o_where)
            constants.append(f"#define {outcome['id']} {o_index}")
            closes = outcome.get("closes", [])
            for closed in closes:
                if closed not in slots:
                    error(f"{o_where}: closes unknown quest {closed}")
            closes_array = "NULL"
            if closes:
                closes_array = f"sQuestCloses_{tag}_{o_index}"
                data.append(f"static const u8 {closes_array}[] = {{ {', '.join(closes)} }};\n")
            outcome_inits.append(
                f"    [{o_index}] = {{ .summary = {c_string(outcome['summary'])}, "
                f".closes = {closes_array}, .closesCount = {len(closes)} }},")
        data.append(f"static const struct QuestOutcome sQuestOutcomes_{tag}[] =\n{{\n" + "\n".join(outcome_inits) + "\n};\n")

        # Rewards
        rewards = q.get("rewards", [])
        reward_inits = []
        for r_index, reward in enumerate(rewards):
            r_where = f"{where}: reward {r_index}"
            if "item" in reward:
                check_keys(reward, {"item", "count"}, r_where)
                reward_inits.append(f"    {{ .type = QUEST_REWARD_ITEM, .item = {reward['item']}, .amount = {reward.get('count', 1)} }},")
            elif "money" in reward:
                check_keys(reward, {"money"}, r_where)
                reward_inits.append(f"    {{ .type = QUEST_REWARD_MONEY, .amount = {reward['money']} }},")
            else:
                error(f"{r_where}: needs 'item' or 'money'")
        rewards_array = "NULL"
        if reward_inits:
            rewards_array = f"sQuestRewards_{tag}"
            data.append(f"static const struct QuestReward {rewards_array}[] =\n{{\n" + "\n".join(reward_inits) + "\n};\n")

        giver_map, giver_local = map_ref(q.get("giver"), f"{where}: giver")
        lead_map = lead.get("map", "MAP_UNDEFINED") if lead else "MAP_UNDEFINED"
        quest_inits.append(
            f"    [{qid}] =\n"
            "    {\n"
            f"        .name = {c_string(q['name'])},\n"
            f"        .summary = {c_string(q['summary'])},\n"
            f"        .leadHint = {c_string_or_null(lead.get('hint') if lead else None)},\n"
            f"        .category = QUEST_CATEGORY_{q['category']},\n"
            f"        .iconType = QUEST_ICON_{icon.get('type', 'OBJECT')},\n"
            f"        .icon = {icon.get('value', 0)},\n"
            f"        .giverMap = {giver_map},\n"
            f"        .giverLocalId = {giver_local},\n"
            f"        .leadMap = {lead_map},\n"
            f"        .rewards = {rewards_array},\n"
            f"        .stages = sQuestStages_{tag},\n"
            f"        .outcomes = sQuestOutcomes_{tag},\n"
            f"        .rewardCount = {len(reward_inits)},\n"
            f"        .stageCount = {len(stages)},\n"
            f"        .outcomeCount = {len(outcome_inits)},\n"
            "    },")
        constants.append("")

    if errors:
        for e in errors:
            print(f"quests_to_header.py: error: {e}", file=sys.stderr)
        sys.exit(1)

    header = "//\n// DO NOT MODIFY THIS FILE! It is auto-generated by tools/quests/quests_to_header.py\n//\n\n"

    with open(CONSTANTS_OUT, "w", encoding="utf-8") as f:
        f.write("#ifndef GUARD_CONSTANTS_QUESTS_H\n#define GUARD_CONSTANTS_QUESTS_H\n\n")
        f.write(header)
        f.write('#include "constants/quest_types.h"\n\n')
        for slot, name in enumerate(order):
            if isinstance(name, str):
                f.write(f"#define {name} {slot}\n")
            else:
                f.write(f"// slot {slot}: removed {name.get('removed')}\n")
        f.write(f"\n#define QUEST_COUNT {len(slots)}\n\n")
        f.write("\n".join(constants))
        f.write("\n#endif // GUARD_CONSTANTS_QUESTS_H\n")

    with open(DATA_OUT, "w", encoding="utf-8") as f:
        f.write(header)
        f.write("\n".join(data))
        f.write("\nconst struct Quest gQuests[QUEST_COUNT] =\n{\n")
        f.write("\n".join(quest_inits))
        f.write("\n};\n")


if __name__ == "__main__":
    main()
