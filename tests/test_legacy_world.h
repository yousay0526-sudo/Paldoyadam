#ifndef PALDO_LEGACY_WORLD_FIXTURE_H
#define PALDO_LEGACY_WORLD_FIXTURE_H
#include "game.h"
#include "world.h"
/* Pre-separation fixtures exercise the unchanged combat/collision algorithms
 * on the preserved original painting. New map tests use actual game_reset. */
static inline void legacy_reset(Game *g) {
    game_reset(g);g->map=MAP_TRAINING;world_select(g->map);
    g->boss.pos=g->boss.home=BOSS_HOME;
    const Vec2 p[3]={{.62f*WORLD_W,.69f*WORLD_H},{.66f*WORLD_W,.74f*WORLD_H},{.70f*WORLD_W,.66f*WORLD_H}};
    for(int i=0;i<3;i++) g->boars[i].pos=g->boars[i].home=p[i];
}
#endif
