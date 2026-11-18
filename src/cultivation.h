#ifndef PALDO_CULTIVATION_H
#define PALDO_CULTIVATION_H
#include <stdbool.h>
#include <stdint.h>

/* Persisted IDs: append new nodes; never reorder existing bits. */
typedef enum {
    CUL_FOUNDATION, CUL_VITALITY, CUL_ENDURANCE, CUL_STRENGTH,
    CUL_EDGE, CUL_FOOTWORK, CUL_BREATH, CUL_COUNT
} CultivationNode;
typedef struct {
    const char *name, *description;
    int prerequisite, cost, health, damage;
    float recharge;
} CultivationInfo;
extern const CultivationInfo CULTIVATION[CUL_COUNT];
bool cultivation_valid(uint32_t nodes);
struct Game;
bool game_cultivation_has(const struct Game *g, CultivationNode node);
int game_cultivation_health(const struct Game *g);
int game_cultivation_damage(const struct Game *g);
float game_dodge_recharge_seconds(const struct Game *g);
bool game_open_cultivation(struct Game *g);
const char *game_cultivation_error(const struct Game *g, CultivationNode node);
bool game_learn_cultivation(struct Game *g, CultivationNode node);
#endif
