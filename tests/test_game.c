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
    game_tick(&g, .05f, (Vec2){0});
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
    game_tick(&g, .05f, (Vec2){0});
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
    g.auto_timer = 0;
    game_tick(&g, .05f, (Vec2){0});
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
        game_tick(&g, .05f, (Vec2){0});
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
        game_tick(&g, .05f, (Vec2){1, 1});
    CHECK(memcmp(&before, &g, sizeof g) == 0);
}
static void augments_and_bounds(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    clues(&g);
    g.hero.pos = g.boars[0].pos;
    CHECK(game_skill(&g));
    CHECK(g.boars[0].hp == 12);
    CHECK(!game_skill(&g));
    g.replay = true;
    g.augment = AUG_STEEL;
    game_retry(&g);
    g.hero.pos = (Vec2){g.boss.pos.x - 70, g.boss.pos.y};
    g.auto_timer = 0;
    game_tick(&g, .05f, (Vec2){0});
    CHECK(g.boss.hp == 227);
    g.augment = AUG_WIND;
    game_retry(&g);
    g.hero.pos = (Vec2){g.boss.pos.x - 110, g.boss.pos.y};
    g.auto_timer = 0;
    game_tick(&g, .05f, (Vec2){0});
    CHECK(g.boss.hp == 234);
    g.augment = AUG_FLOW;
    game_retry(&g);
    g.hero.pos = (Vec2){g.boss.pos.x - 95, g.boss.pos.y};
    CHECK(game_skill(&g));
    CHECK(g.cooldown == 4 && g.boss.hp == 208);
    game_reset(&g);
    game_start(&g);
    g.hero.pos = (Vec2){28, 28};
    game_tick(&g, .05f, (Vec2){-1, -1});
    CHECK(g.hero.pos.x == 28 && g.hero.pos.y == 28);
    Vec2 position = g.hero.pos;
    game_tick(&g, NAN, (Vec2){1, 1});
    CHECK(distance(position, g.hero.pos) == 0);
    /* The northwest corner is now forest. Check time clamping on open ground. */
    g.hero.pos = HOME;
    position = g.hero.pos;
    game_tick(&g, 1, (Vec2){1, 0});
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
int main(void) {
    quest_chain();
    shops();
    supplies();
    death_and_pause();
    augments_and_bounds();
    dedicated_pickup();
    printf(
        "PASS: %d checks across quests, transactions, consumables, recovery, pause, augments and bounds.\n",
        checks);
    return 0;
}
