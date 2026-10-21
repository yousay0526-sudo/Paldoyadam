#include "test_simulation.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define P(x, y) ((Vec2){(x) * 1.5f, (y) * 1.5f})
static int checks;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                     \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)

static void geometry(void) {
    const Vec2 blocked[] = {P(200, 185), P(190, 295), P(601, 198),  P(568, 475), P(740, 345),
                            P(500, 681), P(835, 510), P(1120, 580), P(778, 909)};
    for (size_t i = 0; i < sizeof blocked / sizeof *blocked; i++)
        CHECK(!world_clear(blocked[i], HERO_RADIUS));
    const Vec2 gates[] = {P(301, 286), P(549, 287), P(320, 622), P(673, 446), P(950, 154)};
    for (size_t i = 0; i < sizeof gates / sizeof *gates; i++)
        CHECK(world_clear(gates[i], HERO_RADIUS));
    CHECK(world_clear(HOME, HERO_RADIUS));
    CHECK(world_clear(BOSS_HOME, BOSS_RADIUS));
    CHECK(!world_clear((Vec2){NAN, 0}, HERO_RADIUS));
    Vec2 start = P(180, 350);
    CHECK(world_clear(start, HERO_RADIUS));
    Vec2 stop = world_move(start, (Vec2){0, -300}, HERO_RADIUS);
    CHECK(stop.y > 315 * 1.5f && stop.y < start.y);
    CHECK(world_clear(stop, HERO_RADIUS));
    CHECK(world_segment(start, stop, HERO_RADIUS));
    start = P(230, 340);
    Vec2 slide = world_move(start, (Vec2){150, -120}, HERO_RADIUS);
    CHECK(slide.x > start.x + 60 && world_clear(slide, HERO_RADIUS));

    Game g;
    game_reset(&g);
    game_start(&g);
    g.augment = AUG_FLOW;
    g.hero.pos = P(250, 319);
    g.hero.dir = (Vec2){0, -1};
    start = g.hero.pos;
    CHECK(world_clear(start, HERO_RADIUS));
    CHECK(game_skill(&g));
    for(int i=0;i<5;i++) simulation_tick(&g,.05f,(Vec2){0});
    CHECK(world_clear(g.hero.pos, HERO_RADIUS));
    CHECK(g.hero.pos.y > 300 * 1.5f);
    CHECK(distance(start, g.hero.pos) < 45);
    const Technique movement_techniques[]={TECH_THRUST,TECH_RETREAT};
    for(int t=0;t<2;t++) {
        game_reset(&g); game_start(&g); g.hero.pos=P(250,319);
        g.hero.dir=(Vec2){0,t==0?-1:1}; start=g.hero.pos;
        CHECK(game_technique(&g,movement_techniques[t]));
        for(int i=0;i<5;i++) simulation_tick(&g,.05f,(Vec2){0});
        CHECK(world_clear(g.hero.pos,HERO_RADIUS));
        CHECK(g.hero.pos.y>300*1.5f && distance(start,g.hero.pos)<fabsf(TECHNIQUES[movement_techniques[t]].step));
    }
}

static void walk_route(Vec2 from, Vec2 to, float dt) {
    Route route;
    CHECK(world_route(from, to, HERO_RADIUS, &route));
    CHECK(distance(route.points[route.count - 1], to) < .2f);
    Game g;
    game_reset(&g);
    game_start(&g);
    g.hero.pos = from;
    int ticks = 0;
    while (route.cursor < route.count && ticks++ < 12000) {
        Vec2 direction = world_follow(&route, g.hero.pos);
        if (route.cursor >= route.count)
            break;
        float d = distance(g.hero.pos, route.points[route.cursor]);
        float strength = game_arrival_strength(&g, direction, d, dt);
        simulation_tick(&g, dt, (Vec2){direction.x * strength, direction.y * strength});
        if (!world_clear(g.hero.pos, HERO_RADIUS)) {
            fprintf(stderr, "Route entered solid at %.2f,%.2f\n", g.hero.pos.x, g.hero.pos.y);
            exit(1);
        }
    }
    if (distance(g.hero.pos, to) >= 2.1f)
        fprintf(stderr, "Route stalled %.0f,%.0f -> %.0f,%.0f at %.0f,%.0f waypoint %d/%d\n", from.x, from.y,
                to.x, to.y, g.hero.pos.x, g.hero.pos.y, route.cursor, route.count);
    CHECK(ticks < 12000);
    CHECK(distance(g.hero.pos, to) < 2.1f);
}
static void quests_reachable(void) {
    Game g;
    game_reset(&g);
    for (int i = 0; i < NPC_COUNT; i++) {
        CHECK(world_clear(NPCS[i].pos, HERO_RADIUS));
        walk_route(HOME, NPCS[i].pos, 1.0f / 60);
        walk_route(NPCS[i].pos, HOME, .05f);
    }
    for (int i = 0; i < TRACE_COUNT; i++) {
        CHECK(world_clear(TRACES[i].pos, HERO_RADIUS));
        walk_route(HOME, TRACES[i].pos, .05f);
    }
    for (int i = 0; i < BOAR_COUNT; i++) {
        CHECK(world_clear(g.boars[i].pos, BOAR_RADIUS));
        walk_route(TRACES[1].pos, g.boars[i].pos, .05f);
        walk_route(g.boars[i].pos, NPCS[HERBALIST].pos, .05f);
    }
    walk_route(NPCS[ELDER].pos, BOSS_HOME, .05f);
    walk_route(BOSS_HOME, NPCS[ELDER].pos, .05f);
    Route r;
    CHECK(world_route(HOME, P(566, 474), HERO_RADIUS, &r));
    CHECK(world_clear(r.points[r.count - 1], HERO_RADIUS));
    CHECK(distance(r.points[r.count - 1], P(566, 474)) > HERO_RADIUS);
    CHECK(!world_route(HOME, (Vec2){NAN, 0}, HERO_RADIUS, &r));
}
/* Runs before any other routing so the grids start cold. */
static void prepared_routes(void) {
    const float radii[3] = {HERO_RADIUS, BOAR_RADIUS, BOSS_RADIUS};
    const Vec2 from[3] = {HOME, HOME, BOSS_HOME}, to[3] = {P(566, 474), P(566, 474), NPCS[ELDER].pos};
    clock_t start = clock();
    world_prepare();
    clock_t prepared = clock();
    for (int i = 0; i < 3; i++) {
        Route r;
        CHECK(!world_segment(from[i], to[i], radii[i]));
        CHECK(world_route(from[i], to[i], radii[i], &r));
        CHECK(r.count > 0);
    }
    clock_t routed = clock();
    /* Building a grid samples every cell; a detour on a ready grid is a search. */
    CHECK((routed - prepared) * 4 < prepared - start);
}
static void combat_respects_walls(void) {
    Game g;
    game_reset(&g);
    game_start(&g);
    g.traces[0] = g.traces[1] = true;
    g.quest = Q_RECOVER;
    g.hero.pos = P(250, 314);
    g.boars[0].pos = P(250, 257);
    g.boars[1].hp = g.boars[2].hp = 0;
    CHECK(world_clear(g.hero.pos, HERO_RADIUS));
    CHECK(world_clear(g.boars[0].pos, BOAR_RADIUS));
    CHECK(!world_segment(g.hero.pos, g.boars[0].pos, 0));
    CHECK(game_skill(&g));
    for(int i=0;i<5;i++) simulation_tick(&g,.05f,(Vec2){0});
    CHECK(g.boars[0].hp == 38);
    g.augment = AUG_WIND;
    g.attack_cooldown = 0;
    simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.boars[0].hp == 38);

    g.hero.pos = P(280, 440);
    g.boars[0].pos = P(280, 394);
    CHECK(world_clear(g.hero.pos, HERO_RADIUS));
    CHECK(world_clear(g.boars[0].pos, BOAR_RADIUS));
    CHECK(distance(g.hero.pos, g.boars[0].pos) < 73);
    g.boars[0].state = AI_WINDUP;
    g.boars[0].timer = 0;
    g.attack_cooldown = 100;
    simulation_tick(&g, .05f, (Vec2){0});
    CHECK(g.hero.hp == 100);

    g.hero.pos = P(441, 349);
    g.boars[0].pos = P(356, 360);
    g.boars[0].home = g.boars[0].pos;
    g.boars[0].state = AI_CHASE;
    g.boars[0].repath = 0;
    g.boars[0].route.count = 0;
    g.hero.invul = 100;
    g.attack_cooldown = 100;
    CHECK(world_clear(g.hero.pos, HERO_RADIUS));
    CHECK(world_clear(g.boars[0].pos, BOAR_RADIUS));
    CHECK(!world_segment(g.hero.pos, g.boars[0].pos, BOAR_RADIUS));
    for (int i = 0; i < 600; i++) {
        simulation_tick(&g, .05f, (Vec2){0});
        if (!world_clear(g.boars[0].pos, BOAR_RADIUS)) {
            fputs("Enemy entered terrain\n", stderr);
            exit(1);
        }
    }
    CHECK(distance(g.hero.pos, g.boars[0].pos) < 73);
    CHECK(world_segment(g.hero.pos, g.boars[0].pos, 0));
}
int main(void) {
    prepared_routes();
    geometry();
    quests_reachable();
    combat_respects_walls();
    printf("PASS: %d terrain, gate, route, dash and wall-combat checks.\n", checks);
    return 0;
}
