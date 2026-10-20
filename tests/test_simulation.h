#ifndef PALDO_TEST_SIMULATION_H
#define PALDO_TEST_SIMULATION_H
#include "game.h"
#include <math.h>

/* Legacy combat/quest tests measure simulation seconds, independent of the
 * contact pause. Drain the real pause through the public tick function rather
 * than disabling feedback. test_feedback.c separately tests wall-clock timing;
 * SDL input integration tests also continue to advance actual frame time. */
static inline void simulation_tick(Game *g, float dt, Vec2 input) {
    if (dt > 0 && isfinite(dt) && isfinite(input.x) && isfinite(input.y))
        while (g->phase == PH_PLAY && g->hitstop_time > 0)
            game_tick(g, fminf(.05f, g->hitstop_time), input);
    game_tick(g, dt, input);
}
#endif
