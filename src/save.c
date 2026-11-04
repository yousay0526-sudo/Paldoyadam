#include "save.h"
#include "world.h"
#include <math.h>
#include <string.h>

static void put(unsigned char *b, size_t *n, uint32_t v) {
    for (int i = 0; i < 4; i++) b[(*n)++] = (unsigned char)(v >> (8 * i));
}
static uint32_t get(const unsigned char *b, size_t *n) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)b[(*n)++] << (8 * i);
    return v;
}
static void put_float(unsigned char *b, size_t *n, float f) {
    uint32_t v; memcpy(&v, &f, 4); put(b, n, v);
}
static float get_float(const unsigned char *b, size_t *n) {
    uint32_t v = get(b, n); float f; memcpy(&f, &v, 4); return f;
}
static uint32_t checksum(const unsigned char *b, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
static bool bounded(float f, float hi) { return isfinite(f) && f >= 0 && f <= hi; }
/* This is the currently implemented chapter, including its reward invariants.
 * Revisit deliberately when adding quests/items, with a version migration. */
static bool valid(const Game *g) {
    if (sizeof(float) != 4 || g->map < MAP_VILLAGE || g->map >= MAP_TRAINING ||
        g->quest < Q_MEET || g->quest >= QUEST_COUNT || g->hero.max_hp != 100 ||
        g->hero.hp < 0 || g->hero.hp > 100 || g->coins < 0 || g->coins > 1000000 ||
        g->tonics < 0 || g->tonics > 22 || g->bandages < 0 || g->bandages > 21 ||
        g->herbs < 0 || g->herbs > 3 || g->augment < AUG_NONE || g->augment > AUG_FLOW ||
        (g->sword && !g->owned_sword) || (g->armor && !g->owned_armor) ||
        !bounded(g->hero.bleed, 5) || !bounded(g->hero.bleed_tick, 1) ||
        !bounded(g->cooldown, 5.5f) || !bounded(g->attack_cooldown, .75f) ||
        !bounded(g->item_cooldown, 1) || g->dodge_charges < 0 || g->dodge_charges > DODGE_CHARGES ||
        !bounded(g->dodge_recharge, DODGE_RECHARGE_SECONDS) ||
        (g->dodge_charges == DODGE_CHARGES && g->dodge_recharge != 0)) return false;
    for (int i = 0; i < TECH_COUNT; i++)
        if (!bounded(g->technique_cooldowns[i], TECHNIQUES[i].cooldown)) return false;
    unsigned seen = 0;
    for (int i = 0; i < SWIPE_COUNT; i++) {
        if (g->swipe_bindings[i] < 0 || g->swipe_bindings[i] >= TECH_COUNT ||
            (seen & (1u << g->swipe_bindings[i]))) return false;
        seen |= 1u << g->swipe_bindings[i];
    }
    int collected = 0;
    for (int i = 0; i < BOAR_COUNT; i++) {
        if (g->boars[i].hp < 0 || g->boars[i].hp > 38 ||
            (g->boars[i].hp == 0) != g->drops[i].exists ||
            (g->drops[i].collected && !g->drops[i].exists)) return false;
        collected += g->drops[i].collected;
        if (g->quest < Q_RECOVER && !g->replay && g->boars[i].hp != 38) return false;
    }
    /* Ordinary boars: hp 0 means killed and already paid. */
    if (g->hunt_round < 1) return false;
    for (int i = 0; i < HUNT_COUNT; i++)
        if (g->hunt[i].hp < 0 || g->hunt[i].hp > BOAR_HP ||
            !bounded(g->hunt_respawn[i], HUNT_RESPAWN_SECONDS) ||
            (g->hunt[i].hp > 0 && g->hunt_respawn[i] != 0)) return false;
    if (g->herbs != (g->supplies_delivered ? 0 : collected) ||
        (g->supplies_delivered && collected != 3) ||
        (g->quest == Q_MEET && (g->traces[0] || g->traces[1])) ||
        (g->quest == Q_TRACES && g->traces[0] && g->traces[1]) ||
        (g->quest >= Q_RECOVER && !(g->traces[0] && g->traces[1])) ||
        (g->quest == Q_RECOVER && collected >= 3) ||
        (g->quest == Q_DELIVER && collected != 3) ||
        (!g->replay && g->supplies_delivered != (g->quest >= Q_REPORT)) ||
        (!g->replay && g->boss_rewarded != (g->quest >= Q_RETURN)) ||
        (!g->replay && (g->augment != AUG_NONE) != (g->quest == Q_MESSENGER)) ||
        (g->replay && (!g->supplies_delivered || !g->boss_rewarded || g->augment == AUG_NONE ||
                       g->quest < Q_HUNT)) ||
        g->boss.hp < 0 || g->boss.hp > 240 ||
        (g->quest >= Q_RETURN ? g->boss.hp != 0 : g->boss.hp == 0)) return false;
    return true;
}
/* Only resume modes with meaning outside the current UI are serialized. */
static unsigned mode(const Game *g) {
    return g->phase == PH_AUGMENT ? 1 : g->phase == PH_COMPLETE ? 2 :
           g->hero.hp == 0 || g->phase == PH_DEAD ? 3 : g->phase == PH_PROLOGUE ? 4 : 0;
}
SaveResult save_encode(const Game *g, unsigned char *b, size_t *size) {
    if (!g || !b || !size || g->training.active || g->phase == PH_TITLE || !valid(g)) return SAVE_INVALID;
    size_t n = 0;
    put(b, &n, 0x31594450u); put(b, &n, SAVE_VERSION); put(b, &n, 0);
#define I(v) put(b, &n, (uint32_t)(v))
#define F(v) put_float(b, &n, (v))
    I(g->map); I(g->quest); I(mode(g));
    F(g->hero.pos.x); F(g->hero.pos.y); I(g->hero.hp); I(g->hero.max_hp);
    F(g->hero.bleed); F(g->hero.bleed_tick);
    I(g->coins); I(g->tonics); I(g->bandages); I(g->herbs);
    I(g->traces[0]); I(g->traces[1]); I(g->supplies_delivered); I(g->boss_rewarded);
    I(g->sword); I(g->armor); I(g->owned_sword); I(g->owned_armor); I(g->augment); I(g->replay);
    F(g->cooldown); F(g->attack_cooldown); F(g->item_cooldown);
    for (int i = 0; i < TECH_COUNT; i++) F(g->technique_cooldowns[i]);
    for (int i = 0; i < SWIPE_COUNT; i++) I(g->swipe_bindings[i]);
    I(g->boss.hp);
    for (int i = 0; i < BOAR_COUNT; i++) {
        I(g->boars[i].hp); I(g->drops[i].exists); I(g->drops[i].collected);
        F(g->drops[i].pos.x); F(g->drops[i].pos.y);
    }
    I(g->dodge_charges); F(g->dodge_recharge);
    I(g->hunt_round);
    for (int i = 0; i < HUNT_COUNT; i++) { I(g->hunt[i].hp); F(g->hunt_respawn[i]); }
    size_t p = 8; put(b, &p, (uint32_t)(n + 4));
    uint32_t sum = checksum(b, n); I(sum);
#undef I
#undef F
    *size = n; return SAVE_OK;
}
static Vec2 safe_home(MapId map) {
    return map == MAP_PASS ? MAP_EXITS[0].arrival : map == MAP_CAVE ? MAP_EXITS[1].arrival : HOME;
}
static bool reachable(Vec2 home, Vec2 p, float radius) {
    Route route;
    return isfinite(p.x) && isfinite(p.y) && world_clear(p, radius) &&
        world_route(home, p, radius, &route) && route.count > 0 &&
        distance(route.points[route.count - 1], p) < 2;
}
SaveResult save_decode(Game *out, const unsigned char *b, size_t size) {
    /* Versioned record sizes bound every read, including hostile input. */
    if (!out || !b || size < 16 || size > SAVE_CAPACITY) return SAVE_CORRUPT;
    size_t n = 0;
    if (get(b, &n) != 0x31594450u) return SAVE_CORRUPT;
    uint32_t version = get(b, &n);
    if (version < 1 || version > SAVE_VERSION) return SAVE_VERSION_NEW;
    static const uint32_t sizes[] = {0, 216u, 224u, SAVE_V3_SIZE};
    if (get(b, &n) != size || size != sizes[version]) return SAVE_CORRUPT;
    size_t end = size - 4;
    if (get(b, &end) != checksum(b, size - 4)) return SAVE_CORRUPT;
    MapId previous = world_selected();
    Game g; game_reset(&g);
#define I(v) (v) = (int)get(b, &n)
#define F(v) (v) = get_float(b, &n)
#define B(v) do { uint32_t flag = get(b, &n); if (flag > 1) goto corrupt; (v) = flag != 0; } while (0)
    I(g.map); I(g.quest); unsigned resume = get(b, &n);
    F(g.hero.pos.x); F(g.hero.pos.y); I(g.hero.hp); I(g.hero.max_hp);
    F(g.hero.bleed); F(g.hero.bleed_tick);
    I(g.coins); I(g.tonics); I(g.bandages); I(g.herbs);
    B(g.traces[0]); B(g.traces[1]); B(g.supplies_delivered); B(g.boss_rewarded);
    B(g.sword); B(g.armor); B(g.owned_sword); B(g.owned_armor); I(g.augment); B(g.replay);
    F(g.cooldown); F(g.attack_cooldown); F(g.item_cooldown);
    for (int i = 0; i < TECH_COUNT; i++) F(g.technique_cooldowns[i]);
    for (int i = 0; i < SWIPE_COUNT; i++) I(g.swipe_bindings[i]);
    I(g.boss.hp);
    for (int i = 0; i < BOAR_COUNT; i++) {
        I(g.boars[i].hp); B(g.drops[i].exists); B(g.drops[i].collected);
        F(g.drops[i].pos.x); F(g.drops[i].pos.y);
        g.boars[i].state = g.boars[i].hp == 0 ? AI_DEAD : AI_IDLE;
        g.drops[i].age = DROP_REST_SECONDS;
    }
    /* v1 predates dodge charge persistence; reset defaults migrate it safely. */
    if (version >= 2) { I(g.dodge_charges); F(g.dodge_recharge); }
    /* v1/v2 predate repeat hunting: game_reset's fresh first round stands.
     * From v3 the round and each ordinary boar's health come back as saved;
     * survivors wait at their spawn and loading never starts a new round. */
    if (version >= 3) {
        g.hunt_round = get(b, &n);
        for (int i = 0; i < HUNT_COUNT; i++) {
            I(g.hunt[i].hp); F(g.hunt_respawn[i]);
            g.hunt[i].state = g.hunt[i].hp == 0 ? AI_DEAD : AI_IDLE;
        }
    }
#undef I
#undef F
#undef B
    if (n != size - 4 || !valid(&g) || resume > 4 ||
        (resume == 1 && (g.quest != Q_RETURN || g.augment != AUG_NONE || g.replay)) ||
        (resume == 2 && g.quest != Q_MESSENGER && !(g.replay && g.quest == Q_RETURN)) ||
        (resume == 4 && g.quest != Q_MEET) || ((g.hero.hp == 0) != (resume == 3))) goto corrupt;
    world_select(MAP_PASS);
    for (int i = 0; i < BOAR_COUNT; i++)
        if (g.drops[i].exists && !reachable(safe_home(MAP_PASS), g.drops[i].pos, HERO_RADIUS))
            g.drops[i].pos = g.boars[i].home;
    world_select(g.map);
    if (!reachable(safe_home(g.map), g.hero.pos, HERO_RADIUS)) g.hero.pos = safe_home(g.map);
    g.boss.state = g.boss.hp == 0 ? AI_DEAD : AI_IDLE;
    g.phase = resume == 1 ? PH_AUGMENT : resume == 2 ? PH_COMPLETE : resume == 4 ? PH_PROLOGUE : PH_PLAY;
    if (resume == 3) {
        if (g.replay) game_retry(&g); else game_recover(&g);
    } else if (g.quest == Q_COMBAT) game_retry(&g);
    *out = g; world_select(previous); return SAVE_OK;
corrupt:
    world_select(previous); return SAVE_CORRUPT;
}
const char *save_result_text(SaveResult r) {
    switch (r) {
    case SAVE_OK: return "자동저장 완료";
    case SAVE_MISSING: return "저장된 여정 없음";
    case SAVE_CORRUPT: return "저장 손상 · 새 여정 가능";
    case SAVE_VERSION_NEW: return "지원하지 않는 저장 버전";
    case SAVE_UNAVAILABLE: return "저장소 사용 불가";
    case SAVE_FULL: return "저장 공간 부족";
    case SAVE_INVALID: return "저장 실패 · 상태 오류";
    default: return "저장 실패 · 재시도";
    }
}
