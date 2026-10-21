#include "test_simulation.h"
#include "game.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks = 0;
#define CHECK(condition)                                                                                     \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(condition)) {                                                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                             \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static void at_npc(Game *g, int npc) {
    g->hero.pos = NPCS[npc].pos;
}
static void clues(Game *g) {
    at_npc(g, ELDER);
    CHECK(game_accept(g));
    for (int i = 0; i < TRACE_COUNT; i++) {
        g->hero.pos = TRACES[i].pos;
        CHECK(game_inspect(g, i));
    }
}
static void recover_items(Game *g) {
    clues(g);
    for (int i = 0; i < BOAR_COUNT; i++) {
        g->hero.pos = g->boars[i].pos;
        CHECK(game_damage(g, &g->boars[i], 99, FX_HIT));
        CHECK(game_collect(g, i));
    }
}
static void ready(Game *g) {
    recover_items(g);
    at_npc(g, HERBALIST);
    CHECK(game_deliver(g));
    at_npc(g, ELDER);
    CHECK(game_report(g));
}
static void quest_chain(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    g.hero.pos = BOSS_HOME;
    simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.quest == Q_MEET);
    CHECK(!game_begin_boss(&g));
    CHECK(!game_report(&g));
    CHECK(!game_inspect(&g, -1));
    CHECK(!game_collect(&g, 100));
    at_npc(&g, ELDER);
    CHECK(game_accept(&g));
    CHECK(!game_accept(&g));
    g.hero.pos = TRACES[0].pos;
    CHECK(game_inspect(&g, 0));
    CHECK(!game_inspect(&g, 0));
    CHECK(g.quest == Q_TRACES);
    g.hero.pos = TRACES[1].pos;
    CHECK(game_inspect(&g, 1));
    CHECK(g.quest == Q_RECOVER);
    for (int i = 0; i < BOAR_COUNT; i++) {
        g.hero.pos = g.boars[i].pos;
        CHECK(game_damage(&g, &g.boars[i], 99, FX_HIT));
        CHECK(!game_damage(&g, &g.boars[i], 99, FX_HIT));
        CHECK(game_collect(&g, i));
        CHECK(!game_collect(&g, i));
    }
    CHECK(g.quest == Q_DELIVER);
    CHECK(g.herbs == 3);
    CHECK(g.coins == 82);
    CHECK(!game_deliver(&g));
    at_npc(&g, HERBALIST);
    CHECK(game_deliver(&g));
    CHECK(!game_deliver(&g));
    CHECK(g.coins == 137 && g.herbs == 0 && g.tonics == 4);
    at_npc(&g, ELDER);
    CHECK(game_report(&g));
    CHECK(!game_report(&g));
    g.hero.pos = BOSS_HOME;
    simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.phase == PH_DIALOGUE);
    CHECK(g.notices & NOTICE_BOSS);
    CHECK(game_begin_boss(&g));
    CHECK(game_damage(&g, &g.boss, 240, FX_HIT));
    CHECK(g.quest == Q_RETURN && g.coins == 207);
    CHECK(!game_damage(&g, &g.boss, 240, FX_HIT));
    g.phase = PH_AUGMENT;
    CHECK(!game_choose(&g, AUG_NONE));
    CHECK(game_choose(&g, AUG_WIND));
    CHECK(g.quest == Q_MESSENGER);
    CHECK(!game_choose(&g, AUG_STEEL));
}
static void shops(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    at_npc(&g, SMITH);
    CHECK(game_open_shop(&g, SMITH));
    CHECK(!game_buy(&g, ITEM_SWORD));
    CHECK(g.coins == 55);
    g.coins = 120;
    CHECK(game_buy(&g, ITEM_SWORD));
    CHECK(g.coins == 60 && g.sword);
    CHECK(!game_buy(&g, ITEM_SWORD));
    CHECK(game_buy(&g, ITEM_ARMOR));
    CHECK(g.coins == 15 && g.armor);
    CHECK(!game_buy(&g, ITEM_TONIC));
    game_close_panel(&g);
    ready(&g);
    CHECK(game_begin_boss(&g));
    g.hero.pos = g.boss.pos;
    g.attack_cooldown = 0;
    CHECK(game_attack(&g));
    simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.boss.hp == 240 && g.hero.attacking);
    for (int i = 0; i < 3; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.boss.hp == 226);
    game_hurt(&g, 18, true);
    CHECK(g.hero.hp == 85 && g.hero.bleed == 5);
}
static void supplies(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    CHECK(!game_use(&g, ITEM_TONIC));
    CHECK(g.tonics == 2);
    CHECK(!game_use(&g, ITEM_BANDAGE));
    game_hurt(&g, 30, true);
    CHECK(game_use(&g, ITEM_BANDAGE));
    CHECK(g.hero.bleed == 0 && g.bandages == 0);
    CHECK(!game_use(&g, ITEM_TONIC));
    for (int i = 0; i < 21; i++)
        simulation_tick(&g, .05f, (Vec2){0});
    CHECK(game_use(&g, ITEM_TONIC));
    CHECK(g.hero.hp == 100 && g.tonics == 1);
    at_npc(&g, HERBALIST);
    CHECK(game_open_shop(&g, HERBALIST));
    int coins = g.coins;
    CHECK(game_buy(&g, ITEM_TONIC));
    CHECK(g.coins == coins - 12 && g.tonics == 2);
    g.tonics = 20;
    CHECK(!game_buy(&g, ITEM_TONIC));
    game_close_panel(&g);
    at_npc(&g, INNKEEPER);
    CHECK(game_open_shop(&g, INNKEEPER));
    CHECK(!game_buy(&g, ITEM_MEAL));
    g.hero.hp = 10;
    g.hero.bleed = 3;
    CHECK(game_buy(&g, ITEM_MEAL));
    CHECK(g.hero.hp == 100 && g.hero.bleed == 0);
    coins = g.coins;
    CHECK(!game_buy(&g, ITEM_MEAL));
    CHECK(g.coins == coins);
    CHECK(!game_use(&g, ITEM_ARMOR));
}
static void death_and_pause(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    clues(&g);
    g.hero.pos = g.boars[0].pos;
    game_damage(&g, &g.boars[0], 100, FX_HIT);
    game_collect(&g, 0);
    g.armor = true;
    game_hurt(&g, 150, false);
    CHECK(g.phase == PH_DEAD);
    game_recover(&g);
    CHECK(g.quest == Q_RECOVER && g.herbs == 1 && g.traces[0] && g.traces[1] && g.armor);
    CHECK(g.boars[0].hp == 0);
    CHECK(!game_collect(&g, 0));
    for (int i = 1; i < BOAR_COUNT; i++) {
        g.hero.pos = g.boars[i].pos;
        game_damage(&g, &g.boars[i], 100, FX_HIT);
        CHECK(game_collect(&g, i));
    }
    at_npc(&g, HERBALIST);
    game_deliver(&g);
    at_npc(&g, ELDER);
    game_report(&g);
    game_begin_boss(&g);
    game_damage(&g, &g.boss, 10, FX_HIT);
    g.hero.invul = 0;
    game_hurt(&g, 150, false);
    game_recover(&g);
    CHECK(g.quest == Q_HUNT && g.boss.hp == 240 && g.supplies_delivered && g.herbs == 0);
    CHECK(distance(g.hero.pos, HOME) < .01f);
    g.phase = PH_BAG;
    Game before = g;
    for (int i = 0; i < 100; i++)
        simulation_tick(&g, .05f, (Vec2){1, 1});
    CHECK(memcmp(&before, &g, sizeof g) == 0);
}
static void augments_and_bounds(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    clues(&g);
    g.hero.pos = g.boars[0].pos;
    CHECK(game_skill(&g));
    CHECK(g.boars[0].hp == 38);
    for(int i=0;i<5;i++) simulation_tick(&g,.05f,(Vec2){0});
    CHECK(g.boars[0].hp == 12);
    CHECK(!game_skill(&g));
    g.replay = true;
    g.augment = AUG_STEEL;
    game_retry(&g);
    g.hero.pos = (Vec2){g.boss.pos.x - 70, g.boss.pos.y};
    g.attack_cooldown = 0;
    CHECK(game_attack(&g));
    simulation_tick(&g, .05f, (Vec2){0});
    for (int i = 0; i < 3; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.boss.hp == 227);
    g.augment = AUG_WIND;
    game_retry(&g);
    g.hero.pos = (Vec2){g.boss.pos.x - 110, g.boss.pos.y};
    g.attack_cooldown = 0;
    CHECK(game_attack(&g));
    simulation_tick(&g, .05f, (Vec2){0});
    for (int i = 0; i < 3; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.boss.hp == 234);
    g.augment = AUG_FLOW;
    game_retry(&g);
    g.hero.pos = (Vec2){g.boss.pos.x - 95, g.boss.pos.y};
    CHECK(game_skill(&g));
    CHECK(g.cooldown == 4 && g.boss.hp == 240);
    for(int i=0;i<5;i++) simulation_tick(&g,.05f,(Vec2){0});
    CHECK(g.boss.hp == 208);
    game_reset(&g);
    game_start(&g);
    g.hero.pos = (Vec2){28, 28};
    simulation_tick(&g, .05f, (Vec2){-1, -1});
    CHECK(g.hero.pos.x == 28 && g.hero.pos.y == 28);
    Vec2 position = g.hero.pos;
    simulation_tick(&g, NAN, (Vec2){1, 1});
    CHECK(distance(position, g.hero.pos) == 0);
    /* The northwest corner is now forest. Check time clamping on open ground. */
    g.hero.pos = HOME;
    position = g.hero.pos;
    simulation_tick(&g, 1, (Vec2){1, 0});
    CHECK(fabsf(g.hero.pos.x - position.x - 7.75f) < .01f);
}
static void dedicated_pickup(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    g.quest = Q_RECOVER;
    g.hero.pos = NPCS[ELDER].pos;
    g.drops[0] = (Drop){.pos = g.hero.pos, .exists = true};
    g.drops[1] = (Drop){.pos = {g.hero.pos.x + 20, g.hero.pos.y}, .exists = true};
    g.drops[2] = (Drop){.pos = {g.hero.pos.x + PICKUP_RADIUS, g.hero.pos.y}, .exists = true};
    CHECK(game_nearby(&g).kind == NEAR_NPC);
    CHECK(game_pickup_count(&g) == 2);
    Vec2 position = g.hero.pos;
    CHECK(game_pickup_nearby(&g) == 2);
    CHECK(g.herbs == 2 && g.quest == Q_RECOVER);
    CHECK(distance(g.hero.pos, position) == 0);
    CHECK(game_pickup_nearby(&g) == 0 && g.herbs == 2);
    g.drops[2].pos = g.hero.pos;
    g.phase = PH_PAUSED;
    CHECK(game_pickup_count(&g) == 0 && game_pickup_nearby(&g) == 0);
    g.phase = PH_PLAY;
    CHECK(game_pickup_nearby(&g) == 1);
    CHECK(g.quest == Q_DELIVER && g.herbs == 3);
    CHECK(game_pickup_nearby(&g) == 0);
    char brief[160];
    game_objective_brief(&g, brief, sizeof brief);
    CHECK(strstr(brief, "약방") != NULL);
    CHECK(g.toast_time == TOAST_SECONDS);

    /* A thin garden gate pillar separates two points inside pickup range. */
    game_reset(&g);
    game_start(&g);
    g.quest = Q_RECOVER;
    g.hero.pos = (Vec2){490 * 1.5f, 259 * 1.5f};
    g.drops[0] = (Drop){.pos = {535 * 1.5f, 259 * 1.5f}, .exists = true};
    CHECK(distance(g.hero.pos, g.drops[0].pos) < PICKUP_RADIUS);
    CHECK(!world_segment(g.hero.pos, g.drops[0].pos, 0));
    CHECK(game_pickup_count(&g) == 0);
    CHECK(game_pickup_nearby(&g) == 0 && g.herbs == 0);
}
static void automatic_magnet(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    g.quest = Q_RECOVER;
    g.hero.pos = HOME;
    for (int i = 0; i < BOAR_COUNT; i++) g.boars[i].hp = 0;
    g.drops[0] = (Drop){.pos = {HOME.x + 120, HOME.y}, .exists = true};
    g.drops[1] = (Drop){.pos = {HOME.x, HOME.y + MAGNET_RADIUS + 1}, .exists = true};
    Vec2 first = g.drops[0].pos, outside = g.drops[1].pos;
    simulation_tick(&g, 0, (Vec2){0, 0});
    CHECK(distance(first, g.drops[0].pos) == 0 && g.herbs == 0);
    for (int i = 0; i < 13; i++) simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(!g.drops[0].attracting && distance(first, g.drops[0].pos) == 0 && g.herbs == 0);
    for (int i = 0; i < 2; i++) simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(g.drops[0].attracting && distance(first, g.drops[0].pos) > 0);
    CHECK(distance(first, g.drops[0].pos) < 12); /* Gradual pickup, not the old instant pull. */
    CHECK(g.herbs == 0 && !g.drops[0].collected); /* Travel is visible, not a teleport. */
    CHECK(distance(outside, g.drops[1].pos) == 0 && !g.drops[1].attracting);
    Vec2 traveling = g.drops[0].pos;
    g.phase = PH_DIALOGUE;
    simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(distance(traveling, g.drops[0].pos) == 0 && g.herbs == 0);
    g.phase = PH_PAUSED;
    simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(distance(traveling, g.drops[0].pos) == 0 && g.herbs == 0);
    g.phase = PH_PLAY;
    for (int i = 0; i < 24; i++) simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(g.herbs == 1 && g.drops[0].collected);
    CHECK(distance(g.hero.pos, HOME) == 0 && g.quest == Q_RECOVER);
    g.drops[1].pos = (Vec2){HOME.x + 80, HOME.y};
    g.drops[2] = (Drop){.pos = {HOME.x + 110, HOME.y}, .exists = true};
    for (int i = 0; i < 24; i++) simulation_tick(&g, .05f, (Vec2){.5f, 0});
    CHECK(g.herbs == 3 && g.quest == Q_DELIVER);
    CHECK(distance(g.hero.pos, HOME) > 0); /* Gathering does not interrupt movement. */
    CHECK(g.drops[1].collected && g.drops[2].collected);
    for (int i = 0; i < 12; i++) simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(g.herbs == 3); /* Exactly once even with overlapping pickups. */

    game_reset(&g);
    game_start(&g);
    g.quest = Q_RECOVER;
    g.hero.pos = (Vec2){490 * 1.5f, 259 * 1.5f};
    g.drops[0] = (Drop){.pos = {535 * 1.5f, 259 * 1.5f}, .exists = true};
    first = g.drops[0].pos;
    for (int i = 0; i < 24; i++) simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(distance(first, g.drops[0].pos) == 0 && !g.drops[0].attracting && g.herbs == 0);
    g.hero.pos = g.drops[0].pos;
    g.quest = Q_TRACES;
    simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(g.herbs == 0); /* Inactive quest items cannot advance the story early. */
    /* Even a drop directly under the hero must first be visible on the ground. */
    game_reset(&g); game_start(&g); g.quest = Q_RECOVER;
    g.drops[0] = (Drop){.pos = g.hero.pos, .exists = true};
    for (int i = 0; i < 13; i++) simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(g.herbs == 0 && !g.drops[0].collected);
    float age = g.drops[0].age;
    g.phase = PH_BAG;
    for (int i = 0; i < 20; i++) simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(g.drops[0].age == age && g.herbs == 0);
    g.phase = PH_PLAY;
    for (int i = 0; i < 10; i++) simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(g.herbs == 1 && g.drops[0].collected);

}
static void attack_motion(void) {
    Game g;
    game_reset(&g); game_start(&g); clues(&g);
    g.hero.pos = (Vec2){g.boars[0].pos.x + 40, g.boars[0].pos.y};
    g.boars[0].state = AI_RECOVER; g.boars[0].timer = 9;
    g.attack_cooldown = 0;
    CHECK(game_attack(&g));
    simulation_tick(&g, .01f, (Vec2){0});
    CHECK(game_hero_attack_frame(&g) == 0 && g.boars[0].hp == 38);
    CHECK(g.hero.attack_face == -1);
    simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.boars[0].hp == 38);
    float elapsed = g.hero.attack_elapsed;
    g.phase = PH_BAG;
    for (int i = 0; i < 20; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.hero.attack_elapsed == elapsed && g.boars[0].hp == 38);
    CHECK(game_hero_attack_frame(&g) == -1);
    g.phase = PH_PLAY;
    unsigned seen = 1u | (1u << game_hero_attack_frame(&g));
    for (int i = 0; i < 20; i++) {
        simulation_tick(&g, .02f, (Vec2){0});
        int frame = game_hero_attack_frame(&g);
        if (frame >= 0) seen |= 1u << frame;
        if (g.hero.attacking && g.hero.attack_elapsed < HERO_ATTACK_IMPACT)
            CHECK(g.boars[0].hp == 38);
    }
    CHECK(seen == (1u << HERO_ATTACK_FRAMES) - 1);
    CHECK(g.boars[0].hp == 29); /* One impact despite several strike/recovery frames. */
    CHECK(!g.hero.attacking && game_hero_attack_frame(&g) == -1);

    /* Moving out of range during windup evades the committed target. */
    game_reset(&g); game_start(&g); clues(&g);
    g.hero.pos = (Vec2){g.boars[0].pos.x - 40, g.boars[0].pos.y};
    g.attack_cooldown = 0;
    CHECK(game_attack(&g));
    simulation_tick(&g, .01f, (Vec2){0});
    CHECK(g.hero.attacking && g.hero.attack_face == 1);
    g.hero.pos = HOME;
    for (int i = 0; i < 4; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.boars[0].hp == 38 && g.hero.attack_hit);
    game_recover(&g);
    CHECK(!g.hero.attacking && g.hero.attack_elapsed == 0);
}
static void walking_motion(void) {
    Game g, slow;
    game_reset(&g);
    CHECK(game_hero_frame(&g) == -1);
    game_start(&g);
    simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(game_hero_frame(&g) == -1 && g.hero.walk_cycle == 0);
    Vec2 start = g.hero.pos;
    simulation_tick(&g, .05f, (Vec2){1, 0});
    CHECK(g.hero.pos.x > start.x && game_hero_frame(&g) >= 0);
    CHECK(g.hero.face == 1);
    float first = g.hero.walk_cycle;
    simulation_tick(&g, .05f, (Vec2){-1, 0});
    CHECK(g.hero.face == -1 && g.hero.walk_cycle > first);
    simulation_tick(&g, .05f, (Vec2){0, -1});
    CHECK(g.hero.face == -1 && game_hero_frame(&g) >= 0);
    simulation_tick(&g, .05f, (Vec2){0, 0});
    CHECK(game_hero_frame(&g) == -1 && g.hero.walk_cycle == 0);

    /* Same travel at different render rates and joystick strengths. */
    game_reset(&g); game_start(&g); slow = g;
    g.hero.pos = slow.hero.pos = (Vec2){345, 510};
    for (int i = 0; i < 10; i++) simulation_tick(&g, .05f, (Vec2){1, 0});
    for (int i = 0; i < 50; i++) simulation_tick(&slow, .01f, (Vec2){1, 0});
    CHECK(distance(g.hero.pos, slow.hero.pos) < .01f);
    CHECK(fabsf(g.hero.walk_cycle-slow.hero.walk_cycle) < .0001f);
    game_reset(&g); game_start(&g); slow = g;
    simulation_tick(&g, .05f, (Vec2){1, 0});
    simulation_tick(&slow, .05f, (Vec2){.25f, 0});
    CHECK(fabsf(g.hero.walk_cycle-4*slow.hero.walk_cycle) < .0001f);

    /* All eight poses occur while moving through an unobstructed street. */
    game_reset(&g); game_start(&g); g.hero.pos = (Vec2){345, 510};
    unsigned seen = 0;
    for (int i = 0; i < 100; i++) {
        simulation_tick(&g, 1.0f/120, (Vec2){i < 50 ? 1 : -1, 0});
        int frame = game_hero_frame(&g);
        CHECK(frame >= 0 && frame < HERO_WALK_FRAMES);
        seen |= 1u << frame;
    }
    CHECK(seen == (1u << HERO_WALK_FRAMES)-1);

    /* Collision must stop the feet even with input still held. */
    game_reset(&g); game_start(&g); g.hero.pos = (Vec2){270, 525};
    for (int i = 0; i < 150; i++) simulation_tick(&g, .05f, (Vec2){0, -1});
    CHECK(world_clear(g.hero.pos, HERO_RADIUS));
    CHECK(game_hero_frame(&g) == -1 && g.hero.walk_cycle == 0);
    start = g.hero.pos;
    simulation_tick(&g, .05f, (Vec2){1, -1});
    CHECK(g.hero.pos.x > start.x && game_hero_frame(&g) >= 0);

    const Phase panels[] = {PH_TITLE, PH_PROLOGUE, PH_DIALOGUE, PH_SHOP, PH_BAG,
                            PH_JOURNAL, PH_AUGMENT, PH_COMPLETE, PH_DEAD, PH_PAUSED, PH_TECHNIQUES};
    for (size_t i = 0; i < sizeof panels/sizeof *panels; i++) {
        game_reset(&g); game_start(&g);
        simulation_tick(&g, .05f, (Vec2){1, 0});
        g.phase = panels[i];
        CHECK(game_hero_frame(&g) == -1);
        start = g.hero.pos;
        simulation_tick(&g, .05f, (Vec2){1, 0});
        CHECK(distance(start, g.hero.pos) == 0 && g.hero.walk_cycle == 0);
    }
    g.phase = PH_PLAY;
    simulation_tick(&g, .05f, (Vec2){1, 0});
    game_recover(&g);
    CHECK(game_hero_frame(&g) == -1 && distance(g.hero.pos, HOME) == 0);
    simulation_tick(&g, .05f, (Vec2){1, 0});
    game_retry(&g);
    CHECK(game_hero_frame(&g) == -1 && g.hero.walk_cycle == 0);
}
static void advance(Game *g, int ticks) {
    for(int i=0;i<ticks;i++) simulation_tick(g,.05f,(Vec2){0});
}
static void combat_fixture(Game *g) {
    game_reset(g); game_start(g); g->quest=Q_COMBAT;
    g->hero.pos=HOME; g->hero.invul=99;
    g->boss.pos=(Vec2){HOME.x+40,HOME.y}; g->boss.hp=240;
    g->boss.home=g->boss.pos; g->boss.state=AI_RECOVER; g->boss.timer=99;
}
static void equipment_and_manual_combat(void) {
    Game g; combat_fixture(&g);
    advance(&g,30);
    CHECK(g.boss.hp==240 && !g.hero.attacking); /* Proximity never starts an attack. */
    CHECK(game_attack(&g)); advance(&g,3);
    CHECK(g.boss.hp==231); advance(&g,10); CHECK(g.boss.hp==231);
    g.phase=PH_BAG;
    CHECK(!game_equip(&g,ITEM_SWORD,true));
    g.owned_sword=g.owned_armor=true;
    CHECK(game_equip(&g,ITEM_SWORD,true) && g.sword);
    CHECK(game_equip(&g,ITEM_SWORD,false) && !g.sword && g.owned_sword);
    CHECK(!game_equip(&g,ITEM_TONIC,true));
    CHECK(game_equip(&g,ITEM_ARMOR,true) && g.armor);
    g.phase=PH_SHOP; g.shop=SMITH; g.coins=300;
    CHECK(!game_buy(&g,ITEM_SWORD) && g.coins==300); /* Owned gear cannot be re-bought. */
    CHECK(!game_equip(&g,ITEM_SWORD,true));
    g.phase=PH_BAG; CHECK(game_equip(&g,ITEM_SWORD,true));
    game_close_panel(&g); advance(&g,30);
    CHECK(game_attack(&g)); advance(&g,3); CHECK(g.boss.hp==217);
    game_reset(&g); game_start(&g); CHECK(game_attack(&g));
    CHECK(g.hero.attacking); advance(&g,10); CHECK(!g.hero.attacking);
}
static void techniques_and_combos(void) {
    Game g;
    for(int t=0;t<TECH_COUNT;t++) {
        combat_fixture(&g);
        CHECK(game_technique(&g,(Technique)t));
        CHECK(!game_technique(&g,(Technique)t));
        advance(&g,4); CHECK(g.boss.hp==240 && !g.hero.skill_hit);
        float elapsed=g.hero.skill_elapsed;
        g.phase=PH_TECHNIQUES; advance(&g,10);
        CHECK(g.hero.skill_elapsed==elapsed && game_hero_skill_frame(&g)==-1);
        game_close_panel(&g); advance(&g,1);
        CHECK(g.boss.hp==240-TECHNIQUES[t].damage && g.hero.skill_hit);
        int hp=g.boss.hp; advance(&g,10);
        CHECK(g.boss.hp==hp && !g.hero.skilling);
        CHECK(!game_technique(&g,(Technique)t));
    }
    combat_fixture(&g);
    CHECK(!game_bind_swipe(&g,SWIPE_UP,TECH_THRUST));
    g.phase=PH_TECHNIQUES;
    CHECK(game_bind_swipe(&g,SWIPE_UP,TECH_THRUST));
    CHECK(g.swipe_bindings[SWIPE_UP]==TECH_THRUST && g.swipe_bindings[SWIPE_LEFT]==TECH_CLEAVE);
    for(int d=0;d<SWIPE_COUNT;d++) for(int t=0;t<TECH_COUNT;t++) {
        CHECK(game_bind_swipe(&g,(Swipe)d,(Technique)t));
        unsigned assigned=0;
        for(int i=0;i<SWIPE_COUNT;i++) assigned|=1u<<g.swipe_bindings[i];
        CHECK(assigned==(1u<<TECH_COUNT)-1);
    }
    CHECK(!game_bind_swipe(&g,(Swipe)-1,TECH_CLEAVE));
    CHECK(!game_bind_swipe(&g,SWIPE_UP,(Technique)TECH_COUNT));
    game_close_panel(&g);
    CHECK(!game_technique(&g,(Technique)-1));
    CHECK(!game_technique(&g,(Technique)TECH_COUNT));
    combat_fixture(&g); CHECK(game_attack(&g));
    CHECK(game_technique(&g,TECH_SWEEP) && g.queued_move==TECH_SWEEP);
    /* Early input waits for the previous attack's impact and recovery. */
    advance(&g,5); CHECK(g.boss.hp==231 && g.hero.attacking);
    CHECK(game_technique(&g,TECH_SWEEP) && g.queued_move==TECH_SWEEP);
    advance(&g,4); CHECK(g.hero.skilling && g.hero.technique==TECH_SWEEP && g.input_count==2);
    advance(&g,9); CHECK(g.boss.hp==218); /* 12 * 1.1 rounds to 13. */
    CHECK(game_technique(&g,TECH_CLEAVE)); advance(&g,4);
    CHECK(g.hero.technique==TECH_CLEAVE && g.input_count==3);
    advance(&g,5); CHECK(g.boss.hp==187); /* 26 * 1.2 rounds to 31. */
    CHECK(!game_technique(&g,TECH_CLEAVE));
    advance(&g,35); CHECK(g.combo_time==0);
    CHECK(game_technique(&g,TECH_THRUST) && g.input_count==1);
    combat_fixture(&g); CHECK(game_attack(&g)); advance(&g,5);
    CHECK(game_technique(&g,TECH_SWEEP));
    g.phase=PH_BAG; advance(&g,1);
    CHECK(g.queued_move==MOVE_NONE); game_close_panel(&g); advance(&g,10);
    CHECK(!g.hero.skilling && g.boss.hp==231);
    combat_fixture(&g); CHECK(game_attack(&g)); advance(&g,16);
    CHECK(g.combo_time>0 && game_attack(&g) && g.input_count==1); /* A repeat does not stack. */
}
/* All twelve ordered transitions between the four directions must chain.
 * An early second stroke must not depend on the old last-0.22s window. */
static void all_directional_pair_combos(void) {
    Game g;
    for (int first = 0; first < TECH_COUNT; first++) {
        for (int next = 0; next < TECH_COUNT; next++) {
            if (first == next) continue;
            combat_fixture(&g);
            CHECK(game_technique(&g, (Technique)first));
            advance(&g, 1);
            CHECK(game_technique(&g, (Technique)next));
            CHECK(g.queued_move == next && g.buffer_time > .50f);
            advance(&g, 13);
            CHECK(g.hero.skilling && g.hero.technique == (Technique)next);
            CHECK(g.input_count == 2 && g.queued_move == MOVE_NONE);
            advance(&g, 5);
            CHECK(g.hero.skill_hit); /* Only the second move's timed impact. */
        }
    }
}
static void continuous_input_buffer(void) {
    Game g; combat_fixture(&g);
    CHECK(game_technique(&g,TECH_CLEAVE));
    advance(&g,8); /* An early next direction is deliberately buffered. */
    CHECK(game_technique(&g,TECH_RETREAT) && g.queued_move==TECH_RETREAT);
    CHECK(g.buffer_time>.24f);
    advance(&g,1);
    CHECK(g.queued_move==TECH_RETREAT);
    CHECK(game_technique_cooldown(&g,TECH_RETREAT)==0); /* Queuing is not execution. */
    CHECK(game_technique(&g,TECH_THRUST) && g.queued_move==TECH_THRUST);
    float lifetime=g.buffer_time;
    CHECK(!game_technique(&g,TECH_CLEAVE));
    CHECK(g.queued_move==TECH_THRUST && g.buffer_time==lifetime);
    advance(&g,4);
    CHECK(g.hero.skilling && g.hero.technique==TECH_THRUST && g.hero.skill_elapsed==0);
    CHECK(g.queued_move==MOVE_NONE && g.buffer_time==0 && g.input_count==2);
    CHECK(game_technique_cooldown(&g,TECH_RETREAT)==0);
    int hp=g.boss.hp;
    advance(&g,4); CHECK(g.boss.hp==hp); /* Buffered move still has full windup. */
    advance(&g,1); CHECK(g.boss.hp<hp);
    hp=g.boss.hp; advance(&g,10); CHECK(g.boss.hp==hp);
    advance(&g,110);
    CHECK(!g.hero.skilling && !g.hero.attacking && g.queued_move==MOVE_NONE);

    /* Keyboard/basic and directional requests share one buffer; no hidden queue. */
    combat_fixture(&g); CHECK(game_technique(&g,TECH_CLEAVE)); advance(&g,9);
    CHECK(game_technique(&g,TECH_SWEEP));
    CHECK(game_attack(&g) && g.queued_move==MOVE_ATTACK);
    CHECK(game_technique(&g,TECH_RETREAT) && g.queued_move==TECH_RETREAT);
    CHECK(game_attack(&g) && g.queued_move==MOVE_ATTACK);
    advance(&g,4);
    CHECK(g.hero.attacking && !g.hero.skilling && g.input_count==2);
    CHECK(game_technique_cooldown(&g,TECH_RETREAT)==0 && game_technique_cooldown(&g,TECH_SWEEP)==0);

    const Phase panels[]={PH_BAG,PH_DIALOGUE,PH_PAUSED,PH_TECHNIQUES,PH_JOURNAL};
    for(size_t i=0;i<sizeof panels/sizeof *panels;i++) {
        combat_fixture(&g); CHECK(game_technique(&g,TECH_CLEAVE)); advance(&g,9);
        CHECK(game_technique(&g,TECH_RETREAT));
        g.phase=panels[i]; advance(&g,1);
        CHECK(g.queued_move==MOVE_NONE && g.buffer_time==0);
        g.phase=PH_PLAY; advance(&g,20);
        CHECK(!g.hero.skilling && game_technique_cooldown(&g,TECH_RETREAT)==0);
    }
}
/* The windup lands strictly inside BOAR_STRIKE_RANGE, the same reach the
 * warning circle draws. Probe the boundary on open ground, no armour. */
static void boar_strike_reach(void) {
    const float distances[] = {63.9f, 64.0f, 70.0f, 72.9f, 73.0f, 73.5f};
    for (size_t i = 0; i < sizeof distances / sizeof *distances; i++) {
        Game g;
        game_reset(&g);
        game_start(&g);
        clues(&g);
        Enemy *boar = &g.boars[0];
        g.hero.pos = (Vec2){boar->pos.x, boar->pos.y + distances[i]};
        CHECK(world_segment(boar->pos, g.hero.pos, 0));
        boar->state = AI_WINDUP;
        boar->timer = .001f;
        g.boars[1].state = g.boars[2].state = AI_RETURN;
        game_tick(&g, .01f, (Vec2){0, 0});
        CHECK(boar->state == AI_RECOVER);
        CHECK((g.hero.hp == 91) == (distances[i] < BOAR_STRIKE_RANGE));
        CHECK(g.hero.hp == 91 || g.hero.hp == 100);
    }
}
/* Presentation state: chasing enemies accumulate walked distance, and a
 * defeated enemy keeps a short fall that menus pause and hitstop holds. */
static void enemy_motion_and_falls(void) {
    Game g; game_reset(&g); game_start(&g);
    g.traces[0] = g.traces[1] = true; g.quest = Q_RECOVER;
    Enemy *e = &g.boars[0];
    g.hero.pos = (Vec2){e->pos.x - 120, e->pos.y};
    e->state = AI_CHASE; e->timer = 5;
    Vec2 start = e->pos;
    for (int i = 0; i < 10; i++) game_tick(&g, .05f, (Vec2){0, 0});
    CHECK(e->stride > 1 && fabsf(e->stride - distance(start, e->pos)) < .5f);
    g.hero.pos = (Vec2){e->pos.x - 30, e->pos.y};
    CHECK(game_damage(&g, e, e->hp, FX_HIT));
    CHECK(e->hp == 0 && e->dying == ENEMY_DEATH_SECONDS);
    float held = e->dying;
    game_tick(&g, .02f, (Vec2){0, 0}); /* the kill's hitstop */
    CHECK(e->dying == held);
    g.hitstop_time = 0;
    g.phase = PH_BAG;
    game_tick(&g, .05f, (Vec2){0, 0});
    CHECK(e->dying == held);
    g.phase = PH_PLAY;
    for (int i = 0; i < 6; i++) game_tick(&g, .05f, (Vec2){0, 0});
    CHECK(e->dying > 0 && e->dying < held);
    for (int i = 0; i < 10; i++) game_tick(&g, .05f, (Vec2){0, 0});
    CHECK(e->dying == 0);
}
int main(void) {
    equipment_and_manual_combat();
    techniques_and_combos();
    all_directional_pair_combos();
    continuous_input_buffer();
    quest_chain();
    shops();
    supplies();
    death_and_pause();
    augments_and_bounds();
    dedicated_pickup();
    automatic_magnet();
    walking_motion();
    enemy_motion_and_falls();
    attack_motion();
    boar_strike_reach();
    printf(
        "PASS: %d checks across quests, transactions, consumables, recovery, pause, four techniques, combos and bounds.\n",
        checks);
    return 0;
}
