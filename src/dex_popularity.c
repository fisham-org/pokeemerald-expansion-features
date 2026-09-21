#include "global.h"
#include "data.h"
#include "dex_minigame.h"
#include "malloc.h"
#include "pokemon.h"
#include "wild_encounter.h"
#include "constants/difficulty.h"
#include "constants/maps.h"
#include "data/dex_popularity.h"

// How well known each species is, blending a real-world survey with how often it
// turns up in this game (trainer parties and wild encounters). Used for PokeDoku's
// rarity score: a pick's share of the popularity of every valid answer.

STATIC_ASSERT(POKEDOKU_POPULARITY_SURVEY + POKEDOKU_POPULARITY_TRAINERS + POKEDOKU_POPULARITY_WILD == 100, PokedokuPopularityWeightsAddUpTo100);

#define SCORE_SCALE 1000

// Chance of each encounter slot, in percent. Mirrors the ENCOUNTER_CHANCE_* values in
// src/data/wild_encounters.h, which is only includable from wild_encounter.c.
static const u8 sLandSlotChances[NUM_LAND_MONS_ENCOUNTER_SLOTS]         = {20, 20, 10, 10, 10, 10, 5, 5, 4, 4, 1, 1};
static const u8 sWaterSlotChances[NUM_WATER_MONS_ENCOUNTER_SLOTS]       = {60, 30, 5, 4, 1};
static const u8 sRockSmashSlotChances[NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS] = {60, 30, 5, 4, 1};
static const u8 sFishingSlotChances[NUM_FISHING_MONS_ENCOUNTER_SLOTS]   = {70, 30, 60, 20, 20, 40, 40, 15, 4, 1};

// Counts each trainer once per species, across every trainer (and trainer party pool) in the game
static void CountTrainerUsage(u32 *counts)
{
    u16 *lastTrainer = AllocZeroed(sizeof(u16) * (NATIONAL_DEX_COUNT + 1));
    u32 trainerId, i;

    if (lastTrainer == NULL)
        return;

    for (trainerId = 0; trainerId < TRAINERS_COUNT; trainerId++)
    {
        const struct Trainer *trainer = &gTrainers[DIFFICULTY_NORMAL][trainerId];
        u32 partyCount = max(trainer->partySize, trainer->poolSize);

        if (trainer->party == NULL)
            continue;
        for (i = 0; i < partyCount; i++)
        {
            u32 dex = gSpeciesInfo[trainer->party[i].species].natDexNum;
            if (dex != 0 && dex <= NATIONAL_DEX_COUNT && lastTrainer[dex] != trainerId + 1)
            {
                lastTrainer[dex] = trainerId + 1;
                counts[dex]++;
            }
        }
    }
    Free(lastTrainer);
}

static void AddEncounterTable(u32 *presence, const struct WildPokemonInfo *info, const u8 *slotChances, u32 slotCount)
{
    u32 i;

    if (info == NULL || info->wildPokemon == NULL)
        return;
    for (i = 0; i < slotCount; i++)
    {
        u32 dex = gSpeciesInfo[info->wildPokemon[i].species].natDexNum;
        if (dex != 0 && dex <= NATIONAL_DEX_COUNT)
            presence[dex] += slotChances[i];
    }
}

// Adds up each species' slot chances across every map's encounter tables. A table shared
// by several times of day is only counted once.
static void CountWildPresence(u32 *presence)
{
    const struct WildPokemonHeader *header;
    u32 time, earlier;

    for (header = gWildMonHeaders; header->mapGroup != MAP_GROUP(MAP_UNDEFINED); header++)
    {
        for (time = 0; time < TIMES_OF_DAY_COUNT; time++)
        {
            const struct WildEncounterTypes *types = &header->encounterTypes[time];
            bool32 land = TRUE, water = TRUE, rock = TRUE, fish = TRUE;

            for (earlier = 0; earlier < time; earlier++)
            {
                const struct WildEncounterTypes *prev = &header->encounterTypes[earlier];
                land &= (types->landMonsInfo != prev->landMonsInfo);
                water &= (types->waterMonsInfo != prev->waterMonsInfo);
                rock &= (types->rockSmashMonsInfo != prev->rockSmashMonsInfo);
                fish &= (types->fishingMonsInfo != prev->fishingMonsInfo);
            }
            if (land)
                AddEncounterTable(presence, types->landMonsInfo, sLandSlotChances, NUM_LAND_MONS_ENCOUNTER_SLOTS);
            if (water)
                AddEncounterTable(presence, types->waterMonsInfo, sWaterSlotChances, NUM_WATER_MONS_ENCOUNTER_SLOTS);
            if (rock)
                AddEncounterTable(presence, types->rockSmashMonsInfo, sRockSmashSlotChances, NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS);
            if (fish)
                AddEncounterTable(presence, types->fishingMonsInfo, sFishingSlotChances, NUM_FISHING_MONS_ENCOUNTER_SLOTS);
        }
    }
}

// A gentle (square root) curve, scaled so the most popular species scores SCORE_SCALE + 1
// and a species with no votes/trainers/encounters still scores 1.
static u32 GetCurvedScore(u32 value, u32 maxValue)
{
    if (maxValue == 0)
        return 1;
    return 1 + (SCORE_SCALE * Sqrt(value << 8)) / Sqrt(maxValue << 8);
}

static u32 GetMax(const u32 *values)
{
    u32 dex, result = 0;

    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        if (values[dex] > result)
            result = values[dex];
    }
    return result;
}

void DexPool_LoadPopularity(struct DexPool *pool)
{
    u32 *votes = AllocZeroed(sizeof(u32) * (NATIONAL_DEX_COUNT + 1));
    u32 *trainers = AllocZeroed(sizeof(u32) * (NATIONAL_DEX_COUNT + 1));
    u32 *wild = AllocZeroed(sizeof(u32) * (NATIONAL_DEX_COUNT + 1));
    u32 dex, maxVotes, maxTrainers, maxWild;

    if (votes != NULL && trainers != NULL && wild != NULL)
    {
        for (dex = 1; dex <= NATIONAL_DEX_COUNT && dex < ARRAY_COUNT(sDexSurveyVotes); dex++)
            votes[dex] = sDexSurveyVotes[dex];
        if (POKEDOKU_POPULARITY_TRAINERS > 0)
            CountTrainerUsage(trainers);
        if (POKEDOKU_POPULARITY_WILD > 0)
            CountWildPresence(wild);

        maxVotes = GetMax(votes);
        maxTrainers = GetMax(trainers);
        maxWild = GetMax(wild);
        for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
        {
            if (pool->species[dex] == SPECIES_NONE)
                continue;
            pool->popularity[dex] = (POKEDOKU_POPULARITY_SURVEY * GetCurvedScore(votes[dex], maxVotes)
                                   + POKEDOKU_POPULARITY_TRAINERS * GetCurvedScore(trainers[dex], maxTrainers)
                                   + POKEDOKU_POPULARITY_WILD * GetCurvedScore(wild[dex], maxWild)) / 100;
        }
    }

    TRY_FREE_AND_SET_NULL(votes);
    TRY_FREE_AND_SET_NULL(trainers);
    TRY_FREE_AND_SET_NULL(wild);
}
