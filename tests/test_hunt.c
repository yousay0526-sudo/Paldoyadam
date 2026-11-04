/* Repeat hunting: ordinary boars on the pass, kept apart from the quest. */
#include "save.h"
#include "world.h"
#include "test_simulation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL hunt:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
static unsigned char bytes[SAVE_CAPACITY];
static size_t size;

static Game roundtrip(const Game *g) {
    Game loaded;
    CHECK(save_encode(g, bytes, &size) == SAVE_OK);
    CHECK(size == SAVE_V3_SIZE);
    CHECK(save_decode(&loaded, bytes, size) == SAVE_OK);
    return loaded;
}
static int alive(const Game *g) {
    int n = 0;
    for (int i = 0; i < HUNT_COUNT; i++) n += g->hunt[i].hp > 0;
    return n;
}
/* Quest accepted and both traces read: quest and ordinary boars are out. */
static void ready(Game *g) {
    game_reset(g); game_start(g);
    g->hero.pos = NPCS[ELDER].pos; CHECK(game_accept(g));
    g->hero.pos = MAP_EXITS[0].pos; CHECK(game_travel(g, 0));
    for (int i = 0; i < TRACE_COUNT; i++) { g->hero.pos = TRACES[i].pos; CHECK(game_inspect(g, i)); }
    CHECK(g->quest == Q_RECOVER && game_hunt_active(g));
    g->hero.pos = MAP_EXITS[0].arrival;
}
static void to_village(Game *g) { g->hero.pos = MAP_EXITS[2].pos; CHECK(game_travel(g, 2)); }
static void to_pass(Game *g) { g->hero.pos = MAP_EXITS[0].pos; CHECK(game_travel(g, 0)); }

static void spawns(void) {
    world_select(MAP_PASS);
    const Vec2 quest_homes[BOAR_COUNT] = {{500 * 1.5f, 380 * 1.5f}, {1050 * 1.5f, 400 * 1.5f}, {790 * 1.5f, 315 * 1.5f}};
    for (int i = 0; i < HUNT_COUNT; i++) {
        Vec2 p = HUNT_SPAWNS[i];
        Route r;
        /* Room around the body, reachable on foot, and out of sight of the
         * arrival, the exit and the quest scenes. */
        CHECK(world_clear(p, BOAR_RADIUS + 24));
        CHECK(world_route(MAP_EXITS[0].arrival, p, BOAR_RADIUS, &r) && r.count > 0);
        CHECK(distance(r.points[r.count - 1], p) < 2);
        CHECK(distance(p, MAP_EXITS[0].arrival) >= 330 && distance(p, MAP_EXITS[2].pos) >= 330);
        for (int q = 0; q < BOAR_COUNT; q++) CHECK(distance(p, quest_homes[q]) >= 300);
        for (int t = 0; t < TRACE_COUNT; t++) CHECK(distance(p, TRACES[t].pos) >= 300);
        for (int j = 0; j < i; j++) CHECK(distance(p, HUNT_SPAWNS[j]) >= 300);
    }
    Game g; game_reset(&g);
    for (int i = 0; i < BOAR_COUNT; i++) CHECK(distance(g.boars[i].home, quest_homes[i]) < .01f);
    CHECK(g.hunt_round == 1 && alive(&g) == HUNT_COUNT);
    for (int i = 0; i < HUNT_COUNT; i++)
        CHECK(g.hunt[i].hp == BOAR_HP && g.hunt[i].max_hp == BOAR_HP && distance(g.hunt[i].pos, HUNT_SPAWNS[i]) == 0);
}

static void where_they_appear(void) {
    Game g; game_reset(&g); game_start(&g);
    /* Gated by the traces, like the quest boars. */
    for (int m = 0; m < MAP_COUNT; m++) { g.map = (MapId)m; CHECK(!game_hunt_active(&g)); }
    g.map = MAP_PASS; g.hero.pos = HUNT_SPAWNS[0];
    CHECK(!game_damage(&g, &g.hunt[0], 99, FX_HIT) && g.hunt[0].hp == BOAR_HP);
    g.traces[0] = true; CHECK(!game_hunt_active(&g));
    g.traces[1] = true; CHECK(game_hunt_active(&g));
    /* Never in the village, the cave, the legacy single-map terrain or training. */
    for (int m = 0; m < MAP_COUNT; m++) {
        g.map = (MapId)m;
        CHECK(game_hunt_active(&g) == (m == MAP_PASS));
        if (m != MAP_PASS) { g.hero.pos = g.hunt[0].pos; CHECK(!game_damage(&g, &g.hunt[0], 99, FX_HIT)); }
    }
    CHECK(g.coins == 55 && g.hunt[0].hp == BOAR_HP);
    g.map = MAP_VILLAGE; g.hero.pos = HOME;
    Game saved; CHECK(game_enter_training(&g, &saved));
    CHECK(!game_hunt_active(&g));
    for (int i = 0; i < 40; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(game_leave_training(&g, &saved));
    /* Before the traces the pass is quiet: standing still, nobody comes. */
    game_reset(&g); game_start(&g); to_pass(&g);
    for (int i = 0; i < 100; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.hero.hp == 100);
    for (int i = 0; i < HUNT_COUNT; i++) CHECK(distance(g.hunt[i].pos, HUNT_SPAWNS[i]) == 0);
}

static void rewards_apart_from_quest(void) {
    Game g; ready(&g);
    int coins = g.coins;
    for (int i = 0; i < HUNT_COUNT; i++) {
        g.hero.pos = g.hunt[i].pos;
        CHECK(game_damage(&g, &g.hunt[i], 99, FX_HIT));
        CHECK(g.hunt[i].hp == 0 && g.coins == coins + HUNT_REWARD * (i + 1));
        CHECK(!game_damage(&g, &g.hunt[i], 99, FX_HIT));
        CHECK(!strcmp(g.toast, "멧돼지 제압 +9문"));
    }
    /* No herb, no drop, no quest step, nothing to pick up. */
    CHECK(g.quest == Q_RECOVER && g.herbs == 0 && game_pickup_count(&g) == 0);
    for (int i = 0; i < BOAR_COUNT; i++) CHECK(!g.drops[i].exists && g.boars[i].hp == BOAR_HP);
    char text[128]; game_objective_brief(&g, text, sizeof text); CHECK(strstr(text, "0/3"));
    /* The falls play out, then nothing is left to draw or tick. */
    for (int i = 0; i < 20; i++) simulation_tick(&g, .05f, (Vec2){0});
    for (int i = 0; i < HUNT_COUNT; i++) CHECK(g.hunt[i].dying == 0 && g.hunt[i].state == AI_DEAD);
    /* A quest boar still drops its herb; an ordinary kill never pays a herb. */
    g.hero.pos = g.boars[0].pos; CHECK(game_damage(&g, &g.boars[0], 99, FX_HIT));
    CHECK(g.drops[0].exists && g.coins == coins + HUNT_REWARD * (HUNT_COUNT + 1));
}

/* Real swings and techniques pick ordinary boars as targets. */
static void real_combat(void) {
    Game g; ready(&g);
    Enemy *e = &g.hunt[2];
    g.hero.pos = (Vec2){e->pos.x - 40, e->pos.y}; g.hero.dir = (Vec2){1, 0};
    CHECK(game_attack(&g));
    CHECK(g.hero.attack_target == BOAR_COUNT + 2);
    for (int i = 0; i < 10; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(e->hp == BOAR_HP - 9 && g.last_impact == IMPACT_HIT);
    for (int i = 0; i < 20; i++) simulation_tick(&g, .05f, (Vec2){0});
    g.hero.pos = (Vec2){e->pos.x - 40, e->pos.y};
    CHECK(game_technique(&g, TECH_SWEEP));
    for (int i = 0; i < 20; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(e->hp < BOAR_HP - 9);
    /* Body collision pushes an ordinary boar off the hero. */
    g.hero.invul = 1000; g.hero.pos = g.hunt[1].pos; g.hero.pos.x += 3;
    for (int i = 0; i < 10; i++) simulation_tick(&g, .02f, (Vec2){0});
    CHECK(distance(g.hero.pos, g.hunt[1].pos) > HERO_RADIUS + BOAR_RADIUS - 2);
}

/* Quest and ordinary boars together never wind up more than two at once. */
static void shared_attack_slots(void) {
    Game g; ready(&g);
    Vec2 c = HUNT_SPAWNS[3];
    g.hero.pos = c; g.hero.invul = 0; g.hero.hp = 100;
    for (int i = 0; i < BOAR_COUNT; i++)
        g.boars[i].pos = g.boars[i].home = (Vec2){c.x - 60 + 40 * i, c.y - 70};
    for (int i = 0; i < HUNT_COUNT; i++)
        g.hunt[i].pos = g.hunt[i].home = (Vec2){c.x - 70 + 45 * i, c.y + 70};
    int most = 0, both = 0;
    for (int t = 0; t < 600 && g.phase == PH_PLAY; t++) {
        g.hero.hp = 100; /* Keep the fight going; the slot limit is under test. */
        simulation_tick(&g, .02f, (Vec2){0});
        int quest = 0, ordinary = 0;
        for (int i = 0; i < BOAR_COUNT; i++)
            quest += g.boars[i].hp > 0 && (g.boars[i].state == AI_WINDUP || g.boars[i].state == AI_SWING);
        for (int i = 0; i < HUNT_COUNT; i++)
            ordinary += g.hunt[i].hp > 0 && (g.hunt[i].state == AI_WINDUP || g.hunt[i].state == AI_SWING);
        if (quest + ordinary > most) most = quest + ordinary;
        both |= quest > 0 && ordinary > 0;
        CHECK(quest + ordinary <= BOAR_ATTACK_TOKENS);
    }
    CHECK(most == BOAR_ATTACK_TOKENS);
    CHECK(both); /* Quest and ordinary boars did share the slots. */
}

static void rounds(void) {
    Game g; ready(&g);
    g.hero.pos = g.hunt[0].pos; CHECK(game_damage(&g, &g.hunt[0], 99, FX_HIT));
    g.hero.pos = g.hunt[1].pos; CHECK(game_damage(&g, &g.hunt[1], 20, FX_HIT));
    g.hero.pos = g.boars[0].pos; CHECK(game_damage(&g, &g.boars[0], 99, FX_HIT));
    g.hero.pos = g.drops[0].pos; CHECK(game_collect(&g, 0));
    g.hunt[2].pos.x += 30; /* Wandered off. */
    int coins = g.coins; unsigned round = g.hunt_round;
    /* Same visit: pausing, menus and a short stay bring nobody back early. */
    const Phase menus[] = {PH_PAUSED, PH_BAG, PH_JOURNAL, PH_TECHNIQUES, PH_DIALOGUE};
    for (unsigned m = 0; m < sizeof menus / sizeof *menus; m++) {
        g.phase = menus[m];
        for (int i = 0; i < 10; i++) simulation_tick(&g, .05f, (Vec2){0});
        g.phase = PH_PLAY;
    }
    for (int i = 0; i < 40; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.hunt_round == round && g.hunt[0].hp == 0 && g.hunt[1].hp == BOAR_HP - 20);
    /* Training and back: same round, same health. */
    Game saved; g.hero.pos = MAP_EXITS[0].arrival;
    CHECK(game_enter_training(&g, &saved));
    CHECK(game_leave_training(&g, &saved));
    CHECK(g.hunt_round == round && g.hunt[0].hp == 0 && g.hunt[1].hp == BOAR_HP - 20 && g.coins == coins);
    /* Village and back: a fresh round of ordinary boars only. */
    to_village(&g);
    CHECK(g.hunt_round == round && !game_hunt_active(&g));
    to_pass(&g);
    CHECK(g.hunt_round == round + 1 && alive(&g) == HUNT_COUNT && g.coins == coins);
    for (int i = 0; i < HUNT_COUNT; i++)
        CHECK(g.hunt[i].hp == BOAR_HP && g.hunt[i].state == AI_IDLE && distance(g.hunt[i].pos, HUNT_SPAWNS[i]) == 0);
    CHECK(g.boars[0].hp == 0 && g.drops[0].collected && g.herbs == 1 && g.quest == Q_RECOVER);
    CHECK(g.boars[1].hp == BOAR_HP && !g.drops[1].exists);
    /* Arriving is safe: no ordinary boar notices the hero at the entrance. */
    for (int i = 0; i < 60; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.hero.hp == 100);
    for (int i = 0; i < HUNT_COUNT; i++) CHECK(g.hunt[i].state == AI_IDLE);
    /* The cave is another trip from the village: it never resets the pass. */
    to_village(&g); g.hero.pos = MAP_EXITS[1].pos; CHECK(game_travel(&g, 1));
    g.hero.pos = MAP_EXITS[3].pos; CHECK(game_travel(&g, 3));
    CHECK(g.hunt_round == round + 1);
}

static void saving(void) {
    Game g; ready(&g);
    g.hero.pos = g.hunt[0].pos; CHECK(game_damage(&g, &g.hunt[0], 99, FX_HIT));
    g.hero.pos = g.hunt[3].pos; CHECK(game_damage(&g, &g.hunt[3], 11, FX_HIT));
    g.hunt[3].pos.y -= 25; g.hunt[3].state = AI_CHASE;
    unsigned round = g.hunt_round; int coins = g.coins;
    /* Loading alone never refills the boars or pays again, however often. */
    Game loaded = g;
    for (int n = 0; n < 5; n++) loaded = roundtrip(&loaded);
    CHECK(loaded.hunt_round == round && loaded.coins == coins && loaded.map == MAP_PASS);
    CHECK(loaded.hunt[0].hp == 0 && loaded.hunt[0].state == AI_DEAD);
    CHECK(loaded.hunt[3].hp == BOAR_HP - 11 && loaded.hunt[3].state == AI_IDLE);
    CHECK(distance(loaded.hunt[3].pos, HUNT_SPAWNS[3]) == 0);
    CHECK(!game_damage(&loaded, &loaded.hunt[0], 99, FX_HIT) && loaded.coins == coins);
    CHECK(game_damage(&loaded, &loaded.hunt[3], 99, FX_HIT) && loaded.coins == coins + HUNT_REWARD);
    /* Saved in the village: the next entry is a new round, the load is not. */
    to_village(&g); loaded = roundtrip(&g);
    CHECK(loaded.hunt_round == round && loaded.hunt[0].hp == 0);
    to_pass(&loaded); CHECK(loaded.hunt_round == round + 1 && alive(&loaded) == HUNT_COUNT);
    /* Training never writes; its own state cannot reach the file. */
    Game saved; CHECK(game_enter_training(&g, &saved));
    CHECK(save_encode(&g, bytes, &size) == SAVE_INVALID);
    CHECK(game_leave_training(&g, &saved));
    /* Out-of-range ordinary boars reject the whole record. */
    Game bad = g; bad.hunt[1].hp = BOAR_HP + 1; CHECK(save_encode(&bad, bytes, &size) == SAVE_INVALID);
    bad = g; bad.hunt[1].hp = -1; CHECK(save_encode(&bad, bytes, &size) == SAVE_INVALID);
    bad = g; bad.hunt_round = 0; CHECK(save_encode(&bad, bytes, &size) == SAVE_INVALID);
    CHECK(save_encode(&g, bytes, &size) == SAVE_OK);
    size_t hp_at = 224 + 4 + 4;
    bytes[hp_at] = 39; bytes[hp_at + 1] = 0;
    unsigned h = 2166136261u;
    for (size_t i = 0; i < size - 4; i++) h = (h ^ bytes[i]) * 16777619u;
    for (int i = 0; i < 4; i++) bytes[size - 4 + i] = (unsigned char)(h >> (8 * i));
    Game sentinel = g, out = g;
    CHECK(save_decode(&out, bytes, size) == SAVE_CORRUPT && !memcmp(&out, &sentinel, sizeof out));
}

static void wait_play(Game *g, float seconds) {
    for (int n = (int)lroundf(seconds / .05f); n > 0; n--) {
        g->hero.hp = 100; g->hero.invul = 1;
        simulation_tick(g, .05f, (Vec2){0});
    }
}
/* While the hero stays on the pass, each fallen ordinary boar returns after
 * HUNT_RESPAWN_SECONDS of play, never beside the hero, up to HUNT_COUNT. */
static void timed_respawn(void) {
    Game g; ready(&g);
    Vec2 safe = MAP_EXITS[0].arrival;
    for (int i = 0; i < HUNT_COUNT; i++) CHECK(distance(safe, HUNT_SPAWNS[i]) >= HUNT_RESPAWN_CLEAR);
    g.hero.pos = g.hunt[0].pos; CHECK(game_damage(&g, &g.hunt[0], 99, FX_HIT));
    CHECK(g.hunt_respawn[0] == HUNT_RESPAWN_SECONDS);
    g.hero.pos = safe; int coins = g.coins; unsigned round = g.hunt_round;
    wait_play(&g, HUNT_RESPAWN_SECONDS - 1);
    CHECK(g.hunt[0].hp == 0 && g.hunt_respawn[0] > .5f && g.hunt_respawn[0] < 1.5f);
    /* Menus, training and loading neither run nor reset the clock. */
    float left = g.hunt_respawn[0];
    const Phase menus[] = {PH_PAUSED, PH_BAG, PH_JOURNAL, PH_TECHNIQUES, PH_DIALOGUE, PH_SHOP};
    for (unsigned m = 0; m < sizeof menus / sizeof *menus; m++) {
        g.phase = menus[m];
        for (int i = 0; i < 200; i++) game_tick(&g, .05f, (Vec2){0});
        g.phase = PH_PLAY;
    }
    CHECK(g.hunt_respawn[0] == left && g.hunt[0].hp == 0);
    Game saved; CHECK(game_enter_training(&g, &saved));
    for (int i = 0; i < 200; i++) simulation_tick(&g, .05f, (Vec2){0});
    CHECK(game_leave_training(&g, &saved) && g.hunt_respawn[0] == left);
    g = roundtrip(&g);
    CHECK(g.hunt[0].hp == 0 && g.hunt_respawn[0] == left && g.hunt_round == round);
    g.hero.pos = safe;
    wait_play(&g, 2);
    CHECK(g.hunt[0].hp == BOAR_HP && g.hunt[0].state == AI_IDLE && g.hunt_respawn[0] == 0);
    CHECK(distance(g.hunt[0].pos, HUNT_SPAWNS[0]) == 0 && g.coins == coins && g.hunt_round == round);
    /* Due while the hero stands at its spawn: it waits until they leave. */
    g.hero.pos = g.hunt[1].pos; CHECK(game_damage(&g, &g.hunt[1], 99, FX_HIT));
    wait_play(&g, HUNT_RESPAWN_SECONDS + 5);
    CHECK(g.hunt[1].hp == 0 && g.hunt_respawn[1] == 0);
    g.hero.pos = safe; wait_play(&g, .05f);
    CHECK(g.hunt[1].hp == BOAR_HP);
    /* Hunt the whole pass three times over in one visit: always back to six,
     * nine coins per kill, no herb or quest change. */
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < HUNT_COUNT; i++) {
            g.hero.pos = g.hunt[i].pos; CHECK(game_damage(&g, &g.hunt[i], 99, FX_HIT));
        }
        CHECK(alive(&g) == 0);
        g.hero.pos = safe; wait_play(&g, HUNT_RESPAWN_SECONDS / 2);
        CHECK(alive(&g) == 0);
        wait_play(&g, HUNT_RESPAWN_SECONDS / 2 + 1);
        CHECK(alive(&g) == HUNT_COUNT);
    }
    CHECK(g.coins == coins + (3 * HUNT_COUNT + 1) * HUNT_REWARD && g.herbs == 0 && g.quest == Q_RECOVER);
    CHECK(g.hunt_round == round);
    for (int i = 0; i < BOAR_COUNT; i++) CHECK(!g.drops[i].exists);
    /* Out of the pass the clock stops; walking back in refills everything. */
    g.hero.pos = g.hunt[2].pos; CHECK(game_damage(&g, &g.hunt[2], 99, FX_HIT));
    to_village(&g); left = g.hunt_respawn[2];
    wait_play(&g, HUNT_RESPAWN_SECONDS * 2);
    CHECK(g.hunt[2].hp == 0 && g.hunt_respawn[2] == left);
    to_pass(&g); CHECK(alive(&g) == HUNT_COUNT && g.hunt_respawn[2] == 0);
    /* Before the traces nothing counts down. */
    game_reset(&g); game_start(&g); to_pass(&g);
    g.hunt[3].hp = 0; g.hunt[3].state = AI_DEAD; g.hunt_respawn[3] = 5;
    wait_play(&g, 10); CHECK(g.hunt[3].hp == 0 && g.hunt_respawn[3] == 5);
    /* Saves reject an impossible clock. */
    ready(&g);
    Game bad = g; bad.hunt_respawn[0] = 1; CHECK(save_encode(&bad, bytes, &size) == SAVE_INVALID);
    bad = g; bad.hunt[0].hp = 0; bad.hunt_respawn[0] = HUNT_RESPAWN_SECONDS + 1;
    CHECK(save_encode(&bad, bytes, &size) == SAVE_INVALID);
    bad.hunt_respawn[0] = NAN; CHECK(save_encode(&bad, bytes, &size) == SAVE_INVALID);
}

/* A v2 file (before repeat hunting) keeps its progress and starts round 1. */
static void v2_migration(void) {
    Game g; ready(&g);
    g.hero.pos = g.boars[1].pos; CHECK(game_damage(&g, &g.boars[1], 99, FX_HIT));
    g.hero.pos = g.hunt[2].pos; CHECK(game_damage(&g, &g.hunt[2], 99, FX_HIT));
    g.hunt_round = 7;
    CHECK(save_encode(&g, bytes, &size) == SAVE_OK);
    size = 224; bytes[4] = 2; bytes[8] = 224; bytes[9] = 0;
    unsigned h = 2166136261u;
    for (size_t i = 0; i < size - 4; i++) h = (h ^ bytes[i]) * 16777619u;
    for (int i = 0; i < 4; i++) bytes[size - 4 + i] = (unsigned char)(h >> (8 * i));
    Game loaded;
    CHECK(save_decode(&loaded, bytes, size) == SAVE_OK);
    CHECK(loaded.quest == Q_RECOVER && loaded.coins == g.coins && loaded.boars[1].hp == 0 && loaded.drops[1].exists);
    CHECK(loaded.hunt_round == 1 && alive(&loaded) == HUNT_COUNT);
    CHECK(save_encode(&loaded, bytes, &size) == SAVE_OK && size == SAVE_V3_SIZE && bytes[4] == 3);
    /* Unknown future versions are still refused. */
    bytes[4] = 4; CHECK(save_decode(&loaded, bytes, size) == SAVE_VERSION_NEW);
}

/* Dying on the pass: home to the village, kills and coins kept; walking back
 * in is a new round like any other entry. */
static void death(void) {
    Game g; ready(&g);
    g.hero.pos = g.hunt[1].pos; CHECK(game_damage(&g, &g.hunt[1], 99, FX_HIT));
    g.hero.pos = g.hunt[0].pos; CHECK(game_damage(&g, &g.hunt[0], 5, FX_HIT));
    int coins = g.coins; unsigned round = g.hunt_round;
    g.hero.hp = 0; g.phase = PH_DEAD;
    Game loaded = roundtrip(&g);
    CHECK(loaded.map == MAP_VILLAGE && loaded.phase == PH_PLAY && loaded.coins == coins);
    CHECK(loaded.hunt_round == round && loaded.hunt[1].hp == 0 && loaded.hunt[0].hp == BOAR_HP);
    game_recover(&g);
    CHECK(g.map == MAP_VILLAGE && g.coins == coins && g.hunt_round == round && g.hunt[1].hp == 0);
    to_pass(&g); CHECK(g.hunt_round == round + 1 && alive(&g) == HUNT_COUNT && g.coins == coins);
    /* A butcher retry happens in the cave and leaves the pass alone. */
    g.hunt[0].hp = 0; g.replay = false; g.quest = Q_COMBAT; g.map = MAP_CAVE;
    game_retry(&g); CHECK(g.map == MAP_CAVE && g.hunt[0].hp == 0 && g.hunt_round == round + 1);
}

/* The whole chapter with hunting mixed in, saving and loading at every step. */
static void chapter(void) {
    Game g; game_reset(&g); game_start(&g);
    g = roundtrip(&g);
    g.hero.pos = NPCS[ELDER].pos; CHECK(game_accept(&g)); g = roundtrip(&g);
    to_pass(&g);
    for (int i = 0; i < TRACE_COUNT; i++) { g.hero.pos = TRACES[i].pos; CHECK(game_inspect(&g, i)); g = roundtrip(&g); }
    int hunted = 0;
    for (int visit = 0; visit < 3; visit++) {
        for (int i = 0; i < HUNT_COUNT; i++) {
            g.hero.pos = g.hunt[i].pos; CHECK(game_damage(&g, &g.hunt[i], 99, FX_HIT)); hunted++;
            g = roundtrip(&g);
        }
        CHECK(g.quest == Q_RECOVER && g.herbs == 0);
        to_village(&g); g = roundtrip(&g); to_pass(&g); g = roundtrip(&g);
    }
    for (int i = 0; i < BOAR_COUNT; i++) {
        g.hero.pos = g.boars[i].pos; CHECK(game_damage(&g, &g.boars[i], 99, FX_HIT));
        g = roundtrip(&g); /* Killed, herb not yet collected. */
        CHECK(g.drops[i].exists && !g.drops[i].collected);
        g.hero.pos = g.drops[i].pos; CHECK(game_collect(&g, i));
        g = roundtrip(&g);
    }
    CHECK(g.quest == Q_DELIVER && g.herbs == 3 && g.coins == 82 + hunted * HUNT_REWARD);
    /* Ordinary boars after the herbs still pay only coins. */
    g.hero.pos = g.hunt[0].pos; CHECK(game_damage(&g, &g.hunt[0], 99, FX_HIT)); hunted++;
    CHECK(g.herbs == 3 && g.quest == Q_DELIVER);
    to_village(&g);
    g.hero.pos = NPCS[HERBALIST].pos; CHECK(game_deliver(&g)); g = roundtrip(&g);
    CHECK(!game_deliver(&g) && g.coins == 137 + hunted * HUNT_REWARD);
    to_pass(&g); /* Hunting after the delivery. */
    for (int i = 0; i < HUNT_COUNT; i++) { g.hero.pos = g.hunt[i].pos; CHECK(game_damage(&g, &g.hunt[i], 99, FX_HIT)); hunted++; }
    CHECK(g.quest == Q_REPORT && g.herbs == 0 && game_pickup_count(&g) == 0);
    g = roundtrip(&g); to_village(&g);
    g.hero.pos = NPCS[ELDER].pos; CHECK(game_report(&g)); g = roundtrip(&g);
    g.hero.pos = MAP_EXITS[1].pos; CHECK(game_travel(&g, 1));
    CHECK(game_begin_boss(&g)); CHECK(game_damage(&g, &g.boss, 1000, FX_HIT));
    g = roundtrip(&g); CHECK(g.quest == Q_RETURN && g.coins == 207 + hunted * HUNT_REWARD);
    g.hero.pos = MAP_EXITS[3].pos; CHECK(game_travel(&g, 3));
    g.phase = PH_AUGMENT; g = roundtrip(&g); CHECK(game_choose(&g, AUG_WIND));
    g = roundtrip(&g); CHECK(g.quest == Q_MESSENGER);
    /* After the chapter the pass still offers a fresh round each visit. */
    unsigned round = g.hunt_round; to_pass(&g);
    CHECK(g.hunt_round == round + 1 && alive(&g) == HUNT_COUNT && game_hunt_active(&g));
    g.hero.pos = g.hunt[1].pos; CHECK(game_damage(&g, &g.hunt[1], 99, FX_HIT)); hunted++;
    g = roundtrip(&g);
    CHECK(g.quest == Q_MESSENGER && g.coins == 207 + hunted * HUNT_REWARD && g.boss_rewarded && g.supplies_delivered);
    to_village(&g); g.hero.pos = NPCS[MESSENGER].pos;
    CHECK(game_nearby(&g).kind == NEAR_NPC && game_nearby(&g).index == MESSENGER);
}

int main(void) {
    spawns(); where_they_appear(); rewards_apart_from_quest(); real_combat();
    shared_attack_slots(); rounds(); timed_respawn(); saving(); v2_migration(); death(); chapter();
    printf("PASS: %d repeat hunting checks.\n", checks);
    return 0;
}
