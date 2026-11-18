#ifndef PALDO_RAID_H
#define PALDO_RAID_H
/* 짧은 토벌 수련: a three-minute fight on Solbaramjae against growing boar
 * packs, temporary stacking augments chosen as 기운 fills, and an elite boar.
 * The run is a scratch copy of the game: raid_enter keeps the real state
 * aside, raid_leave restores it and adds only the result reward (문 and
 * 정혼주 파편, fields that already exist in the save). Nothing here is saved. */
#include "game.h"

#define RAID_SECONDS 180.0f
#define RAID_ELITE_AT 150.0f
#define RAID_ELITE_HP 160
#define RAID_KILL_COINS 3
#define RAID_WIN_COINS 30
#define RAID_WIN_SHARDS 3
#define RAID_FIRST_ENERGY 4
#define RAID_ENERGY_STEP 2

typedef enum { RAUG_STEEL, RAUG_WIND, RAUG_FLOW, RAUG_DRAIN, RAUG_FOCUS, RAUG_STEP } RaidAugment;
typedef struct {
    const char *name, *description;
} RaidAugmentInfo;
extern const RaidAugmentInfo RAID_AUGMENTS[RAID_AUG_COUNT];

/* Offered by the village elder once both traces on the pass are inspected. */
bool raid_available(const Game *g);
bool raid_offer(Game *g);
void raid_decline(Game *g);
bool raid_enter(Game *g, Game *saved);
/* Called by game_tick while a run is in play. */
void raid_tick(Game *g, float dt);
void raid_on_kill(Game *g, Enemy *e);
bool raid_choose(Game *g, int offer_index);
/* From the pause menu: ends the run as a failure (half the 문 earned). */
bool raid_abandon(Game *g);
/* The hero fell during a run: result screen instead of the death screen. */
void raid_hero_down(Game *g);
int raid_reward_coins(const Raid *r);
int raid_reward_shards(const Raid *r);
/* Result screen → back to the saved game plus the reward. */
bool raid_leave(Game *g, const Game *saved);
/* Back to the saved game with no reward (quitting to the title). */
bool raid_cancel(Game *g, const Game *saved);
bool raid_is_elite(const Game *g, const Enemy *e);

/* Temporary augment effects; all neutral outside a run. */
int raid_basic_bonus(const Game *g);
float raid_range_bonus(const Game *g);
int raid_wind_damage(const Game *g);
int raid_technique_bonus(const Game *g);
float raid_cooldown(const Game *g, float cooldown);
float raid_dodge_recharge(const Game *g, float seconds);
#endif
