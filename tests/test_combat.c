#include "test_legacy_world.h"
#include "test_simulation.h"
#include "game.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
static void advance(Game *g, int ticks) {
    for (int i = 0; i < ticks; i++) simulation_tick(g, .01f, (Vec2){0});
}
static void fixture(Game *g) {
    Game saved;
    legacy_reset(g); game_start(g);
    CHECK(game_enter_training(g, &saved));
    g->hero.pos = BOSS_HOME;
    g->hero.dir = (Vec2){1, 0};
    g->boss.pos = (Vec2){BOSS_HOME.x + 40, BOSS_HOME.y};
}
static bool request(Game *g, int move) {
    return move == MOVE_ATTACK ? game_attack(g) : game_technique(g, (Technique)move);
}
static Vec2 polar(Vec2 p, float a, float r) {
    return (Vec2){p.x + cosf(a) * r, p.y + sinf(a) * r};
}
static void sectors(void) {
    Game g;
    fixture(&g);
    for (int move = MOVE_ATTACK; move < TECH_COUNT; move++) {
        float r = game_attack_range(&g, move), half = game_attack_half_angle(move);
        for (int direction = 0; direction < 8; direction++) {
            float a = direction * .785398163f;
            CHECK(game_attack_contains(BOSS_HOME, a, r, move, polar(BOSS_HOME, a, r - .1f)));
            CHECK(!game_attack_contains(BOSS_HOME, a, r, move, polar(BOSS_HOME, a, r + .1f)));
            CHECK(game_attack_contains(BOSS_HOME, a, r, move, polar(BOSS_HOME, a + half - .001f, 40)));
            CHECK(!game_attack_contains(BOSS_HOME, a, r, move, polar(BOSS_HOME, a + half + .001f, 40)));
            CHECK(game_attack_contains(BOSS_HOME, a, r, move, polar(BOSS_HOME, a - half + .001f, 40)));
            CHECK(!game_attack_contains(BOSS_HOME, a, r, move, polar(BOSS_HOME, a - half - .001f, 40)));
            CHECK(!game_attack_contains(BOSS_HOME, a, r, move, polar(BOSS_HOME, a + 3.14159265f, 40)));
            CHECK(game_attack_contains(BOSS_HOME, a, r, move, polar(BOSS_HOME, a + 1.5707963f, 40)) == (move == TECH_SWEEP));
            CHECK(game_attack_contains(BOSS_HOME, a, r, move, BOSS_HOME));
        }
        /* Exact radius remains exclusive, as in the original game. */
        CHECK(!game_attack_contains(BOSS_HOME, 0, r, move, polar(BOSS_HOME, 0, r)));
        Vec2 wall = {375, 471}, blocked = {375, 385.5f};
        CHECK(!world_segment(wall, blocked, 0));
        CHECK(!game_attack_contains(wall, -1.5707963f, 120, move, blocked));
    }
    CHECK(!game_attack_contains(BOSS_HOME, NAN, 58, MOVE_ATTACK, BOSS_HOME));
    CHECK(!game_attack_contains(BOSS_HOME, 0, 58, MOVE_NONE, BOSS_HOME));
}
static void actual_impacts(void) {
    for (int move = MOVE_ATTACK; move < TECH_COUNT; move++) {
        for (int position = 0; position < 6; position++) {
            Game g; fixture(&g);
            CHECK(request(&g, move));
            CHECK(g.combo_count == 0 && g.input_count == 1);
            float step = move >= 0 ? fmaxf(0, TECHNIQUES[move].step) : 0;
            float r = game_attack_range(&g, move);
            Vec2 targets[] = {{BOSS_HOME.x + step + 20, BOSS_HOME.y},
                {BOSS_HOME.x, BOSS_HOME.y + 70}, {BOSS_HOME.x - 40, BOSS_HOME.y},
                {BOSS_HOME.x + step + r - .1f, BOSS_HOME.y},
                {BOSS_HOME.x + step + r, BOSS_HOME.y},
                {BOSS_HOME.x + step + r + .1f, BOSS_HOME.y}};
            g.boss.pos = targets[position]; /* Move after aim locks. */
            int windup = move == MOVE_ATTACK ? 13 : 23;
            advance(&g, windup);
            CHECK(g.training.hits == 0 && g.training.misses == 0 && g.combo_count == 0);
            advance(&g, 2);
            bool hit = position == 0 || position == 3 || (position == 1 && move == TECH_SWEEP);
            CHECK(g.training.hits == (unsigned)hit && g.training.misses == (unsigned)!hit);
            CHECK(g.combo_count == (int)hit);
            CHECK(g.last_impact == (hit ? IMPACT_HIT : IMPACT_MISS));
            CHECK(g.training.last_damage == (hit ? (move == MOVE_ATTACK ? 9 : TECHNIQUES[move].damage) : 0));
            advance(&g, 55);
            CHECK(g.training.hits + g.training.misses == 1); /* One impact only. */
        }
    }
    /* A target crossed during forward motion still touches the forward blade. */
    for (int move = TECH_CLEAVE; move <= TECH_THRUST; move += 2) {
        Game g; fixture(&g); g.boss.pos.x = g.hero.pos.x + 5;
        CHECK(request(&g, move)); advance(&g, 25);
        CHECK(g.training.hits == 1);
        CHECK(g.hero.pos.x > g.boss.pos.x);
    }
    /* No late target snapping when the locked target walks behind the hero. */
    Game g; fixture(&g); CHECK(game_attack(&g));
    g.boss.pos.x = g.hero.pos.x - 20; advance(&g, 15);
    CHECK(g.training.misses == 1 && g.hero.attack_angle == 0);
    /* No aim target at start: late arrivals must still be in the locked cone. */
    fixture(&g); g.boss.pos.x += 300; CHECK(game_attack(&g));
    g.boss.pos = (Vec2){g.hero.pos.x - 20, g.hero.pos.y}; advance(&g, 15);
    CHECK(g.training.misses == 1);
    fixture(&g); CHECK(game_attack(&g)); g.boss.hp = 0; advance(&g, 15);
    CHECK(g.combo_count == 0 && g.training.misses == 1);
    for (int move = MOVE_ATTACK; move < TECH_COUNT; move++) {
        fixture(&g); g.hero.pos = (Vec2){375, 471}; g.boss.pos = (Vec2){375, 385.5f};
        g.hero.dir = (Vec2){0, -1}; g.augment = AUG_WIND;
        CHECK(request(&g, move)); advance(&g, 65);
        CHECK(g.training.hits == 0 && g.training.misses == 1);
        CHECK(world_clear(g.hero.pos, HERO_RADIUS));
    }
}
static void confirmed_chains(void) {
    Game g; fixture(&g);
    CHECK(game_attack(&g)); g.boss.pos.x += 300;
    CHECK(game_technique(&g, TECH_SWEEP)); advance(&g, 45);
    CHECK(g.input_count == 2 && g.combo_count == 0 && g.training.misses == 1);
    g.boss.pos = (Vec2){g.hero.pos.x + 40, g.hero.pos.y}; advance(&g, 25);
    CHECK(g.combo_count == 1 && g.training.last_damage == 12);
    CHECK(game_technique(&g, TECH_CLEAVE));
    advance(&g, 40); g.boss.pos.x += 300; advance(&g, 25);
    CHECK(g.combo_count == 0 && g.training.misses == 2 && g.input_count == 3);
    CHECK(game_attack(&g)); advance(&g, 40);
    g.boss.pos = (Vec2){g.hero.pos.x + 40, g.hero.pos.y}; advance(&g, 20);
    CHECK(g.combo_count == 1 && g.training.last_damage == 9);

    /* Expiry is checked at impact, not when the next input is accepted. */
    for (int timely = 0; timely < 2; timely++) {
        fixture(&g); CHECK(game_attack(&g)); advance(&g, 50);
        g.combo_time = timely ? .30f : .20f;
        CHECK(game_technique(&g, TECH_SWEEP));
        CHECK(g.combo_count == 1); advance(&g, 25);
        CHECK(g.combo_count == (timely ? 2 : 1));
        CHECK(g.training.last_damage == (timely ? 13 : 12));
    }
    /* Repeat resets, distinct fourth move saturates at three. Cooldowns remain
     * enforced in other cases; reset here only to isolate the duplicate rule. */
    fixture(&g);
    const int moves[] = {MOVE_ATTACK, TECH_SWEEP, TECH_CLEAVE, TECH_RETREAT, TECH_RETREAT};
    for (int i = 0; i < 5; i++) {
        if (i == 4) g.technique_cooldowns[TECH_RETREAT] = 0;
        g.hero.pos = BOSS_HOME; g.boss.pos = polar(BOSS_HOME, 0, 40);
        CHECK(request(&g, moves[i])); advance(&g, 65);
        CHECK(g.combo_count == (i == 4 ? 1 : i < 3 ? i + 1 : 3));
    }
    advance(&g, 110);
    CHECK(g.combo_count == 0 && g.last_hit_move == MOVE_NONE);
    CHECK(g.input_count == 0 && g.last_move == MOVE_NONE);
    /* Wind alone is a real hit, remains flat 6 damage, and counts only once. */
    fixture(&g); g.augment = AUG_WIND; g.boss.pos.x = g.hero.pos.x + 100;
    CHECK(game_attack(&g)); advance(&g, 15);
    CHECK(g.training.hits == 1 && g.combo_count == 1 && g.training.last_damage == 6);
    fixture(&g); g.augment = AUG_WIND;
    CHECK(game_attack(&g)); advance(&g, 15);
    CHECK(g.training.hits == 1 && g.combo_count == 1 && g.training.last_damage == 15);
}
static void reservations(void) {
    Game g; fixture(&g);
    CHECK(game_technique(&g, TECH_CLEAVE));
    CHECK(game_technique(&g, TECH_SWEEP));
    CHECK(game_technique(&g, TECH_RETREAT));
    CHECK(game_attack(&g));
    CHECK(g.queued_move == MOVE_ATTACK && g.combat_request == TRAIN_QUEUED);
    float lifetime = g.buffer_time;
    CHECK(!game_technique(&g, TECH_CLEAVE));
    CHECK(g.combat_request == TRAIN_COOLDOWN && g.queued_move == MOVE_ATTACK && g.buffer_time == lifetime);
    CHECK(!game_technique(&g, (Technique)-1));
    CHECK(g.combat_request == TRAIN_INVALID && g.queued_move == MOVE_ATTACK && g.buffer_time == lifetime);
    game_invalid_direction(&g);
    CHECK(g.queued_move == MOVE_ATTACK && g.buffer_time == lifetime);
    CHECK(game_technique(&g, TECH_THRUST)); advance(&g, 65);
    CHECK(g.hero.technique == TECH_THRUST && g.queued_move == MOVE_NONE);
    CHECK(g.combo_count == 1 && g.input_count == 2);
    advance(&g, 150);
    CHECK(!g.hero.attacking && !g.hero.skilling && g.training.hits == 2);
    CHECK(g.attack_cooldown == 0 && game_technique_cooldown(&g, TECH_RETREAT) == 0);
    fixture(&g); CHECK(game_attack(&g)); CHECK(!game_attack(&g));
    CHECK(g.combat_request == TRAIN_COOLDOWN && g.queued_move == MOVE_NONE);
}
static void multiple_targets(void) {
    Game g; legacy_reset(&g); game_start(&g);
    g.traces[0] = g.traces[1] = true; g.quest = Q_RECOVER; g.hero.pos = BOSS_HOME;
    for (int i = 0; i < BOAR_COUNT; i++) {
        g.boars[i].pos = polar(BOSS_HOME, i == 2 ? 3.14159265f : i * .7f, i == 0 ? 28 : 50); /* bodies apart */
        g.boars[i].timer = 99; g.boars[i].state = AI_RECOVER;
    }
    CHECK(game_technique(&g, TECH_SWEEP)); advance(&g, 25);
    CHECK(g.boars[0].hp == 26 && g.boars[1].hp == 26 && g.boars[2].hp == 38);
    CHECK(g.combo_count == 1); /* Two victims, one combo link. */
    advance(&g, 40); CHECK(game_attack(&g)); advance(&g, 15);
    CHECK(g.boars[0].hp == 16 && g.boars[1].hp == 26); /* Basic stays single target. */
    CHECK(g.combo_count == 2);
    /* No target was locked; a nearer enemy behind must not steal the frontal hit. */
    advance(&g, 80);
    for (int i = 0; i < BOAR_COUNT; i++) g.boars[i].pos.x += 500;
    g.hero.dir = (Vec2){1, 0}; CHECK(game_attack(&g));
    g.boars[0].pos = polar(g.hero.pos, 3.14159265f, 10);
    g.boars[1].pos = polar(g.hero.pos, 0, 30);
    int hp0 = g.boars[0].hp, hp1 = g.boars[1].hp;
    advance(&g, 15);
    CHECK(g.boars[0].hp == hp0 && g.boars[1].hp == hp1 - 9);
}
/* The on-screen chain counts every confirmed hit in the window, including a
 * repeated move, while the damage tier keeps its own rules. */
static void display_chain(void) {
    Game g; fixture(&g);
    const int moves[] = {MOVE_ATTACK, TECH_SWEEP, TECH_CLEAVE, TECH_RETREAT, TECH_RETREAT};
    for (int i = 0; i < 5; i++) {
        if (i == 4) g.technique_cooldowns[TECH_RETREAT] = 0;
        g.hero.pos = BOSS_HOME; g.boss.pos = polar(BOSS_HOME, 0, 40);
        CHECK(request(&g, moves[i])); advance(&g, 65);
        CHECK(g.chain_hits == i + 1 && g.chain_fade == 0);
        CHECK(g.combo_count == (i == 4 ? 1 : i < 3 ? i + 1 : 3));
    }
    advance(&g, 110);
    CHECK(g.chain_hits == 0 && g.chain_last == 5 && g.chain_fade > 0);
    advance(&g, 50);
    CHECK(g.chain_fade == 0 && g.chain_last == 5);
    /* A whiff breaks the chain at once. */
    fixture(&g);
    CHECK(game_attack(&g)); advance(&g, 50);
    CHECK(g.chain_hits == 1);
    g.boss.pos.x += 300;
    CHECK(game_technique(&g, TECH_SWEEP)); advance(&g, 30);
    CHECK(g.chain_hits == 0 && g.chain_last == 0 && g.chain_fade == 0);
    /* Leaving training clears it without a fade. */
    Game saved;
    legacy_reset(&g); game_start(&g);
    CHECK(game_enter_training(&g, &saved));
    for (int i = 0; i < 2; i++) {
        g.hero.pos = BOSS_HOME; g.boss.pos = polar(BOSS_HOME, 0, 40);
        CHECK(request(&g, i ? TECH_SWEEP : MOVE_ATTACK)); advance(&g, 65);
    }
    CHECK(g.chain_hits == 2);
    CHECK(game_leave_training(&g, &saved));
    CHECK(g.chain_hits == 0 && g.chain_last == 0 && g.chain_fade == 0 && g.chain_pop == 0);
}
static void dodging(void) {
    Game g; fixture(&g);
    Vec2 start = g.hero.pos;
    CHECK(g.dodge_charges == DODGE_CHARGES);
    CHECK(game_dodge(&g, (Vec2){0, 3}));
    CHECK(g.hero.dodging && g.hero.invul >= DODGE_INVUL_SECONDS && g.dodge_charges == 1);
    CHECK(!game_dodge(&g, (Vec2){0, 1}));   /* one dash at a time */
    advance(&g, 23);
    CHECK(!g.hero.dodging);
    CHECK(fabsf(g.hero.pos.x - start.x) < .5f && g.hero.pos.y - start.y > 78 && g.hero.pos.y - start.y < 84);
    /* Charges: two, then empty; each refills in turn. */
    CHECK(game_dodge(&g, (Vec2){0, -1})); advance(&g, 23);
    CHECK(g.dodge_charges == 0 && !game_dodge(&g, (Vec2){1, 0}));
    advance(&g, 75);
    CHECK(g.dodge_charges == 1);
    advance(&g, 121);
    CHECK(g.dodge_charges == 2 && g.dodge_recharge == 0);
    /* No direction: step back from the facing. */
    fixture(&g); g.boss.pos.x -= 300; g.hero.dir = (Vec2){1, 0};
    start = g.hero.pos;
    CHECK(game_dodge(&g, (Vec2){0, 0})); advance(&g, 23);
    CHECK(start.x - g.hero.pos.x > 78);
    /* Before impact the swing is lost; a follow-up waits for the dash. */
    fixture(&g);
    CHECK(game_attack(&g)); advance(&g, 5);
    CHECK(game_dodge(&g, (Vec2){0, 1}));
    CHECK(!g.hero.attacking);
    g.attack_cooldown = 0;
    CHECK(game_attack(&g) && g.queued_move == MOVE_ATTACK && !g.hero.attacking);
    advance(&g, 23);
    CHECK(g.hero.attacking && g.training.hits == 0 && g.training.misses == 0);
    /* After impact it cuts only the recovery and keeps the chain alive. */
    fixture(&g);
    CHECK(game_attack(&g)); advance(&g, 20);
    CHECK(g.training.hits == 1 && g.hero.attacking && g.chain_hits == 1);
    CHECK(game_dodge(&g, (Vec2){0, 1}));
    CHECK(!g.hero.attacking && g.chain_hits == 1 && g.combo_time > 0);
    /* Invulnerable for the dash, hurt again afterwards (outside training). */
    legacy_reset(&g); game_start(&g);
    CHECK(game_dodge(&g, (Vec2){1, 0}));
    game_hurt(&g, 10, false);
    CHECK(g.hero.hp == 100);
    advance(&g, 25);
    game_hurt(&g, 10, false);
    CHECK(g.hero.hp < 100);
    /* Not while dead or in a menu. */
    legacy_reset(&g);
    CHECK(!game_dodge(&g, (Vec2){1, 0}) && g.dodge_charges == DODGE_CHARGES);
}
int main(void) {
    sectors(); actual_impacts(); confirmed_chains(); reservations(); multiple_targets(); display_chain();
    dodging();
    printf("PASS: %d directional combat checks.\n", checks);
    return 0;
}
