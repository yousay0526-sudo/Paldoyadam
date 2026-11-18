#include "game.h"
#include "raid.h"
#include <stdio.h>

/* First playable balance, not a final economy. Rare-currency outer nodes
 * belong to the separate growth-currency integration, not a second wallet. */
const CultivationInfo CULTIVATION[CUL_COUNT] = {
    {"기초 호흡", "최대 체력 +5", -1, 30, 5, 0, 0},
    {"단단한 몸", "최대 체력 +5", CUL_FOUNDATION, 60, 5, 0, 0},
    {"끈질긴 생명", "최대 체력 +10", CUL_VITALITY, 120, 10, 0, 0},
    {"힘 다지기", "기본 공격·무공 피해 +1", CUL_FOUNDATION, 60, 0, 1, 0},
    {"벼린 일격", "기본 공격·무공 피해 +1", CUL_STRENGTH, 120, 0, 1, 0},
    {"가벼운 발", "보법 충전 시간 -0.05초", CUL_FOUNDATION, 60, 0, 0, .05f},
    {"고른 숨", "보법 충전 시간 -0.05초", CUL_FOOTWORK, 120, 0, 0, .05f}
};
bool cultivation_valid(uint32_t nodes) {
    if (nodes & ~((1u << CUL_COUNT) - 1u)) return false;
    for (int i = 0; i < CUL_COUNT; i++) {
        int parent = CULTIVATION[i].prerequisite;
        if ((nodes & (1u << i)) && parent >= 0 && !(nodes & (1u << parent))) return false;
    }
    return true;
}
bool game_cultivation_has(const Game *g, CultivationNode node) {
    return g && (unsigned)node < CUL_COUNT && (g->cultivation_nodes & (1u << node)) != 0;
}
int game_cultivation_health(const Game *g) {
    int health = 100;
    for (int i = 0; i < CUL_COUNT; i++)
        if (game_cultivation_has(g, (CultivationNode)i)) health += CULTIVATION[i].health;
    return health;
}
int game_cultivation_damage(const Game *g) {
    int damage = 0;
    for (int i = 0; i < CUL_COUNT; i++)
        if (game_cultivation_has(g, (CultivationNode)i)) damage += CULTIVATION[i].damage;
    return damage;
}
float game_dodge_recharge_seconds(const Game *g) {
    float seconds = DODGE_RECHARGE_SECONDS;
    for (int i = 0; i < CUL_COUNT; i++)
        if (game_cultivation_has(g, (CultivationNode)i)) seconds -= CULTIVATION[i].recharge;
    return raid_dodge_recharge(g, seconds);
}
bool game_open_cultivation(Game *g) {
    if (!g || g->training.active || g->raid.active || g->hero.hp <= 0 ||
        (g->phase != PH_PLAY && g->phase != PH_BAG && g->phase != PH_PAUSED)) return false;
    g->phase = PH_CULTIVATION;
    return true;
}
const char *game_cultivation_error(const Game *g, CultivationNode node) {
    if (!g || (unsigned)node >= CUL_COUNT) return "수련 항목 오류";
    if (g->phase != PH_CULTIVATION || g->training.active || g->raid.active || g->hero.hp <= 0 ||
        g->map != MAP_VILLAGE || g->quest == Q_COMBAT) return "수련은 마을에서 할 수 있습니다";
    if (!cultivation_valid(g->cultivation_nodes) || g->hero.max_hp != game_cultivation_health(g) ||
        g->hero.hp > g->hero.max_hp || g->coins < 0 || g->coins > COIN_LIMIT) return "수련 상태 오류";
    if (game_cultivation_has(g, node)) return "이미 익힌 수련입니다";
    int parent = CULTIVATION[node].prerequisite;
    if (parent >= 0 && !game_cultivation_has(g, (CultivationNode)parent)) return "앞선 수련을 먼저 익히세요";
    if (g->coins < CULTIVATION[node].cost) return "엽전이 부족합니다";
    return NULL;
}
bool game_learn_cultivation(Game *g, CultivationNode node) {
    if (game_cultivation_error(g, node)) return false;
    /* One transaction. Health-cap growth does not heal current wounds. */
    g->coins -= CULTIVATION[node].cost;
    g->cultivation_nodes |= 1u << node;
    g->hero.max_hp = game_cultivation_health(g);
    char message[128];
    snprintf(message, sizeof message, "%s 습득 · %s", CULTIVATION[node].name, CULTIVATION[node].description);
    game_toast(g, message);
    return true;
}
