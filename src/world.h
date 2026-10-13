#ifndef PALDO_WORLD_H
#define PALDO_WORLD_H
#include "game.h"

/* Geometry is authored against the 1536 x 1024 village painting, then scaled
 * uniformly to world units. Bodies use a small circle at their feet. */
#define HERO_RADIUS 10.0f
#define BOAR_RADIUS 16.0f
#define BOSS_RADIUS 23.0f
#define SHAPE_POINTS 28
typedef struct {
    const char *name;
    int count;
    Vec2 points[SHAPE_POINTS];
} WorldShape;
typedef struct {
    WorldShape shape;
    float depth;
} WorldCover;
extern const WorldShape WORLD_SOLIDS[];
extern const int WORLD_SOLID_COUNT;
extern const WorldCover WORLD_COVERS[];
extern const int WORLD_COVER_COUNT;
bool world_clear(Vec2 point, float radius);
bool world_segment(Vec2 from, Vec2 to, float radius);
Vec2 world_move(Vec2 from, Vec2 delta, float radius);
bool world_route(Vec2 from, Vec2 to, float radius, Route *route);
Vec2 world_follow(Route *route, Vec2 position);
#endif
