#include "test_legacy_world.h"
#include "test_simulation.h"
#include "game.h"
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(condition) do { checks++; if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

static void advance(Game *game, int frames) {
    for (int frame = 0; frame < frames; frame++) simulation_tick(game, .01f, (Vec2){0, 0});
}

static void isolation_and_restore(void) {
    for (int quest = 0; quest < QUEST_COUNT; quest++) {
        Game game, saved;
        legacy_reset(&game);
        CHECK(!game_enter_training(&game, &saved));
        game_start(&game);
        game.quest = (Quest)quest;
        game.hero.hp = 37;
        game.hero.bleed = 4;
        game.hero.bleed_tick = .2f;
        game.coins = 347;
        game.herbs = 2;
        game.traces[0] = game.traces[1] = true;
        game.drops[1] = (Drop){.exists = true, .pos = game.hero.pos, .age = .5f};
        game.boss.state = AI_WINDUP;
        game.boss.timer = .1f;
        game.cooldown = 2.3f;
        game.technique_cooldowns[TECH_THRUST] = 1.7f;
        game.item_cooldown = .7f;
        game.attack_cooldown = .3f;
        game.queued_move = TECH_SWEEP;
        game.buffer_time = .2f;
        game.input_count = game.combo_count = 2;
        game.input_time = game.combo_time = .7f;
        game.last_move = game.last_hit_move = TECH_THRUST;
        game.combat_request = TRAIN_QUEUED;
        game.input_move = TECH_SWEEP;
        game.request_time = .5f;
        game.last_impact = IMPACT_HIT;
        game.hero.attacking = true;
        game.hero.attack_elapsed = .05f;
        Game before = game;
        CHECK(game_enter_training(&game, &saved));
        CHECK(memcmp(&before, &saved, sizeof saved) == 0);
        CHECK(!game_enter_training(&game, &saved));
        CHECK(memcmp(&before, &saved, sizeof saved) == 0);
        CHECK(world_clear(game.hero.pos, HERO_RADIUS));
        CHECK(world_clear(game.boss.pos, BOSS_RADIUS));
        CHECK(world_segment(game.hero.pos, game.boss.pos, HERO_RADIUS));
        CHECK(game_nearby(&game).kind == NEAR_NONE);
        CHECK(!game_boars_active(&game) && !game_begin_boss(&game));
        game.hero.pos = NPCS[ELDER].pos;
        CHECK(!game_accept(&game) && !game_report(&game));
        game.hero.pos = NPCS[HERBALIST].pos;
        CHECK(!game_deliver(&game) && !game_open_shop(&game, HERBALIST));
        CHECK(!game_use(&game, ITEM_TONIC) && !game_use(&game, ITEM_BANDAGE));
        game.hero.pos = TRACES[0].pos;
        CHECK(!game_inspect(&game, 0) && game_pickup_nearby(&game) == 0);
        game_hurt(&game, 999, true);
        CHECK(game.hero.hp == 100 && game.hero.bleed == 0);
        Vec2 dummy = game.boss.pos;
        int coins = game.coins, tonics = game.tonics, bandages = game.bandages;
        for (int hit = 0; hit < 100; hit++) CHECK(game_damage(&game, &game.boss, 9999, FX_HIT));
        advance(&game, 1000);
        CHECK(game.boss.hp == game.boss.max_hp && game.boss.state == AI_IDLE);
        CHECK(distance(dummy, game.boss.pos) == 0 && game.hero.hp == 100);
        CHECK(game.coins == coins && game.tonics == tonics && game.bandages == bandages);
        CHECK(game.quest == Q_MEET && game.herbs == 0 && game.notices == 0);
        for (int drop = 0; drop < BOAR_COUNT; drop++) CHECK(!game.drops[drop].exists);
        CHECK(memcmp(&before, &saved, sizeof saved) == 0);
        game.phase = PH_TECHNIQUES;
        CHECK(game_bind_swipe(&game, SWIPE_UP, TECH_SWEEP));
        CHECK(game_leave_training(&game, &saved));
        CHECK(!game_leave_training(&game, &saved));
        before.queued_move = before.last_move = before.last_hit_move = before.input_move = MOVE_NONE;
        before.buffer_time = before.combo_time = before.input_time = before.request_time = 0;
        before.combo_count = before.input_count = 0;
        before.last_impact = IMPACT_NONE;
        before.combat_request = TRAIN_READY;
        before.hero.attacking = before.hero.skilling = before.hero.walking = false;
        before.hero.attack_elapsed = before.hero.skill_elapsed = before.hero.walk_cycle = 0;
        CHECK(memcmp(&before, &game, sizeof game) == 0);
        advance(&game, 1);
        CHECK(!game.hero.attacking && !game.hero.skilling && game.queued_move == MOVE_NONE);
    }
}

static void combos_and_misses(void) {
    for (int first = 0; first < SWIPE_COUNT; first++) {
        for (int second = 0; second < SWIPE_COUNT; second++) {
            if (first == second) continue;
            for (int third = 0; third < SWIPE_COUNT; third++) {
                if (third == first || third == second) continue;
                Game game, saved;
                legacy_reset(&game); game_start(&game);
                CHECK(game_enter_training(&game, &saved));
                Technique opening = game.swipe_bindings[first], next = game.swipe_bindings[second];
                Technique finish = game.swipe_bindings[third];
                CHECK(game_technique(&game, opening));
                advance(&game, 10);
                CHECK(game.training.hits == 0 && game.training.misses == 0);
                CHECK(game_technique(&game, next));
                CHECK(game.combat_request == TRAIN_QUEUED && game.queued_move == (int)next);
                CHECK(!game_technique(&game, opening));
                CHECK(game.combat_request == TRAIN_COOLDOWN && game.queued_move == (int)next);
                advance(&game, 65);
                CHECK(game.training.hits == 1 && game.hero.technique == next && game.combo_count == 1 && game.input_count == 2);
                CHECK(game.training.last_damage == TECHNIQUES[opening].damage);
                CHECK(game_technique(&game, finish));
                advance(&game, 65);
                CHECK(game.training.hits == 2 && game.combo_count == 2 && game.input_count == 3);
                CHECK(game.training.last_damage == (TECHNIQUES[next].damage * 110 + 50) / 100);
                advance(&game, 64);
                CHECK(game.training.hits == 3 && game.training.misses == 0);
                CHECK(game.training.last_damage == (TECHNIQUES[finish].damage * 120 + 50) / 100);
                CHECK(game.boss.hp == game.boss.max_hp && game.coins == 55 && game.notices == 0);
                CHECK(game_leave_training(&game, &saved));
            }
        }
    }
    Game game, saved;
    legacy_reset(&game); game_start(&game);
    CHECK(game_enter_training(&game, &saved));
    CHECK(game_attack(&game));
    advance(&game, 15);
    CHECK(game.training.hits == 1 && game.training.last_damage == 9);
    CHECK(game_technique(&game, TECH_SWEEP));
    advance(&game, 60);
    CHECK(game.training.hits == 2 && game.combo_count == 2);
    advance(&game, 200);
    CHECK(game.combo_time == 0);
    CHECK(game_attack(&game));
    CHECK(game.combo_count == 0 && game.input_count == 1);
    game.hero.pos.x += 250;
    advance(&game, 50);
    CHECK(game.training.misses == 1 && game.training.last_damage == 0);
    CHECK(game_technique(&game, TECH_SWEEP));
    advance(&game, 70);
    CHECK(game.training.misses == 2);
    CHECK(game_leave_training(&game, &saved));
    for (int augment = AUG_NONE; augment <= AUG_FLOW; augment++) {
        legacy_reset(&game); game_start(&game);
        game.augment = (Augment)augment; game.sword = true;
        CHECK(game_enter_training(&game, &saved));
        CHECK(game.augment == (Augment)augment && game.sword);
        CHECK(game_attack(&game)); advance(&game, 50);
        CHECK(game.training.hits == 1);
        CHECK(game.training.last_damage == (augment == AUG_STEEL ? 18 : augment == AUG_WIND ? 20 : 14));
        CHECK(game_leave_training(&game, &saved));
    }
    legacy_reset(&game); game_start(&game);
    CHECK(game_enter_training(&game, &saved));
    game.hero.pos = (Vec2){250 * 1.5f, 314 * 1.5f};
    game.boss.pos = (Vec2){250 * 1.5f, 257 * 1.5f};
    CHECK(!world_segment(game.hero.pos, game.boss.pos, 0));
    CHECK(game_technique(&game, TECH_CLEAVE)); advance(&game, 70);
    CHECK(game.training.hits == 0 && game.training.misses == 1);
    CHECK(world_clear(game.hero.pos, HERO_RADIUS));
    CHECK(game_leave_training(&game, &saved));
}

int main(void) {
    isolation_and_restore();
    combos_and_misses();
    printf("PASS: %d isolated training checks.\n", checks);
    return 0;
}
