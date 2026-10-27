#include "test_legacy_world.h"
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
static bool near(float a, float b) { return fabsf(a - b) < .0001f; }
static void tick(Game *g, float dt) { game_tick(g, dt, (Vec2){0}); }
static void fixture(Game *g) {
    Game saved;
    legacy_reset(g); game_start(g); CHECK(game_enter_training(g, &saved));
    g->hero.pos = BOSS_HOME; g->hero.dir = (Vec2){1, 0};
    g->boss.pos = (Vec2){BOSS_HOME.x + 40, BOSS_HOME.y};
}
static void real_frames(Game *g, int frames) { for (int i = 0; i < frames; i++) tick(g, .01f); }
static void stop_and_resume(void) {
    Game g; fixture(&g); CHECK(game_attack(&g)); real_frames(&g, 15);
    CHECK(g.training.hits == 1 && g.training.last_damage == 9 && g.combo_count == 1);
    CHECK(near(g.hitstop_time, HITSTOP_LIGHT_SECONDS) && g.boss.hit_time == HIT_FLASH_SECONDS);
    Game before = g, expected = g;
    game_tick(&g, .01f, (Vec2){1, 0});
    expected.hitstop_time -= .01f;
    CHECK(memcmp(&g, &expected, sizeof g) == 0); /* Every simulation clock and world field stays frozen. */
    CHECK(game_technique(&g, TECH_SWEEP));
    CHECK(g.queued_move == TECH_SWEEP && g.combat_request == TRAIN_QUEUED);
    CHECK(!game_attack(&g) && g.combat_request == TRAIN_COOLDOWN);
    CHECK(g.queued_move == TECH_SWEEP && g.technique_cooldowns[TECH_SWEEP] == 0);
    float remaining = g.hitstop_time, buffer = g.buffer_time;
    game_tick(&g, .05f, (Vec2){1, 0});
    float resumed = .05f - remaining;
    float speed = HERO_SPEED * (before.hero.attacking || before.hero.skilling ? HERO_ATTACK_MOVE_SCALE : 1);
    float run_up = speed * (resumed - HERO_ACCEL_SECONDS * (1 - expf(-resumed / HERO_ACCEL_SECONDS)));
    CHECK(g.hitstop_time == 0 && near(g.hero.pos.x - before.hero.pos.x, run_up));
    CHECK(near(g.time - before.time, resumed) && near(g.combo_time, before.combo_time - resumed));
    CHECK(near(g.buffer_time, buffer - resumed) && g.queued_move == TECH_SWEEP);
    CHECK(g.training.hits == 1 && g.combo_count == 1);
    real_frames(&g, 90);
    CHECK(g.training.hits == 2 && g.training.misses == 0 && g.combo_count == 2);
    CHECK(g.training.last_damage == 13); /* Existing combo multiplier, exactly one follow-up. */
    real_frames(&g, 120); CHECK(g.training.hits == 2 && !g.hero.skilling && g.queued_move == MOVE_NONE);
}
static void accepted_damage_only(void) {
    for (int move = MOVE_ATTACK; move < TECH_COUNT; move++) {
        for (int miss = 0; miss < 2; miss++) {
            Game g; fixture(&g);
            CHECK(move == MOVE_ATTACK ? game_attack(&g) : game_technique(&g, (Technique)move));
            if (miss) g.boss.pos = (Vec2){g.hero.pos.x - 40, g.hero.pos.y};
            real_frames(&g, move == MOVE_ATTACK ? 15 : 25);
            CHECK(g.training.hits == (unsigned)!miss && g.training.misses == (unsigned)miss);
            CHECK(miss ? g.hitstop_time == 0 && g.boss.hit_time == 0 : g.hitstop_time > 0 && g.boss.hit_time > 0);
            if (!miss) CHECK(g.hitstop_time <= HITSTOP_HEAVY_SECONDS);
        }
    }
    Game g; fixture(&g); g.augment = AUG_WIND;
    CHECK(game_attack(&g)); real_frames(&g, 15);
    CHECK(g.training.last_damage == 15 && near(g.hitstop_time, HITSTOP_LIGHT_SECONDS));
    fixture(&g); g.augment = AUG_WIND; g.boss.pos.x = g.hero.pos.x + 100;
    CHECK(game_attack(&g)); real_frames(&g, 15);
    CHECK(g.training.last_damage == 6 && near(g.hitstop_time, HITSTOP_WIND_SECONDS));
    fixture(&g); CHECK(game_technique(&g, TECH_CLEAVE));
    for (int frame = 0; frame < 30 && !g.hero.skill_hit; frame++) tick(&g, .01f);
    CHECK(near(g.hitstop_time, HITSTOP_HEAVY_SECONDS));
    fixture(&g);
    CHECK(!game_damage(&g, &g.boars[0], 12, FX_HIT));
    CHECK(!game_damage(&g, &g.boss, 0, FX_HIT) && !game_damage(&g, &g.boss, -1, FX_HIT));
    CHECK(g.hitstop_time == 0 && g.boss.hit_time == 0);
    game_hurt(&g, 99, true); CHECK(g.hitstop_time == 0 && g.hero.hp == 100 && g.hero.hurt_time == 0);
    g.phase = PH_PAUSED;
    CHECK(!game_damage(&g, &g.boss, 12, FX_HIT));
    CHECK(g.hitstop_time == 0);
}
static void victims_and_enemy_timing(void) {
    Game g; legacy_reset(&g); game_start(&g);
    g.quest = Q_RECOVER; g.traces[0] = g.traces[1] = true; g.hero.pos = BOSS_HOME;
    for (int i = 0; i < BOAR_COUNT; i++) {
        g.boars[i].pos = (Vec2){BOSS_HOME.x + 30 + i * 45, BOSS_HOME.y}; /* bodies apart */
        g.boars[i].state = AI_WINDUP; g.boars[i].timer = .4f;
        CHECK(game_damage(&g, &g.boars[i], 1, FX_HIT));
        CHECK(near(g.hitstop_time, HITSTOP_LIGHT_SECONDS)); /* Max, never sum. */
        CHECK(g.boars[i].state == AI_WINDUP && g.boars[i].timer == .4f);
    }
    Vec2 hero = g.hero.pos, enemy = g.boars[0].pos;
    tick(&g, .02f);
    CHECK(g.hero.hp == 100 && g.boars[0].timer == .4f && distance(enemy, g.boars[0].pos) == 0);
    tick(&g, .03f);
    CHECK(near(g.boars[0].timer, .385f));
    /* Approved knockback: after the stop the boar slides away from the hero
     * (the hero is not pushed by its own hit), within the decaying total. */
    CHECK(distance(hero, g.hero.pos) == 0 && g.boars[0].pos.x > enemy.x &&
          fabsf(g.boars[0].pos.y - enemy.y) < .01f && g.boars[0].pos.x - enemy.x < 70 / KNOCK_DECAY + .01f);
    real_frames(&g, 12); CHECK(g.boars[0].hit_time == 0);
    /* Death/reward stays one-shot; a rejected repeated hit cannot extend the stop. */
    g.boars[0].hp = 1; int coins = g.coins;
    CHECK(game_damage(&g, &g.boars[0], 100, FX_HIT)); tick(&g, .01f);
    float remaining = g.hitstop_time;
    CHECK(!game_damage(&g, &g.boars[0], 100, FX_HIT));
    CHECK(g.hitstop_time == remaining && g.coins == coins + 9 && g.drops[0].exists);
}
static void hero_hurt_and_idle_buffer(void) {
    Game g; fixture(&g); g.training.active = false; g.quest = Q_COMBAT;
    g.boss.state = AI_RECOVER; g.boss.timer = 99;
    g.armor = true; game_hurt(&g, 9, true);
    CHECK(g.hero.hp == 94 && g.hero.invul == .75f && g.hero.bleed == 5);
    CHECK(g.hero.hurt_time == HIT_FLASH_SECONDS && near(g.hitstop_time, HITSTOP_HURT_SECONDS));
    CHECK(game_technique(&g, TECH_SWEEP));
    CHECK(!g.hero.skilling && g.queued_move == TECH_SWEEP && g.technique_cooldowns[TECH_SWEEP] == 0);
    float stop = g.hitstop_time; game_hurt(&g, 9, true);
    CHECK(g.hero.hp == 94 && g.hitstop_time == stop); /* Invulnerability rejects feedback too. */
    tick(&g, .02f); CHECK(g.hero.invul == .75f && g.hero.bleed == 5 && !g.hero.skilling);
    tick(&g, .03f);
    CHECK(g.hitstop_time == 0 && g.hero.skilling && g.queued_move == MOVE_NONE);
    CHECK(g.hero.skill_elapsed == 0 && g.technique_cooldowns[TECH_SWEEP] == TECHNIQUES[TECH_SWEEP].cooldown);
    CHECK(near(g.hero.invul, .745f) && near(g.hero.bleed, 4.995f));
    int hp = g.hero.hp; game_hurt(&g, 0, false); CHECK(g.hero.hp == hp);
    game_recover(&g); CHECK(g.hitstop_time == 0 && g.hero.hurt_time == 0 && g.boss.hit_time == 0);
    game_hurt(&g, 4, false); CHECK(g.hitstop_time > 0);
    game_retry(&g); CHECK(g.hitstop_time == 0 && g.hero.hurt_time == 0);
}
static void pause_restore_and_frame_rates(void) {
    for (int phase = PH_TITLE; phase <= PH_TECHNIQUES; phase++) {
        if (phase == PH_PLAY) continue;
        Game g; fixture(&g); CHECK(game_damage(&g, &g.boss, 1, FX_HIT));
        g.phase = (Phase)phase; tick(&g, .01f);
        CHECK(g.hitstop_time == 0 && g.boss.hit_time == 0 && g.hero.hurt_time == 0);
    }
    const int rates[] = {30, 60, 120};
    for (int i = 0; i < 3; i++) {
        Game g; fixture(&g); CHECK(game_damage(&g, &g.boss, 1, FX_HIT));
        for (int frame = 0; frame < rates[i] / 5; frame++) tick(&g, 1.0f / rates[i]);
        CHECK(near(g.time, .2f - HITSTOP_LIGHT_SECONDS));
        CHECK(g.hitstop_time == 0 && g.boss.hit_time == 0);
    }
    Game g, saved; fixture(&g); g.training.active = false;
    g.quest = Q_COMBAT; g.hero.hp = 41; g.coins = 123;
    g.hero.hurt_time = .1f; g.hitstop_time = .04f; g.boss.hit_time = .08f; g.cooldown = 2;
    CHECK(game_enter_training(&g, &saved)); CHECK(g.hitstop_time == 0 && g.boss.hit_time == 0);
    CHECK(game_damage(&g, &g.boss, 20, FX_HIT)); CHECK(game_leave_training(&g, &saved));
    CHECK(g.hero.hp == 41 && g.coins == 123 && g.quest == Q_COMBAT && g.cooldown == 2);
    CHECK(g.hitstop_time == 0 && g.hero.hurt_time == 0 && g.boss.hit_time == 0);
}
int main(void) {
    stop_and_resume(); accepted_damage_only(); victims_and_enemy_timing();
    hero_hurt_and_idle_buffer(); pause_restore_and_frame_rates();
    printf("PASS: %d real-time hit feedback checks.\n", checks);
    return 0;
}
