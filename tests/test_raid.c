/* 짧은 토벌 수련: offer, isolated run, packs, picks, elite, result, reward. */
#include "raid.h"
#include "save.h"
#include "world.h"
#include "test_simulation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL raid:%d: %s\n", __LINE__, #c); exit(1); } } while (0)

/* Quest accepted, both traces read, back in the village beside the elder. */
static void ready(Game *g) {
    game_reset(g); game_start(g);
    g->hero.pos = NPCS[ELDER].pos; CHECK(game_accept(g));
    g->hero.pos = MAP_EXITS[0].pos; CHECK(game_travel(g, 0));
    for (int i = 0; i < TRACE_COUNT; i++) { g->hero.pos = TRACES[i].pos; CHECK(game_inspect(g, i)); }
    g->hero.pos = MAP_EXITS[2].pos; CHECK(game_travel(g, 2));
    g->hero.pos = NPCS[ELDER].pos;
}
static int alive(const Game *g) {
    int n = 0;
    for (int i = 0; i < HUNT_COUNT; i++) n += g->hunt[i].hp > 0;
    return n;
}
static void run(Game *g, float seconds);
static void kill_one(Game *g) {
    /* Wait (up to a few spawn intervals) for an ordinary boar to be out. */
    for (int n = 0; n < 200 && g->phase == PH_PLAY; n++) {
        bool any = false;
        for (int i = 0; i < HUNT_COUNT; i++) any |= g->hunt[i].hp > 0 && !raid_is_elite(g, &g->hunt[i]);
        if (any) break;
        run(g, .05f);
    }
    for (int i = 0; i < HUNT_COUNT; i++)
        if (g->hunt[i].hp > 0 && !raid_is_elite(g, &g->hunt[i])) {
            g->hero.pos = g->hunt[i].pos;
            CHECK(game_damage(g, &g->hunt[i], 999, FX_HIT));
            g->hunt[i].dying = 0;
            return;
        }
    CHECK(false);
}
/* Keep the hero alive and still; packs spawn and chase on their own. */
static void run(Game *g, float seconds) {
    for (float t = 0; t < seconds && g->phase == PH_PLAY; t += .05f) {
        g->hero.hp = g->hero.max_hp; g->hero.invul = 1;
        simulation_tick(g, .05f, (Vec2){0});
    }
}

static void offer_and_isolation(void) {
    Game g, saved;
    /* Locked before both traces; never in training or away from the village. */
    game_reset(&g); game_start(&g); CHECK(!raid_available(&g) && !raid_offer(&g));
    ready(&g); CHECK(raid_available(&g));
    g.coins = 40; g.spirit_shards = 2; g.materials[0] = 5; int herbs = g.herbs;
    Game before = g;
    CHECK(raid_offer(&g) && g.phase == PH_RAID && g.raid.stage == RAID_OFFER && !g.raid.active);
    raid_decline(&g); CHECK(g.phase == PH_PLAY);
    CHECK(raid_offer(&g)); CHECK(raid_enter(&g, &saved));
    CHECK(g.raid.active && g.phase == PH_PLAY && g.map == MAP_PASS && game_hunt_active(&g));
    CHECK(g.hero.hp == g.hero.max_hp && alive(&g) == 0);
    for (int i = 0; i < BOAR_COUNT; i++) CHECK(g.boars[i].hp == 0);
    unsigned char bytes[SAVE_CAPACITY]; size_t size = 0;
    CHECK(!raid_enter(&g, &saved) && !raid_offer(&g));
    /* Kills in the run pay nothing directly and drop no materials or 정혼주. */
    run(&g, 4); CHECK(alive(&g) >= 1);
    kill_one(&g);
    CHECK(g.raid.kills == 1 && g.raid.coins == RAID_KILL_COINS);
    CHECK(g.coins == before.coins && g.materials[0] == 5 && g.spirit_shards == 2);
    for (int i = 0; i < MATERIAL_DROP_MAX; i++) CHECK(g.material_drops[i].quantity == 0);
    /* Quitting mid-run (to the title) restores the game with no reward. */
    CHECK(raid_cancel(&g, &saved));
    CHECK(!g.raid.active && g.phase == PH_PLAY && g.map == MAP_VILLAGE && g.coins == 40 &&
          g.spirit_shards == 2 && g.herbs == herbs && g.quest == before.quest);
    CHECK(save_encode(&g, bytes, &size) == SAVE_OK && size == SAVE_V6_SIZE);
}

static void packs_and_picks(void) {
    Game g, saved; ready(&g); CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    /* Packs grow over time but never past the six ordinary slots. */
    run(&g, 3); int early = alive(&g); CHECK(early >= 1 && early <= 2);
    int most = 0;
    for (int n = 0; n < 40 && g.phase == PH_PLAY; n++) {
        run(&g, 3); if (alive(&g) > most) most = alive(&g);
        CHECK(alive(&g) <= HUNT_COUNT && alive(&g) <= 2 + (int)(g.raid.time / 36));
    }
    CHECK(most >= 3);
    /* Boars spawned out of reach walk in to the hero instead of standing still. */
    ready(&g); CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    run(&g, 3); CHECK(alive(&g) >= 1);
    float start = 1e9f;
    for (int i = 0; i < HUNT_COUNT; i++) if (g.hunt[i].hp > 0) start = fminf(start, distance(g.hunt[i].pos, g.hero.pos));
    CHECK(start >= 150);
    run(&g, 3);
    float later = 1e9f;
    for (int i = 0; i < HUNT_COUNT; i++) if (g.hunt[i].hp > 0) later = fminf(later, distance(g.hunt[i].pos, g.hero.pos));
    CHECK(later < start - 100);
    /* The 기운 bar opens a pick of three different offers. */
    ready(&g); CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    for (int k = 0; k < RAID_FIRST_ENERGY; k++) { run(&g, 4); if (g.phase == PH_PLAY) kill_one(&g); }
    CHECK(g.phase == PH_RAID && g.raid.stage == RAID_PICK && g.raid.offer_count == 3 && g.raid.level == 1);
    CHECK(g.raid.offer[0] != g.raid.offer[1] && g.raid.offer[1] != g.raid.offer[2] && g.raid.offer[0] != g.raid.offer[2]);
    float before = g.raid.time; game_tick(&g, .05f, (Vec2){0}); CHECK(g.raid.time == before); /* paused */
    CHECK(!raid_choose(&g, 3) && !raid_choose(&g, -1));
    int pick = g.raid.offer[0];
    CHECK(raid_choose(&g, 0) && g.phase == PH_PLAY && g.raid.stacks[pick] == 1);
    CHECK(g.raid.energy == 0 && g.raid.energy_need == RAID_FIRST_ENERGY + RAID_ENERGY_STEP);
    /* Stacks cap at three; a maxed augment is never offered again. */
    memset(g.raid.stacks, 0, sizeof g.raid.stacks);
    for (int i = 0; i < RAID_AUG_COUNT - 1; i++) g.raid.stacks[i] = RAID_AUG_MAX_STACK;
    g.raid.energy = g.raid.energy_need - 1; run(&g, 3); kill_one(&g);
    CHECK(g.phase == PH_RAID && g.raid.offer_count == 1 && g.raid.offer[0] == RAID_AUG_COUNT - 1);
    CHECK(raid_choose(&g, 0));
    for (int i = 0; i < RAID_AUG_COUNT; i++) g.raid.stacks[i] = RAID_AUG_MAX_STACK;
    g.raid.energy = g.raid.energy_need - 1; run(&g, 3); kill_one(&g);
    CHECK(g.phase == PH_PLAY); /* nothing left to offer: no pick */
}

static void augment_effects(void) {
    Game g, saved; ready(&g);
    /* Neutral outside a run. */
    CHECK(raid_basic_bonus(&g) == 0 && raid_range_bonus(&g) == 0 && raid_wind_damage(&g) == 0 &&
          raid_technique_bonus(&g) == 0 && raid_cooldown(&g, 2) == 2 && raid_dodge_recharge(&g, 1.2f) == 1.2f);
    float range = game_attack_range(&g, MOVE_ATTACK);
    CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    for (int i = 0; i < RAID_AUG_COUNT; i++) g.raid.stacks[i] = 2;
    CHECK(raid_basic_bonus(&g) == 4 && raid_range_bonus(&g) == 12 && raid_wind_damage(&g) == 8 &&
          raid_technique_bonus(&g) == 6);
    CHECK(fabsf(raid_cooldown(&g, 2) - 1.4f) < .001f && fabsf(raid_dodge_recharge(&g, 1.2f) - .8f) < .001f);
    CHECK(game_attack_range(&g, MOVE_ATTACK) > range);
    /* 피의 기운 heals on a kill, up to max. */
    run(&g, 4); g.hero.hp = 50; g.hero.invul = 1;
    for (int i = 0; i < HUNT_COUNT; i++) if (g.hunt[i].hp > 0) {
        g.hero.pos = g.hunt[i].pos; CHECK(game_damage(&g, &g.hunt[i], 999, FX_HIT)); break;
    }
    CHECK(g.hero.hp == 56);
    /* Restoring drops every temporary augment. */
    CHECK(raid_cancel(&g, &saved));
    CHECK(raid_basic_bonus(&g) == 0 && game_attack_range(&g, MOVE_ATTACK) == range);
}

static void results_and_rewards(void) {
    Game g, saved;
    /* Surviving the full time is a success: earned 문 + win bonus + shards. */
    ready(&g); g.coins = 100; g.spirit_shards = 1;
    CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    run(&g, 6); kill_one(&g); kill_one(&g);
    g.raid.time = RAID_SECONDS - .1f; g.raid.elite_spawned = true;
    run(&g, .5f);
    CHECK(g.phase == PH_RAID && g.raid.stage == RAID_RESULT && g.raid.success);
    CHECK(raid_reward_coins(&g.raid) == 2 * RAID_KILL_COINS + RAID_WIN_COINS && raid_reward_shards(&g.raid) == RAID_WIN_SHARDS);
    simulation_tick(&g, .05f, (Vec2){0}); CHECK(g.phase == PH_RAID); /* frozen */
    CHECK(raid_leave(&g, &saved));
    CHECK(!g.raid.active && g.phase == PH_PLAY && g.map == MAP_VILLAGE);
    CHECK(g.coins == 100 + 2 * RAID_KILL_COINS + RAID_WIN_COINS && g.spirit_shards == 1 + RAID_WIN_SHARDS);
    CHECK(strstr(g.toast, "토벌 성공") != NULL);
    CHECK(!raid_leave(&g, &saved));
    unsigned char bytes[SAVE_CAPACITY]; size_t size = 0;
    CHECK(save_encode(&g, bytes, &size) == SAVE_OK);
    /* The elite appears at its time and ends the run when it falls. */
    ready(&g); CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    g.raid.time = RAID_ELITE_AT - .1f; run(&g, .3f);
    CHECK(g.raid.elite_spawned && g.raid.elite >= 0);
    Enemy *elite = &g.hunt[g.raid.elite];
    CHECK(elite->hp == RAID_ELITE_HP && raid_is_elite(&g, elite));
    g.hero.pos = elite->pos; CHECK(game_damage(&g, elite, 999, FX_HIT));
    CHECK(g.phase == PH_RAID && g.raid.stage == RAID_RESULT && g.raid.success);
    /* Falling in the run is a failed run, not the death screen; half the 문. */
    ready(&g); g.coins = 10; CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    run(&g, 6); kill_one(&g); kill_one(&g); kill_one(&g);
    g.hero.invul = 0; game_hurt(&g, 999, false);
    CHECK(g.phase == PH_RAID && g.raid.stage == RAID_RESULT && !g.raid.success);
    CHECK(raid_reward_coins(&g.raid) == 3 * RAID_KILL_COINS / 2 && raid_reward_shards(&g.raid) == 0);
    CHECK(raid_leave(&g, &saved) && g.coins == 10 + 3 * RAID_KILL_COINS / 2 && g.hero.hp > 0 && g.phase == PH_PLAY);
    /* Giving up from the pause menu is the same as failing. */
    ready(&g); CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    g.phase = PH_PAUSED; CHECK(raid_abandon(&g) && g.raid.stage == RAID_RESULT && !g.raid.success);
    CHECK(!raid_abandon(&g));
    /* Coin and shard caps hold. */
    CHECK(raid_leave(&g, &saved));
    ready(&g); g.coins = COIN_LIMIT; g.spirit_shards = SPIRIT_LIMIT;
    CHECK(raid_offer(&g) && raid_enter(&g, &saved));
    g.raid.time = RAID_SECONDS; run(&g, .1f); CHECK(g.raid.success);
    CHECK(raid_leave(&g, &saved) && g.coins == COIN_LIMIT && g.spirit_shards == SPIRIT_LIMIT);
}

int main(void) {
    offer_and_isolation();
    packs_and_picks();
    augment_effects();
    results_and_rewards();
    printf("Raid: %d checks passed\n", checks);
    return 0;
}
