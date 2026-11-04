#include "game.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define P(x, y) {(x) * WORLD_W, (y) * WORLD_H}
const Vec2 HOME = P(.30f, .51f), BOSS_HOME = P(.81f, .46f);
const Vec2 CAVE_HOME = {768 * 1.5f, 440 * 1.5f};
/* Open dirt paths on the pass, far from the arrival point, the return exit,
 * the quest boars and both traces; tests check clearance and reachability. */
const Vec2 HUNT_SPAWNS[HUNT_COUNT] = {{225, 540}, {1050, 150}, {2100, 315}, {1950, 1050}, {300, 880}, {1440, 225}};
const Npc NPCS[NPC_COUNT] = {{P(.29f, .43f), "마을 이장", "이야기"},
                             {P(.21f, .285f), "대장장이", "대장간"},
                             {P(.355f, .29f), "약방 주인", "약방"},
                             {{400 * 1.5f, 616 * 1.5f}, "주모", "주막"},
                             {{667 * 1.5f, 547 * 1.5f}, "윤겸", "한양의 전령"}};
const Trace TRACES[TRACE_COUNT] = {
    {P(.51f, .55f), "찢어진 옷자락",
     "솔바람재 숲길에 백정의 옷자락이 걸려 있다.\n곁에는 비어 있는 약재 바구니가 놓여 있다."},
    {{1150 * 1.5f, 530 * 1.5f},
     "깊게 패인 칼자국",
     "나무에 커다란 식칼 자국이 남아 있다.\n짐승 발자국과 흩어진 약재가 산길로 이어진다."}};
const QuestInfo QUESTS[QUEST_COUNT] = {{"사라진 사람들", "마을 이장에게 실종 사건에 관해 듣기"},
                                       {"숲길의 흔적", "솔바람재에서 숲길의 흔적 두 곳 조사"},
                                       {"빼앗긴 약재", "변이한 멧돼지 3마리 제압 · 약재 꾸러미 회수"},
                                       {"돌려받은 약재", "마을 약방 주인에게 약재 꾸러미 전달"},
                                       {"백정의 행방", "마을 이장에게 조사 결과 보고"},
                                       {"숲속의 울음", "탐식굴에서 변이한 백정 찾기"},
                                       {"돌아오지 못한 사람", "붉은 식칼 공격 범위를 피해 백정 제압"},
                                       {"마을의 불빛", "이장에게 소탕 소식을 전하고 깨달음 선택"},
                                       {"한양에서 온 서신", "전령 윤겸과 대화"}};
const ItemInfo ITEMS[ITEM_COUNT] = {{"단련한 환도", "기본 공격과 갈라베기 피해 +5 · 즉시 장착", 60, SMITH},
                                    {"누비 갑옷", "직접 받는 피해 -3 · 즉시 장착", 45, SMITH},
                                    {"탕약", "체력 45 회복 · 가방 또는 하단 버튼으로 사용", 12, HERBALIST},
                                    {"지혈산", "출혈 즉시 해제 · 가방 또는 하단 버튼으로 사용", 8, HERBALIST},
                                    {"국밥과 휴식", "체력 모두 회복 · 출혈 해제", 10, INNKEEPER}};
/* Disjoint intervals in one 0..99 draw: leather 0..34, bone 35..54. */
const MaterialInfo MATERIALS[MATERIAL_COUNT] = {
    {"가죽 조각", "일반 재료 · 대장간에 매각", 35, 1, 3},
    {"뼈 조각", "일반 재료 · 대장간에 매각", 20, 1, 2}
};
int game_material_roll(unsigned roll) {
    if (roll >= 100) return -1;
    unsigned end = 0;
    for (int i = 0; i < MATERIAL_COUNT; i++) {
        end += (unsigned)MATERIALS[i].chance;
        if (roll < end) return i;
    }
    return -1;
}
int game_material_total(const Game *g, Material material) {
    if ((unsigned)material >= MATERIAL_COUNT) return -1;
    int total = g->materials[material];
    int first = (int)material * MATERIAL_PILES_PER_KIND;
    for (int i = first; i < first + MATERIAL_PILES_PER_KIND; i++) {
        int q = g->material_drops[i].quantity;
        if (q < 0 || q > MATERIAL_LIMIT || total < 0 || total > MATERIAL_LIMIT - q) return -1;
        total += q;
    }
    return total;
}
static void hunt_material(Game *g, Vec2 pos) {
    /* Exactly one PRNG transition even for no-drop/cap results. Health, coins,
     * RNG and the new pile are then serialized together, never re-rolled. */
    uint32_t x = g->loot_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->loot_rng = x;
    int kind = game_material_roll((unsigned)(((uint64_t)x * 100) >> 32));
    if (kind < 0) return;
    int quantity = MATERIALS[kind].quantity;
    int total = game_material_total(g, (Material)kind);
    if (total < 0 || total > MATERIAL_LIMIT - quantity) {
        game_toast(g, "멧돼지 제압 +9문 · 재료 한도 도달(9,999개)");
        return;
    }
    int first = kind * MATERIAL_PILES_PER_KIND, slot = first;
    for (int i = first; i < first + MATERIAL_PILES_PER_KIND; i++)
        if (g->material_drops[i].quantity == 0) { slot = i; break; }
    MaterialDrop *pile = &g->material_drops[slot];
    if (pile->quantity == 0)
        pile->drop = (Drop){.pos = pos, .exists = true};
    else
        game_toast(g, "멧돼지 제압 +9문 · 재료는 기존 바닥 더미에 합산");
    pile->quantity += quantity;
}
const AugmentInfo AUGMENTS[3] = {
    {"강철 대검", "무기", "베는 범위가 넓어지고 한 번의 베기가 무거워집니다.\n공격 범위 +18 · 기본 피해 +4"},
    {"바람의 깃", "패시브", "검 끝에서 바람 칼날이 뻗어 나갑니다.\n거리 120 안의 적에게 바람 피해 6"},
    {"흐르는 발", "액티브", "갈라베기와 함께 앞으로 파고듭니다.\n스킬 재사용 4초 · 스킬 피해 +6"}};
const TechniqueInfo TECHNIQUES[TECH_COUNT] = {
    {"갈라베기", "힘을 모아 크게 내려베기", 5.5f, 105, 10, 26},
    {"횡베기", "넓게 휘둘러 주변을 베기", 1.5f, 84, 0, 12},
    {"파고들기", "앞으로 파고들며 찌르기", 2.5f, 76, 45, 14},
    {"물러베기", "가까운 적을 베고 뒤로 빠지기", 2.0f, 90, -36, 8}
};
float clampf(float v, float lo, float hi) {
    return fmaxf(lo, fminf(hi, v));
}
float distance(Vec2 a, Vec2 b) {
    return hypotf(a.x - b.x, a.y - b.y);
}
static Vec2 add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}
static void move_toward(Enemy *e, Vec2 target, float step, float radius, float dt) {
    e->repath -= dt;
    Vec2 direction = {0};
    float d = distance(e->pos, target);
    if (d < .001f)
        return;
    if (world_segment(e->pos, target, radius)) {
        step = fminf(step, d);
        direction = (Vec2){(target.x - e->pos.x) / d, (target.y - e->pos.y) / d};
        e->route.count = 0;
    } else {
        if (e->repath <= 0) {
            world_route(e->pos, target, radius, &e->route);
            e->repath = .65f;
        }
        direction = world_follow(&e->route, e->pos);
        if (e->route.cursor < e->route.count)
            step = fminf(step, distance(e->pos, e->route.points[e->route.cursor]));
    }
    Vec2 before = e->pos;
    e->pos = world_move(e->pos, (Vec2){direction.x * step, direction.y * step}, radius);
    float travelled = distance(before, e->pos);
    e->stride += travelled;
    e->walking = travelled > .0001f;
    if (e->walking)
        e->facing = (Vec2){(e->pos.x - before.x) / travelled, (e->pos.y - before.y) / travelled};
}
static bool near_npc(const Game *g, int n) {
    world_select(g->map);
    return (g->map == MAP_VILLAGE || g->map == MAP_TRAINING) && n >= 0 && n < NPC_COUNT && distance(g->hero.pos, NPCS[n].pos) < 70 &&
           world_segment(g->hero.pos, NPCS[n].pos, 0);
}
static Enemy make_enemy(Vec2 p, int hp) {
    Enemy e = {0};
    e.pos = e.home = p;
    e.hp = e.max_hp = hp;
    e.timer = .5f;
    e.facing = (Vec2){0, 1};
    return e;
}
/* Every ordinary boar back at its spawn with full health. */
static void new_hunt_round(Game *g) {
    for (int i = 0; i < HUNT_COUNT; i++) {
        g->hunt[i] = make_enemy(HUNT_SPAWNS[i], BOAR_HP);
        g->hunt_respawn[i] = 0;
    }
    g->hunt_round++;
}
static bool is_hunt(const Game *g, const Enemy *e) {
    return e >= g->hunt && e < g->hunt + HUNT_COUNT;
}
static void effect(Game *g, Effect e) {
    world_select(g->map);
    if (g->effect_count < EFFECT_MAX)
        g->effects[g->effect_count++] = e;
}
static void dead(Game *g) {
    world_select(g->map);
    if (g->hero.hp <= 0 && g->phase != PH_DEAD) {
        g->phase = PH_DEAD;
        g->notices |= NOTICE_DEATH;
    }
}
void game_toast(Game *g, const char *s) {
    world_select(g->map);
    snprintf(g->toast, sizeof g->toast, "%s", s);
    g->toast_time = TOAST_SECONDS;
}
void game_reset(Game *g) {
    memset(g, 0, sizeof *g);
    g->map = MAP_VILLAGE;
    world_select(g->map);
    g->phase = PH_TITLE;
    g->quest = Q_MEET;
    g->hero = (Hero){.pos = HOME, .dir = {1, 0}, .hp = 100, .max_hp = 100, .face = 1};
    g->boss = make_enemy(CAVE_HOME, 240);
    const Vec2 positions[3] = {{500*1.5f,380*1.5f},{1050*1.5f,400*1.5f},{790*1.5f,315*1.5f}};
    for (int i = 0; i < BOAR_COUNT; i++)
        g->boars[i] = make_enemy(positions[i], BOAR_HP);
    new_hunt_round(g);
    g->coins = 55;
    g->tonics = 2;
    g->bandages = 1;
    g->shop = -1;
    g->loot_rng = 0x50445931u;
    g->attack_cooldown = 0;
    g->queued_move = g->last_move = g->last_hit_move = g->input_move = MOVE_NONE;
    g->swipe_bindings[SWIPE_UP] = TECH_CLEAVE;
    g->swipe_bindings[SWIPE_RIGHT] = TECH_SWEEP;
    g->swipe_bindings[SWIPE_DOWN] = TECH_RETREAT;
    g->swipe_bindings[SWIPE_LEFT] = TECH_THRUST;
    g->dodge_charges = DODGE_CHARGES;
}
void game_start(Game *g) {
    world_select(g->map);
    g->phase = PH_PLAY;
    game_toast(g, "여정 시작 · 마을 이장과 대화");
}
bool game_boars_active(const Game *g) {
    world_select(g->map);
    return !g->training.active && (g->map == MAP_PASS || g->map == MAP_TRAINING) && g->traces[0] && g->traces[1];
}
bool game_hunt_active(const Game *g) {
    world_select(g->map);
    return !g->training.active && g->map == MAP_PASS && g->traces[0] && g->traces[1];
}
bool game_enter_training(Game *game, Game *saved) {
    world_select(game->map);
    if (game == saved || game->training.active || game->phase != PH_PLAY || game->hero.hp <= 0)
        return false;
    *saved = *game;
    game_reset(game);
    game->phase = PH_PLAY;
    game->training.active = true;
    game->map = MAP_TRAINING;
    game->boss = make_enemy(BOSS_HOME, 240);
    world_select(game->map);
    game->hero.pos = (Vec2){BOSS_HOME.x, BOSS_HOME.y + 45};
    game->hero.dir = (Vec2){0, -1};
    game->sword = saved->sword;
    game->armor = saved->armor;
    game->augment = saved->augment;
    memcpy(game->swipe_bindings, saved->swipe_bindings, sizeof game->swipe_bindings);
    return true;
}
bool game_leave_training(Game *game, const Game *saved) {
    world_select(game->map);
    if (game == saved || !game->training.active || saved->training.active || saved->phase != PH_PLAY)
        return false;
    *game = *saved;
    world_select(game->map);
    game->queued_move = game->last_move = game->last_hit_move = game->input_move = MOVE_NONE;
    game->buffer_time = game->combo_time = game->input_time = game->request_time = 0;
    game->combo_count = game->input_count = game->chain_hits = game->chain_last = 0;
    game->chain_pop = game->chain_fade = 0;
    game->last_impact = IMPACT_NONE;
    game->combat_request = TRAIN_READY;
    game->hero.attacking = game->hero.skilling = game->hero.walking = game->hero.dodging = false;
    game->hero.dodge_elapsed = 0; game->hero.dodge_perfect = false;
    game->perfect_slow = game->perfect_bonus = game->perfect_flash = 0;
    game->hero.vel = game->hero.knock = (Vec2){0, 0};
    game->hero.attack_elapsed = game->hero.skill_elapsed = game->hero.walk_cycle = 0;
    game_clear_hit_feedback(game);
    return true;
}
Nearby game_nearby(const Game *g) {
    world_select(g->map);
    Nearby result = {NEAR_NONE, -1};
    float best = 66;
    if (g->training.active || g->phase != PH_PLAY || g->quest == Q_COMBAT)
        return result;
    for (int i = 0; (g->map == MAP_VILLAGE || g->map == MAP_TRAINING) && i < (g->quest == Q_MESSENGER ? NPC_COUNT : NPC_COUNT - 1); i++) {
        float d = distance(g->hero.pos, NPCS[i].pos);
        if (d < best && world_segment(g->hero.pos, NPCS[i].pos, 0)) {
            best = d;
            result = (Nearby){NEAR_NPC, i};
        }
    }
    if ((g->map == MAP_PASS || g->map == MAP_TRAINING) && g->quest == Q_TRACES)
        for (int i = 0; i < TRACE_COUNT; i++) {
            float d = distance(g->hero.pos, TRACES[i].pos);
            if (!g->traces[i] && d < best && world_segment(g->hero.pos, TRACES[i].pos, 0)) {
                best = d;
                result = (Nearby){NEAR_TRACE, i};
            }
        }
    int exit = game_exit_nearby(g);
    if (result.kind == NEAR_NONE && exit >= 0) result = (Nearby){NEAR_EXIT, exit};
    return result;
}
/* The optional keyboard pickup never competes with NPC interaction. */
static bool can_pickup(const Game *g, int i) {
    world_select(g->map);
    return !g->training.active && (g->map == MAP_PASS || g->map == MAP_TRAINING) && i >= 0 && i < BOAR_COUNT && g->phase == PH_PLAY && g->quest == Q_RECOVER &&
           g->drops[i].exists && !g->drops[i].collected &&
           distance(g->hero.pos, g->drops[i].pos) < PICKUP_RADIUS &&
           world_segment(g->hero.pos, g->drops[i].pos, 0);
}
int game_pickup_count(const Game *g) {
    world_select(g->map);
    int count = 0;
    for (int i = 0; i < BOAR_COUNT; i++)
        if (can_pickup(g, i))
            count++;
    if (!g->training.active && g->map == MAP_PASS && g->phase == PH_PLAY)
        for (int i = 0; i < MATERIAL_DROP_MAX; i++)
            if (g->material_drops[i].quantity > 0 &&
                distance(g->hero.pos, g->material_drops[i].drop.pos) < PICKUP_RADIUS &&
                world_segment(g->hero.pos, g->material_drops[i].drop.pos, 0)) count++;
    return count;
}
int game_pickup_nearby(Game *g) {
    world_select(g->map);
    int count = 0;
    for (int i = 0; i < BOAR_COUNT; i++)
        if (game_collect(g, i))
            count++;
    if (count) {
        char text[128];
        snprintf(text, sizeof text, "약재 +%d · 회수 %d/3", count, g->herbs);
        game_toast(g, text);
    }
    for (int i = 0; i < MATERIAL_DROP_MAX; i++)
        if (game_collect_material(g, i)) count++;
    return count;
}
bool game_collect_material(Game *g, int i) {
    world_select(g->map);
    if (g->training.active || g->map != MAP_PASS || g->phase != PH_PLAY ||
        i < 0 || i >= MATERIAL_DROP_MAX) return false;
    MaterialDrop *pile = &g->material_drops[i];
    int kind = i / MATERIAL_PILES_PER_KIND, q = pile->quantity;
    if (q <= 0 || q > MATERIAL_LIMIT || g->materials[kind] < 0 ||
        g->materials[kind] > MATERIAL_LIMIT - q ||
        distance(g->hero.pos, pile->drop.pos) >= PICKUP_RADIUS ||
        !world_segment(g->hero.pos, pile->drop.pos, 0)) return false;
    /* No callback/save boundary inside the inventory + pile transaction. */
    g->materials[kind] += q;
    g->material_notice[kind] = (int)fminf(MATERIAL_LIMIT, g->material_notice[kind] + q);
    *pile = (MaterialDrop){0};
    return true;
}
Vec2 game_objective(const Game *g) {
    world_select(g->map);
    MapId target_map = g->quest == Q_TRACES || g->quest == Q_RECOVER ? MAP_PASS
        : g->quest == Q_HUNT || g->quest == Q_COMBAT ? MAP_CAVE : MAP_VILLAGE;
    if (!g->training.active && g->map != MAP_TRAINING && target_map != g->map)
        for (int i=0;i<4;i++)
            if (MAP_EXITS[i].from == g->map &&
                (g->map != MAP_VILLAGE || MAP_EXITS[i].to == target_map))
                return MAP_EXITS[i].pos;
    Vec2 p = NPCS[ELDER].pos;
    float best = 1e9f;
    if (g->quest == Q_DELIVER)
        return NPCS[HERBALIST].pos;
    if (g->quest == Q_MESSENGER)
        return NPCS[MESSENGER].pos;
    if (g->quest == Q_HUNT || g->quest == Q_COMBAT)
        return g->boss.pos;
    if (g->quest == Q_TRACES)
        for (int i = 0; i < TRACE_COUNT; i++) {
            float d = distance(g->hero.pos, TRACES[i].pos);
            if (!g->traces[i] && d < best) {
                best = d;
                p = TRACES[i].pos;
            }
        }
    if (g->quest == Q_RECOVER)
        for (int i = 0; i < BOAR_COUNT; i++) {
            Vec2 target = g->drops[i].exists ? g->drops[i].pos : g->boars[i].pos;
            float d = distance(g->hero.pos, target);
            if (!g->drops[i].collected && d < best) {
                best = d;
                p = target;
            }
        }
    return p;
}
void game_quest_text(const Game *g, char *out, size_t size) {
    world_select(g->map);
    if (g->quest == Q_TRACES)
        snprintf(out, size, "%s (%d/2)", QUESTS[g->quest].detail, g->traces[0] + g->traces[1]);
    else if (g->quest == Q_RECOVER)
        snprintf(out, size, "%s (%d/3)", QUESTS[g->quest].detail, g->herbs);
    else
        snprintf(out, size, "%s", QUESTS[g->quest].detail);
}
void game_objective_brief(const Game *g, char *out, size_t size) {
    world_select(g->map);
    static const char *brief[QUEST_COUNT] = {
        "마을 이장과 대화", "솔바람재의 흔적 조사", "멧돼지 제압 · 회수",
        "약방에 약재 전달", "이장에게 조사 결과 보고", "탐식굴로 이동",
        "공격을 피하며 백정 제압", "이장에게 소식 전하기", "전령 윤겸과 대화"};
    if (g->quest == Q_TRACES)
        snprintf(out, size, "%s  %d/2", brief[g->quest], g->traces[0] + g->traces[1]);
    else if (g->quest == Q_RECOVER)
        snprintf(out, size, "%s  %d/3", brief[g->quest], g->herbs);
    else
        snprintf(out, size, "%s", brief[g->quest]);
}
bool game_accept(Game *g) {
    world_select(g->map);
    if (g->training.active || g->phase != PH_PLAY || g->quest != Q_MEET || !near_npc(g, ELDER))
        return false;
    g->quest = Q_TRACES;
    game_toast(g, "의뢰 수락 · 숲길의 흔적");
    return true;
}
bool game_inspect(Game *g, int i) {
    world_select(g->map);
    if (g->training.active || (g->map != MAP_PASS && g->map != MAP_TRAINING) || i < 0 || i >= TRACE_COUNT || g->phase != PH_PLAY || g->quest != Q_TRACES || g->traces[i] ||
        distance(g->hero.pos, TRACES[i].pos) >= 70 || !world_segment(g->hero.pos, TRACES[i].pos, 0))
        return false;
    g->traces[i] = true;
    if (game_boars_active(g))
        g->quest = Q_RECOVER;
    return true;
}
bool game_collect(Game *g, int i) {
    world_select(g->map);
    if (!can_pickup(g, i))
        return false;
    g->drops[i].collected = true;
    g->drops[i].attracting = false;
    g->herbs++;
    if (g->herbs == 3) {
        g->quest = Q_DELIVER;
        game_toast(g, "약재 회수 완료 · 약방으로");
    } else {
        char s[128];
        snprintf(s, sizeof s, "약재 꾸러미 회수 · %d/3", g->herbs);
        game_toast(g, s);
    }
    return true;
}
bool game_deliver(Game *g) {
    world_select(g->map);
    if (g->training.active || g->phase != PH_PLAY || g->quest != Q_DELIVER || g->herbs < 3 || g->supplies_delivered ||
        !near_npc(g, HERBALIST))
        return false;
    g->herbs -= 3;
    g->supplies_delivered = true;
    g->coins += 55;
    g->tonics += 2;
    g->bandages++;
    g->quest = Q_REPORT;
    game_toast(g, "보상 +55문 · 탕약 2 · 지혈산 1");
    return true;
}
bool game_report(Game *g) {
    world_select(g->map);
    if (g->training.active || g->phase != PH_PLAY || g->quest != Q_REPORT || !g->supplies_delivered || !near_npc(g, ELDER))
        return false;
    g->quest = Q_HUNT;
    game_toast(g, "목표 갱신 · 탐식굴");
    return true;
}
bool game_open_shop(Game *g, int n) {
    world_select(g->map);
    if (g->training.active || g->phase != PH_PLAY || g->quest == Q_COMBAT || n < SMITH || n > INNKEEPER || !near_npc(g, n))
        return false;
    g->shop = n;
    g->phase = PH_SHOP;
    return true;
}
const char *game_sale_error(const Game *g, Material material, int quantity) {
    if (g->training.active || g->map != MAP_VILLAGE || g->phase != PH_SHOP ||
        g->shop != SMITH || !near_npc(g, SMITH)) return "대장간에서 매각하세요";
    if ((unsigned)material >= MATERIAL_COUNT) return "매각할 수 없는 물품";
    if (quantity <= 0 || quantity > MATERIAL_LIMIT || g->materials[material] < quantity ||
        g->materials[material] > MATERIAL_LIMIT) return "재료 수량 부족 또는 오류";
    int price = MATERIALS[material].price;
    /* Check before multiplication/addition; never clamp away paid materials. */
    if (price <= 0 || g->coins < 0 || g->coins > COIN_LIMIT ||
        quantity > (COIN_LIMIT - g->coins) / price) return "소지금 한도 초과";
    return NULL;
}
bool game_sell_material(Game *g, Material material, int quantity) {
    if (game_sale_error(g, material, quantity)) return false;
    int proceeds = quantity * MATERIALS[material].price;
    g->materials[material] -= quantity;
    g->coins += proceeds;
    char text[160];
    snprintf(text, sizeof text, "%s %d개 매각 · +%d문", MATERIALS[material].name, quantity, proceeds);
    game_toast(g, text);
    return true;
}
bool game_equip(Game *g, Item item, bool equipped) {
    world_select(g->map);
    if (g->phase != PH_BAG) return false;
    if (item == ITEM_SWORD && (g->owned_sword || g->sword)) {
        g->owned_sword = true; g->sword = equipped; return true;
    }
    if (item == ITEM_ARMOR && (g->owned_armor || g->armor)) {
        g->owned_armor = true; g->armor = equipped; return true;
    }
    return false;
}
void game_close_panel(Game *g) {
    world_select(g->map);
    if (g->phase == PH_SHOP || g->phase == PH_BAG || g->phase == PH_JOURNAL || g->phase == PH_PAUSED || g->phase == PH_TECHNIQUES) {
        g->shop = -1;
        g->phase = PH_PLAY;
    }
}
const char *game_purchase_error(const Game *g, Item item) {
    if (item < 0 || item >= ITEM_COUNT || g->phase != PH_SHOP || g->shop != (int)ITEMS[item].shop ||
        !near_npc(g, g->shop))
        return "상점에서 구입하세요";
    if ((item == ITEM_SWORD && (g->owned_sword || g->sword)) ||
        (item == ITEM_ARMOR && (g->owned_armor || g->armor)))
        return "장착 중";
    if (item == ITEM_MEAL && g->hero.hp == g->hero.max_hp && g->hero.bleed <= 0)
        return "충분히 쉬었습니다";
    if ((item == ITEM_TONIC && g->tonics >= 20) || (item == ITEM_BANDAGE && g->bandages >= 20))
        return "최대 20개 소지";
    if (g->coins < ITEMS[item].price)
        return "소지금 부족";
    return NULL;
}
bool game_buy(Game *g, Item item) {
    world_select(g->map);
    if (game_purchase_error(g, item))
        return false;
    g->coins -= ITEMS[item].price;
    switch (item) {
    case ITEM_SWORD:
        g->owned_sword = g->sword = true;
        break;
    case ITEM_ARMOR:
        g->owned_armor = g->armor = true;
        break;
    case ITEM_TONIC:
        g->tonics++;
        break;
    case ITEM_BANDAGE:
        g->bandages++;
        break;
    case ITEM_MEAL:
        g->hero.hp = g->hero.max_hp;
        g->hero.bleed = 0;
        break;
    default:
        break;
    }
    char s[128];
    snprintf(s, sizeof s, "%s · %s", ITEMS[item].name,
             item == ITEM_MEAL ? "회복 완료" : (item <= ITEM_ARMOR ? "장착 완료" : "구입 완료"));
    game_toast(g, s);
    return true;
}
bool game_can_use(const Game *g, Item item) {
    world_select(g->map);
    if (g->training.active || (g->phase != PH_PLAY && g->phase != PH_BAG) || (g->phase == PH_PLAY && g->item_cooldown > 0))
        return false;
    if (item == ITEM_TONIC)
        return g->tonics > 0 && g->hero.hp < g->hero.max_hp;
    return item == ITEM_BANDAGE && g->bandages > 0 && g->hero.bleed > 0;
}
bool game_use(Game *g, Item item) {
    world_select(g->map);
    if (!game_can_use(g, item))
        return false;
    g->item_cooldown = 1;
    if (item == ITEM_TONIC) {
        g->tonics--;
        g->hero.hp = (int)fminf((float)g->hero.max_hp, (float)g->hero.hp + 45);
        game_toast(g, "탕약 사용 · 체력 45 회복");
    } else {
        g->bandages--;
        g->hero.bleed = 0;
        game_toast(g, "지혈산 사용 · 출혈 해제");
    }
    return true;
}
static bool active_enemy(const Game *g, const Enemy *e) {
    world_select(g->map);
    if (g->training.active) return e == &g->boss;
    if (g->quest == Q_COMBAT)
        return (g->map == MAP_CAVE || g->map == MAP_TRAINING) && e == &g->boss;
    for (int i = 0; i < BOAR_COUNT; i++)
        if (e == &g->boars[i])
            return game_boars_active(g);
    return is_hunt(g, e) && game_hunt_active(g);
}
bool game_damage(Game *g, Enemy *e, int amount, EffectKind kind) {
    world_select(g->map);
    if (g->phase != PH_PLAY || e->hp <= 0 || amount <= 0 || !active_enemy(g, e))
        return false;
    e->hp = (int)fmaxf(0, (float)e->hp - (float)amount);
    e->hit_time = HIT_FLASH_SECONDS;
    if (!g->training.active) {
        /* Push away from the hero; heavier techniques push further. */
        const float speeds[TECH_COUNT] = {220, 140, 160, 110};
        float speed = kind == FX_WIND ? 40 : g->hero.skilling ? speeds[g->hero.technique] : 70;
        if (e == &g->boss) speed *= KNOCK_BOSS_SCALE;
        float d = distance(e->pos, g->hero.pos);
        Vec2 away = d > .001f ? (Vec2){(e->pos.x - g->hero.pos.x) / d, (e->pos.y - g->hero.pos.y) / d}
                              : (Vec2){cosf(g->hero.attack_angle), sinf(g->hero.attack_angle)};
        e->knock = (Vec2){e->knock.x + away.x * speed, e->knock.y + away.y * speed};
        if (e != &g->boss && kind != FX_WIND && g->hero.skilling && e->state == AI_WINDUP) {
            e->state = AI_RECOVER;
            e->timer = BOAR_STAGGER_SECONDS;
        } else if (e != &g->boss && e->state == AI_CHASE)
            e->timer = fmaxf(e->timer, .25f);
    }
    float stop = kind == FX_WIND ? HITSTOP_WIND_SECONDS
        : g->perfect_bonus > 0 || (g->hero.skilling && g->hero.technique == TECH_CLEAVE)
            ? HITSTOP_HEAVY_SECONDS : HITSTOP_LIGHT_SECONDS;
    /* Simultaneous victims and the wind add-on share a single stop. */
    g->hitstop_time = fmaxf(g->hitstop_time, stop);
    effect(g, (Effect){.kind = FX_NUMBER, .pos = add(e->pos, (Vec2){0, -48}), .life = .7f, .amount = amount});
    effect(g, (Effect){.kind = kind, .pos = g->hero.pos, .target = e->pos, .life = .25f,
                      .angle = atan2f(e->pos.y - g->hero.pos.y, e->pos.x - g->hero.pos.x)});
    if (g->training.active) {
        e->hp = e->max_hp;
        g->training.impact_damage += amount;
        g->training.hit_time = .22f;
        return true;
    }
    if (e->hp == 0) {
        e->state = AI_DEAD;
        e->dying = ENEMY_DEATH_SECONDS;
        effect(g, (Effect){.kind = FX_DUST, .pos = e->pos, .life = .55f, .amount = 3});
        if (e == &g->boss) {
            g->hero.bleed = 0;
            g->quest = Q_RETURN;
            if (!g->boss_rewarded && !g->replay) {
                g->coins += 70;
                g->boss_rewarded = true;
            }
            g->notices |= NOTICE_VICTORY;
        } else if (is_hunt(g, e)) {
            /* Paid in the same tick as the kill, so a save always sees both. */
            g->coins += HUNT_REWARD;
            g->hunt_respawn[e - g->hunt] = HUNT_RESPAWN_SECONDS;
            game_toast(g, "멧돼지 제압 +9문");
            hunt_material(g, e->pos);
        } else {
            for (int i = 0; i < BOAR_COUNT; i++)
                if (e == &g->boars[i])
                    g->drops[i] = (Drop){.pos = e->pos, .exists = true};
            g->coins += 9;
            game_toast(g, "멧돼지 제압 +9문 · 약재 발견");
        }
    }
    return true;
}
static void hurt_from(Game *g, int amount, bool bleed, Vec2 from, float push);
void game_hurt(Game *g, int amount, bool bleed) {
    world_select(g->map);
    hurt_from(g, amount, bleed, g->hero.pos, 0);
}
/* Taking a hit also shoves the hero away from the attacker. */
static void hurt_from(Game *g, int amount, bool bleed, Vec2 from, float push) {
    world_select(g->map);
    if (g->training.active || g->phase != PH_PLAY || g->hero.hp <= 0 || g->hero.invul > 0 || amount <= 0)
        return;
    amount = (int)fmaxf(1, (float)amount - (g->armor ? 3.0f : 0));
    g->hero.hp = (int)fmaxf(0, (float)g->hero.hp - amount);
    g->hero.invul = .75f;
    g->hero.hurt_time = HIT_FLASH_SECONDS;
    float d = distance(g->hero.pos, from);
    if (push > 0 && d > .001f)
        g->hero.knock = (Vec2){(g->hero.pos.x - from.x) / d * push, (g->hero.pos.y - from.y) / d * push};
    g->hitstop_time = fmaxf(g->hitstop_time, HITSTOP_HURT_SECONDS);
    if (bleed) {
        g->hero.bleed = 5;
        g->hero.bleed_tick = 1;
        game_toast(g, "출혈 · 지혈산으로 치료");
    }
    effect(g, (Effect){.kind = FX_NUMBER,
                       .pos = add(g->hero.pos, (Vec2){0, -35}),
                       .life = .8f,
                       .amount = amount,
                       .damage = true});
    effect(g, (Effect){.kind = FX_HIT, .pos = g->hero.pos, .target = g->hero.pos,
                      .life = .25f, .damage = true});
    dead(g);
}
void game_clear_hit_feedback(Game *g) {
    world_select(g->map);
    g->hitstop_time = g->hero.hurt_time = g->boss.hit_time = g->training.hit_time = 0;
    for (int i = 0; i < BOAR_COUNT; i++) g->boars[i].hit_time = 0;
    for (int i = 0; i < HUNT_COUNT; i++) g->hunt[i].hit_time = 0;
}
static Enemy *nearest_enemy(Game *g) {
    if (g->training.active)
        return world_segment(g->hero.pos, g->boss.pos, 0) ? &g->boss : NULL;
    Enemy *e = NULL;
    float best = 1e9f;
    if ((g->map == MAP_CAVE || g->map == MAP_TRAINING) && g->quest == Q_COMBAT && g->boss.hp > 0 && world_segment(g->hero.pos, g->boss.pos, 0))
        return &g->boss;
    if (game_boars_active(g))
        for (int i = 0; i < BOAR_COUNT; i++) {
            float d = distance(g->hero.pos, g->boars[i].pos);
            if (g->boars[i].hp > 0 && d < best && world_segment(g->hero.pos, g->boars[i].pos, 0)) {
                best = d;
                e = &g->boars[i];
            }
        }
    if (game_hunt_active(g))
        for (int i = 0; i < HUNT_COUNT; i++) {
            float d = distance(g->hero.pos, g->hunt[i].pos);
            if (g->hunt[i].hp > 0 && d < best && world_segment(g->hero.pos, g->hunt[i].pos, 0)) {
                best = d;
                e = &g->hunt[i];
            }
        }
    return e;
}
static bool request_move(Game *g, int move);
float game_technique_cooldown(const Game *g, Technique technique) {
    world_select(g->map);
    if (technique < 0 || technique >= TECH_COUNT) return 999;
    return technique == TECH_CLEAVE ? g->cooldown : g->technique_cooldowns[technique];
}
bool game_bind_swipe(Game *g, Swipe direction, Technique technique) {
    world_select(g->map);
    if (g->phase != PH_TECHNIQUES || direction < 0 || direction >= SWIPE_COUNT ||
        technique < 0 || technique >= TECH_COUNT) return false;
    Technique previous = g->swipe_bindings[direction];
    for (int i = 0; i < SWIPE_COUNT; i++)
        if (i != (int)direction && g->swipe_bindings[i] == technique)
            g->swipe_bindings[i] = previous;
    g->swipe_bindings[direction] = technique;
    return true;
}
bool game_attack(Game *g) {
    world_select(g->map); return request_move(g, MOVE_ATTACK); }
bool game_technique(Game *g, Technique technique) {
    world_select(g->map);
    if (technique < 0 || technique >= TECH_COUNT) { game_invalid_direction(g); return false; }
    return request_move(g, technique);
}
bool game_dodge(Game *g, Vec2 direction) {
    world_select(g->map);
    Hero *h = &g->hero;
    if (g->phase != PH_PLAY || h->hp <= 0 || h->dodging || g->dodge_charges <= 0 ||
        !isfinite(direction.x) || !isfinite(direction.y))
        return false;
    float length = hypotf(direction.x, direction.y);
    Vec2 dir = length > .05f ? (Vec2){direction.x / length, direction.y / length}
                             : (Vec2){-h->dir.x, -h->dir.y};
    if (hypotf(dir.x, dir.y) < .5f) dir = (Vec2){(float)-h->face, 0};
    /* The dash wins over a swing: before its impact the swing is lost, after
     * it only the recovery is cut. A queued follow-up waits for the dash. */
    h->attacking = h->skilling = false;
    h->attack_elapsed = h->skill_elapsed = 0;
    h->dodging = true; h->dodge_elapsed = 0; h->dodge_dir = dir;
    h->dodge_origin = h->pos; h->dodge_perfect = false;
    h->invul = fmaxf(h->invul, DODGE_INVUL_SECONDS);
    if (g->dodge_charges == DODGE_CHARGES) g->dodge_recharge = DODGE_RECHARGE_SECONDS;
    g->dodge_charges--;
    effect(g, (Effect){.kind = FX_DUST, .pos = h->pos, .life = .45f, .amount = 2,
                       .angle = atan2f(dir.y, dir.x)});
    return true;
}
bool game_skill(Game *g) {
    world_select(g->map); return game_technique(g, TECH_CLEAVE); }
bool game_begin_boss(Game *g) {
    world_select(g->map);
    if (g->training.active || (g->map != MAP_CAVE && g->map != MAP_TRAINING)) return false;
    if (g->quest != Q_HUNT)
        return false;
    g->quest = Q_COMBAT;
    g->phase = PH_PLAY;
    g->boss.state = AI_CHASE;
    g->boss.timer = 1.2f;
    game_toast(g, "붉은 범위 밖으로 · 공격 뒤가 기회");
    return true;
}
/* Shared by recovery and retry: a fresh, full-health hero at `pos` with
 * no combat state, cooldowns, effects or pending notices left over. */
static void reset_fight(Game *g, Vec2 pos) {
    world_select(g->map);
    game_clear_hit_feedback(g);
    g->hero.pos = pos;
    g->hero.walking = false;
    g->hero.walk_cycle = 0;
    g->hero.attacking = false;
    g->hero.attack_elapsed = 0;
    g->hero.skilling = false; g->hero.skill_elapsed = 0;
    g->hero.dodging = false; g->hero.dodge_elapsed = 0; g->hero.dodge_perfect = false;
    g->dodge_charges = DODGE_CHARGES; g->dodge_recharge = 0;
    g->perfect_slow = g->perfect_bonus = g->perfect_flash = 0;
    g->combo_count = g->input_count = g->chain_hits = g->chain_last = 0;
    g->chain_pop = g->chain_fade = 0;
    g->combo_time = g->input_time = g->buffer_time = g->request_time = 0;
    g->queued_move = g->last_move = g->last_hit_move = g->input_move = MOVE_NONE;
    g->last_impact = IMPACT_NONE; g->combat_request = TRAIN_READY;
    memset(g->technique_cooldowns, 0, sizeof g->technique_cooldowns);
    g->hero.hp = g->hero.max_hp;
    g->hero.bleed = g->hero.invul = 0;
    g->hero.knock = g->hero.vel = (Vec2){0, 0};
    g->cooldown = g->item_cooldown = 0;
    g->effect_count = 0;
    g->notices = 0;
}
void game_recover(Game *g) {
    world_select(g->map);
    g->map = MAP_VILLAGE;
    world_select(g->map);
    reset_fight(g, HOME);
    if (g->quest == Q_COMBAT) {
        g->boss = make_enemy(CAVE_HOME, 240);
        g->quest = Q_HUNT;
    }
    for (int i = 0; i < BOAR_COUNT; i++)
        if (g->boars[i].hp > 0)
            g->boars[i] = make_enemy(g->boars[i].home, BOAR_HP);
    /* Like the quest boars, survivors heal at home; kills and their coins
     * stand. Walking back in from the village starts a new round anyway. */
    for (int i = 0; i < HUNT_COUNT; i++)
        if (g->hunt[i].hp > 0)
            g->hunt[i] = make_enemy(HUNT_SPAWNS[i], BOAR_HP);
    g->phase = PH_PLAY;
    g->shop = -1;
    game_toast(g, "마을에서 회복 · 여정 진행 유지");
}
void game_retry(Game *g) {
    world_select(g->map);
    g->map = MAP_CAVE;
    world_select(g->map);
    reset_fight(g, add(CAVE_HOME, (Vec2){-145, 0}));
    g->boss = make_enemy(CAVE_HOME, 240);
    g->attack_cooldown = .4f;
    g->quest = Q_HUNT;
    game_begin_boss(g);
}
bool game_choose(Game *g, Augment a) {
    world_select(g->map);
    if (g->phase != PH_AUGMENT || a < AUG_STEEL || a > AUG_FLOW)
        return false;
    g->augment = a;
    g->quest = Q_MESSENGER;
    g->phase = PH_PLAY;
    game_toast(g, "한양의 전령 도착 · 윤겸과 대화");
    return true;
}
typedef struct { Vec2 *pos; float radius, mass; } Body;
/* Soft circle separation: each tick close part of every overlap, through
 * terrain collision, heavier bodies giving way less. */
static void separate_bodies(Game *g, float dt) {
    world_select(g->map);
    Body bodies[BOAR_COUNT + HUNT_COUNT + 2];
    int count = 0;
    /* A dodging hero slips between bodies; separation resumes after the dash. */
    if (!g->hero.dodging)
        bodies[count++] = (Body){&g->hero.pos, HERO_RADIUS, BODY_MASS_HERO};
    if ((g->map == MAP_CAVE || g->map == MAP_TRAINING) && g->quest == Q_COMBAT && g->boss.hp > 0)
        bodies[count++] = (Body){&g->boss.pos, BOSS_RADIUS, BODY_MASS_BOSS};
    else if (game_boars_active(g))
        for (int i = 0; i < BOAR_COUNT; i++)
            if (g->boars[i].hp > 0)
                bodies[count++] = (Body){&g->boars[i].pos, BOAR_RADIUS, BODY_MASS_BOAR};
    if (game_hunt_active(g))
        for (int i = 0; i < HUNT_COUNT; i++)
            if (g->hunt[i].hp > 0)
                bodies[count++] = (Body){&g->hunt[i].pos, BOAR_RADIUS, BODY_MASS_BOAR};
    float close = 1 - expf(-dt / BODY_SEPARATE_SECONDS);
    for (int a = 0; a < count; a++)
        for (int b = a + 1; b < count; b++) {
            Vec2 *p = bodies[a].pos, *q = bodies[b].pos;
            float dx = q->x - p->x, dy = q->y - p->y, d = hypotf(dx, dy);
            float overlap = bodies[a].radius + bodies[b].radius - d;
            if (overlap <= 0)
                continue;
            /* Stacked exactly: split along a fixed per-pair direction. */
            Vec2 n = d > .001f ? (Vec2){dx / d, dy / d} : (Vec2){cosf(a * 2.4f + b), sinf(a * 2.4f + b)};
            float push = overlap * close, share = bodies[b].mass / (bodies[a].mass + bodies[b].mass);
            *p = world_move(*p, (Vec2){-n.x * push * share, -n.y * push * share}, bodies[a].radius);
            *q = world_move(*q, (Vec2){n.x * push * (1 - share), n.y * push * (1 - share)}, bodies[b].radius);
        }
}
static int boars_winding_up(const Game *g) {
    world_select(g->map);
    int n = 0;
    for (int i = 0; i < BOAR_COUNT; i++)
        n += g->boars[i].hp > 0 && (g->boars[i].state == AI_WINDUP || g->boars[i].state == AI_SWING);
    /* Quest and ordinary boars share the same attack slots. */
    for (int i = 0; i < HUNT_COUNT; i++)
        n += g->hunt[i].hp > 0 && (g->hunt[i].state == AI_WINDUP || g->hunt[i].state == AI_SWING);
    return n;
}
/* A strike that meets a fresh dodge: true when the hero is early in the dash
 * and would have been hit where the dash began or where it is now. The
 * first one per dash slows the world, refills the combo window and a dodge
 * charge, and empowers the next impact. The dash's invulnerability already
 * stops the damage either way. */
static bool gwanpa(Game *g, const Enemy *e, bool (*reaches)(const Enemy *, Vec2)) {
    Hero *h = &g->hero;
    if (!h->dodging || h->dodge_elapsed > PERFECT_WINDOW_SECONDS ||
        !(reaches(e, h->dodge_origin) || reaches(e, h->pos)))
        return false;
    if (h->dodge_perfect) return true;
    h->dodge_perfect = true;
    g->perfect_slow = PERFECT_SLOW_SECONDS;
    g->perfect_bonus = PERFECT_BONUS_SECONDS;
    g->perfect_flash = .7f;
    g->perfect_count++;
    if (g->combo_time > 0) g->combo_time = COMBO_SECONDS;
    if (g->dodge_charges < DODGE_CHARGES) {
        g->dodge_charges++;
        g->dodge_recharge = g->dodge_charges < DODGE_CHARGES ? DODGE_RECHARGE_SECONDS : 0;
    }
    return true;
}
/* Along and across the boar's fixed heading. */
static float charge_along(const Enemy *e, Vec2 target) {
    return (target.x - e->pos.x) * cosf(e->angle) + (target.y - e->pos.y) * sinf(e->angle);
}
static bool boar_reaches(const Enemy *e, Vec2 target) {
    float along = charge_along(e, target);
    float across = -(target.x - e->pos.x) * sinf(e->angle) + (target.y - e->pos.y) * cosf(e->angle);
    return along >= 0 && along < BOAR_CHARGE_LENGTH && fabsf(across) < BOAR_CHARGE_HALF_WIDTH &&
           world_segment(e->pos, target, 0);
}
static bool boss_reaches(const Enemy *e, Vec2 target) {
    Vec2 boss = e->pos;
    float a = atan2f(target.y - boss.y, target.x - boss.x) - e->angle;
    return distance(boss, target) < BOSS_STRIKE_RANGE && fabsf(atan2f(sinf(a), cosf(a))) < BOSS_STRIKE_HALF &&
           world_segment(boss, target, 0);
}
/* Enemy behaviour numbers in one place (distances in world units, speeds
 * per second, times in seconds). Windup and strike reach live in game.h
 * because drawing shares them. */
typedef struct {
    float speed, notice, give_up, close_to, wait_at, strike_from, recover, rest;
    int damage;
} EnemyTuning;
static const EnemyTuning BOAR_TUNING = {.speed = 64, .notice = 180, .give_up = 270, .close_to = 43, .wait_at = 80,
                                        .strike_from = 90, .recover = BOAR_RECOVER_SECONDS, .rest = .4f, .damage = 9};
static const EnemyTuning BOSS_TUNING = {.speed = 45, .close_to = 57, .strike_from = 138, .recover = 1.25f,
                                        .rest = .9f, .damage = 18};
static void tick_boar(Game *g, Enemy *e, float dt) {
    world_select(g->map);
    const EnemyTuning *t = &BOAR_TUNING;
    e->walking = false;
    float d = distance(e->pos, g->hero.pos);
    e->timer -= dt;
    switch (e->state) {
    case AI_IDLE:
        if (d < t->notice)
            e->state = AI_CHASE;
        break;
    case AI_CHASE:
        if (d > t->give_up) {
            e->state = AI_RETURN;
            break;
        }
        {
            /* Without a free attack slot, wait just outside striking reach. */
            bool turn = boars_winding_up(g) < BOAR_ATTACK_TOKENS;
            if (d > (turn ? t->close_to : t->wait_at))
                move_toward(e, g->hero.pos, t->speed * dt, BOAR_RADIUS, dt);
            if (!turn)
                break;
        }
        if (d < t->strike_from && e->timer <= 0 && world_segment(e->pos, g->hero.pos, 0)) {
            e->state = AI_WINDUP;
            e->angle = atan2f(g->hero.pos.y - e->pos.y, g->hero.pos.x - e->pos.x);
            e->facing = (Vec2){cosf(e->angle), sinf(e->angle)};
            e->timer = BOAR_WINDUP_SECONDS;
        }
        break;
    case AI_WINDUP:
        if (e->timer <= 0) {
            /* The rush stops short of a hero it hits; otherwise it runs its length. */
            e->charge_left = BOAR_CHARGE_LENGTH;
            if (!gwanpa(g, e, boar_reaches) && boar_reaches(e, g->hero.pos)) {
                e->charge_left = fmaxf(0, charge_along(e, g->hero.pos) - HERO_RADIUS - BOAR_RADIUS);
                hurt_from(g, t->damage, false, e->pos, KNOCK_HERO_BOAR);
            }
            e->state = AI_SWING;
            e->timer = BOAR_CHARGE_SECONDS;
        }
        break;
    case AI_SWING: {
        float step = fminf(e->charge_left, BOAR_CHARGE_LENGTH / BOAR_CHARGE_SECONDS * dt);
        Vec2 before = e->pos;
        e->pos = world_move(e->pos, (Vec2){cosf(e->angle) * step, sinf(e->angle) * step}, BOAR_RADIUS);
        e->stride += distance(before, e->pos);
        e->charge_left = distance(before, e->pos) < step * .5f ? 0 : e->charge_left - step;
        if (e->timer <= 0) {
            e->charge_left = 0;
            e->state = AI_RECOVER;
            e->timer = t->recover - BOAR_CHARGE_SECONDS;
        }
        break;
    }
    case AI_RECOVER:
        if (e->timer <= 0) {
            e->state = AI_CHASE;
            e->timer = t->rest;
        }
        break;
    case AI_RETURN:
        if (distance(e->pos, e->home) < 4)
            e->state = AI_IDLE;
        else
            move_toward(e, e->home, t->speed * dt, BOAR_RADIUS, dt);
        break;
    default:
        break;
    }
}
static void tick_boss(Game *g, float dt) {
    world_select(g->map);
    Enemy *e = &g->boss;
    e->walking = false;
    const EnemyTuning *t = &BOSS_TUNING;
    float d = distance(g->hero.pos, e->pos);
    e->timer -= dt;
    switch (e->state) {
    case AI_CHASE:
        if (d > t->close_to)
            move_toward(e, g->hero.pos, t->speed * dt, BOSS_RADIUS, dt);
        if (e->timer <= 0 && d < t->strike_from && world_segment(e->pos, g->hero.pos, 0)) {
            e->state = AI_WINDUP;
            e->timer = BOSS_WINDUP_SECONDS;
            e->angle = atan2f(g->hero.pos.y - e->pos.y, g->hero.pos.x - e->pos.x);
            e->facing = (Vec2){cosf(e->angle), sinf(e->angle)};
        }
        break;
    case AI_WINDUP:
        if (e->timer <= 0) {
            e->state = AI_SWING;
            e->timer = BOSS_SWING_SECONDS;
            effect(g, (Effect){.kind = FX_BOSS_SWING, .pos = e->pos, .angle = e->angle, .life = .25f});
            if (!gwanpa(g, e, boss_reaches) && boss_reaches(e, g->hero.pos))
                hurt_from(g, t->damage, true, e->pos, KNOCK_HERO_BOSS);
        }
        break;
    case AI_SWING:
        if (e->timer <= 0) {
            e->state = AI_RECOVER;
            e->timer = t->recover;
        }
        break;
    case AI_RECOVER:
        if (e->timer <= 0) {
            e->state = AI_CHASE;
            e->timer = t->rest;
        }
        break;
    default:
        break;
    }
}
/* Fallen ordinary boars count down only while the hero plays on the pass.
 * When due, one returns at its spawn with full health, but only out of the
 * hero's sight; otherwise it waits at zero until the hero moves away. */
static void tick_hunt_respawn(Game *g, float dt) {
    if (!game_hunt_active(g) || g->phase != PH_PLAY || dt <= 0) return;
    for (int i = 0; i < HUNT_COUNT; i++) {
        Enemy *e = &g->hunt[i];
        if (e->hp > 0) continue;
        g->hunt_respawn[i] = fmaxf(0, g->hunt_respawn[i] - dt);
        if (g->hunt_respawn[i] > 0 || e->dying > 0 || distance(g->hero.pos, HUNT_SPAWNS[i]) < HUNT_RESPAWN_CLEAR)
            continue;
        *e = make_enemy(HUNT_SPAWNS[i], BOAR_HP);
        effect(g, (Effect){.kind = FX_DUST, .pos = e->pos, .life = .55f, .amount = 3});
    }
}
/* Shared herb/material magnet: same delay, acceleration, radius and wall
 * checks. Caller owns the single inventory transaction when this returns true. */
static bool magnet_drop(Game *g, Drop *drop, float dt) {
    drop->attracting = false;
    drop->age += dt;
    if (drop->age < DROP_REST_SECONDS) return false;
    float d = distance(g->hero.pos, drop->pos);
    if (d > MAGNET_RADIUS || !world_segment(drop->pos, g->hero.pos, 0)) {
        drop->magnet_time = 0;
        return false;
    }
    drop->attracting = true;
    drop->magnet_time += dt;
    if (d > COLLECT_RADIUS) {
        float speed = 70 + 150 * clampf(drop->magnet_time / .7f, 0, 1);
        float step = fminf(d, speed * dt);
        drop->pos = world_move(drop->pos,
            (Vec2){(g->hero.pos.x - drop->pos.x) * step / d,
                   (g->hero.pos.y - drop->pos.y) * step / d}, 0);
    }
    return drop->magnet_time >= .25f && distance(g->hero.pos, drop->pos) <= COLLECT_RADIUS;
}
static void tick_loot(Game *g, float dt) {
    world_select(g->map);
    if (g->training.active || (g->map != MAP_PASS && g->map != MAP_TRAINING) ||
        g->phase != PH_PLAY || dt <= 0) return;
    if (g->quest == Q_RECOVER)
        for (int i = 0; i < BOAR_COUNT; i++)
            if (g->drops[i].exists && !g->drops[i].collected && magnet_drop(g, &g->drops[i], dt))
                game_collect(g, i);
    if (g->map == MAP_PASS)
        for (int i = 0; i < MATERIAL_DROP_MAX; i++) {
            MaterialDrop *pile = &g->material_drops[i];
            if (pile->quantity <= 0) continue;
            bool arrived = magnet_drop(g, &pile->drop, dt);
            pile->drop.age = fminf(DROP_REST_SECONDS, pile->drop.age);
            pile->drop.magnet_time = fminf(1, pile->drop.magnet_time);
            if (arrived) game_collect_material(g, i);
        }
}
/* One combined, deferred notice in the existing slot, without masking
 * combat/quest toasts or creating a second HUD overlay beside saving. It
 * runs on every map, so a pickup made just before leaving the pass is
 * still announced once the arrival toast clears. */
static void flush_material_notice(Game *g) {
    if (g->toast_time <= 0 && (g->material_notice[0] || g->material_notice[1])) {
        char text[160];
        snprintf(text, sizeof text, "재료 회수 · 가죽 +%d · 뼈 +%d", g->material_notice[0], g->material_notice[1]);
        game_toast(g, text);
        memset(g->material_notice, 0, sizeof g->material_notice);
    }
}
static void chain_move(Game *g, int move) {
    world_select(g->map);
    g->input_count = g->input_time > 0 && g->last_move != move
        ? (int)fminf(COMBO_MAX, g->input_count + 1) : 1;
    g->last_move = move;
    g->input_time = COMBO_SECONDS;
}
static int next_combo(const Game *g, int move) {
    world_select(g->map);
    return g->combo_time > 0 && g->last_hit_move != move
        ? (int)fminf(COMBO_MAX, g->combo_count + 1) : 1;
}
static int combo_damage(const Game *g, int move, int damage) {
    world_select(g->map);
    int scaled = (damage * (100 + 10 * (next_combo(g, move) - 1)) + 50) / 100;
    return g->perfect_bonus > 0 ? (scaled * (100 + PERFECT_BONUS_PERCENT) + 50) / 100 : scaled;
}
/* Basic attack reach: the blade, and the wind blade that extends it. */
#define BLADE_RANGE 58.0f
#define BLADE_RANGE_STEEL 76.0f
#define WIND_RANGE 120.0f
static float blade_range(const Game *g) {
    world_select(g->map);
    return g->augment == AUG_STEEL ? BLADE_RANGE_STEEL : BLADE_RANGE;
}
float game_attack_range(const Game *g, int move) {
    world_select(g->map);
    if (move == MOVE_ATTACK)
        return g->augment == AUG_WIND ? WIND_RANGE : blade_range(g);
    return move >= 0 && move < TECH_COUNT ? TECHNIQUES[move].range : 0;
}
float game_attack_half_angle(int move) {
    /* Radians: basic 103deg, cleave 63deg, sweep 201deg,
     * thrust 32deg, retreat 120deg total opening. */
    switch (move) {
    case MOVE_ATTACK: return .90f;
    case TECH_CLEAVE: return .55f;
    case TECH_SWEEP: return 1.75f;
    case TECH_THRUST: return .28f;
    case TECH_RETREAT: return 1.05f;
    default: return 0;
    }
}
bool game_attack_contains(Vec2 origin, float angle, float range, int move, Vec2 target) {
    float dx = target.x - origin.x, dy = target.y - origin.y;
    float d = hypotf(dx, dy), half = game_attack_half_angle(move);
    if (half <= 0 || !isfinite(angle) || !(d < range)) return false;
    /* Coincident centers count as contact; never normalize a zero vector. */
    if (d > .0001f && (dx * cosf(angle) + dy * sinf(angle)) / d < cosf(half) - .000001f)
        return false;
    return world_segment(origin, target, 0);
}
/* Sweep the forward sector along the collision-resolved advance. This includes
 * contact passed during a thrust without granting a radial hit behind it.
 * Clip the origin segment against the two cone planes, then find its nearest
 * point to the target. Forward moves have half angles strictly below pi/2. */
bool game_attack_path_contains(Vec2 start, Vec2 end, float angle, float range, int move, Vec2 target) {
    float half = game_attack_half_angle(move);
    Vec2 step = {end.x - start.x, end.y - start.y};
    float length2 = step.x * step.x + step.y * step.y;
    if (length2 < .000001f || half >= 1.5707963f)
        return game_attack_contains(end, angle, range, move, target);
    float c = cosf(angle), s = sinf(angle), tangent = tanf(half);
    Vec2 delta = {target.x - start.x, target.y - start.y};
    float x = delta.x * c + delta.y * s, y = -delta.x * s + delta.y * c;
    float vx = step.x * c + step.y * s, vy = -step.x * s + step.y * c;
    float low = 0, high = 1;
    for (int side = -1; side <= 1; side += 2) {
        float a = x * tangent + side * y, b = vx * tangent + side * vy;
        if (fabsf(b) < .000001f) { if (a < 0) return false; }
        else if (b > 0) high = fminf(high, a / b);
        else low = fmaxf(low, a / b);
    }
    if (low > high) return false;
    float t = clampf((delta.x * step.x + delta.y * step.y) / length2, low, high);
    Vec2 origin = {start.x + step.x * t, start.y + step.y * t};
    return game_attack_contains(origin, angle, range, move, target);
}
static void combat_feedback(Game *g, int move, int result) {
    world_select(g->map);
    g->input_move = move;
    g->combat_request = result;
    g->request_time = .8f;
}
void game_invalid_direction(Game *g) {
    world_select(g->map);
    combat_feedback(g, MOVE_NONE, TRAIN_INVALID);
}
static void begin_attack(Game *g, Enemy *enemy) {
    world_select(g->map);
    Hero *h = &g->hero;
    h->attacking = true; h->attack_hit = false; h->attack_elapsed = 0;
    h->attack_target = !enemy ? -2 : enemy == &g->boss ? -1
        : is_hunt(g, enemy) ? BOAR_COUNT + (int)(enemy - g->hunt) : (int)(enemy - g->boars);
    h->attack_angle = enemy ? atan2f(enemy->pos.y - h->pos.y, enemy->pos.x - h->pos.x)
        : atan2f(h->dir.y, h->dir.x);
    if (enemy && fabsf(enemy->pos.x - h->pos.x) > .1f)
        h->face = enemy->pos.x > h->pos.x ? 1 : -1;
    h->attack_face = h->face;
    g->attack_cooldown = .75f;
    chain_move(g, MOVE_ATTACK);
}
static void begin_technique(Game *g, Technique technique) {
    world_select(g->map);
    Hero *h = &g->hero;
    Enemy *e = nearest_enemy(g);
    h->skill_angle = e && distance(e->pos, h->pos) < 180
        ? atan2f(e->pos.y - h->pos.y, e->pos.x - h->pos.x)
        : atan2f(h->dir.y, h->dir.x);
    if (fabsf(cosf(h->skill_angle)) > .1f)
        h->face = cosf(h->skill_angle) > 0 ? 1 : -1;
    h->attack_face = h->face;
    h->attacking = false; h->skilling = true; h->skill_hit = false;
    h->skill_elapsed = 0; h->technique = technique;
    float cooldown = technique == TECH_CLEAVE && g->augment == AUG_FLOW
        ? 4 : TECHNIQUES[technique].cooldown;
    if (technique == TECH_CLEAVE) g->cooldown = cooldown;
    else g->technique_cooldowns[technique] = cooldown;
    if (technique == TECH_RETREAT || technique == TECH_THRUST)
        h->invul = fmaxf(h->invul, .38f);
    chain_move(g, technique);
}
/* After its impact, a swing's recovery can be cut by a different move. The
 * same move repeated still waits, so mixing moves is the faster chain. */
static bool cancels_recovery(const Game *g, int move) {
    const Hero *h = &g->hero;
    return g->hitstop_time <= 0 &&
           ((h->attacking && h->attack_hit && move != MOVE_ATTACK) ||
            (h->skilling && h->skill_hit && move != (int)h->technique));
}
static bool request_move(Game *g, int move) {
    world_select(g->map);
    if (move != MOVE_ATTACK && (move < 0 || move >= TECH_COUNT)) {
        game_invalid_direction(g);
        return false;
    }
    if (g->phase != PH_PLAY || g->hero.hp <= 0) {
        combat_feedback(g, move, TRAIN_BLOCKED);
        return false;
    }
    if (move == MOVE_ATTACK ? g->attack_cooldown > 0
                            : game_technique_cooldown(g, (Technique)move) > 0) {
        combat_feedback(g, move, TRAIN_COOLDOWN);
        return false;
    }
    if (cancels_recovery(g, move)) {
        Hero *h = &g->hero;
        h->attacking = h->skilling = false;
        h->attack_elapsed = h->skill_elapsed = 0;
        effect(g, (Effect){.kind = FX_HIT, .pos = h->pos, .life = .15f,
                           .target = {h->pos.x + h->dir.x * 18, h->pos.y + h->dir.y * 18 - 20}});
        g->cancel_count++;
    }
    float remaining = g->hero.skilling ? HERO_SKILL_SECONDS - g->hero.skill_elapsed
        : g->hero.attacking ? HERO_ATTACK_SECONDS - g->hero.attack_elapsed
        : g->hero.dodging ? DODGE_SECONDS - g->hero.dodge_elapsed : 0;
    if (remaining > 0 || g->hitstop_time > 0) {
        /* Keep one intentional follow-up across the entire active animation.
         * The last valid input replaces it; no held direction auto-repeats.
         * Its lifetime covers the remaining animation, not just 0.28 s. */
        g->queued_move = move;
        g->buffer_time = fmaxf(0, remaining) + .10f;
        combat_feedback(g, move, TRAIN_QUEUED);
        return true;
    }
    if (move == MOVE_ATTACK) {
        Enemy *e = nearest_enemy(g);
        if (e && distance(e->pos, g->hero.pos) >= game_attack_range(g, MOVE_ATTACK) + 22) e = NULL;
        begin_attack(g, e);
    } else begin_technique(g, (Technique)move);
    combat_feedback(g, move, TRAIN_STARTED);
    return true;
}
static void break_chain(Game *g) {
    if (g->chain_hits >= 2) {
        g->chain_last = g->chain_hits;
        g->chain_fade = CHAIN_FADE_SECONDS;
    }
    g->chain_hits = 0;
    g->chain_pop = 0;
}
static void finish_impact(Game *game, int move, bool hit) {
    world_select(game->map);
    game->perfect_bonus = 0;
    if (hit) {
        /* Skill cycling: a landed hit brings the other techniques back sooner. */
        for (int t = 0; t < TECH_COUNT; t++) {
            if (t == move) continue;
            float *cd = t == TECH_CLEAVE ? &game->cooldown : &game->technique_cooldowns[t];
            *cd = fmaxf(0, *cd - COOLDOWN_REFUND_SECONDS);
        }
        game->chain_hits = game->combo_time > 0 ? game->chain_hits + 1 : 1;
        game->chain_pop = CHAIN_POP_SECONDS;
        game->chain_fade = 0;
    } else break_chain(game);
    game->input_time = COMBO_SECONDS;
    game->last_impact = hit ? IMPACT_HIT : IMPACT_MISS;
    game->combo_count = hit ? next_combo(game, move) : 0;
    game->last_hit_move = hit ? move : MOVE_NONE;
    game->combo_time = hit ? COMBO_SECONDS : 0;
    if (!game->training.active) return;
    game->training.last_damage = game->training.impact_damage;
    if (hit) game->training.hits++;
    else game->training.misses++;
    game->training.impact_damage = 0;
}
static Enemy *unlocked_attack_target(Game *g) {
    Enemy *best = NULL;
    float nearest = game_attack_range(g, MOVE_ATTACK);
    Enemy *enemies[1 + BOAR_COUNT + HUNT_COUNT] = {&g->boss};
    for (int i = 0; i < BOAR_COUNT; i++) enemies[1 + i] = &g->boars[i];
    for (int i = 0; i < HUNT_COUNT; i++) enemies[1 + BOAR_COUNT + i] = &g->hunt[i];
    for (size_t i = 0; i < sizeof enemies / sizeof *enemies; i++) {
        Enemy *e = enemies[i];
        if (active_enemy(g, e) && e->hp > 0 &&
            game_attack_contains(g->hero.pos, g->hero.attack_angle, nearest, MOVE_ATTACK, e->pos)) {
            best = e;
            nearest = distance(g->hero.pos, e->pos);
        }
    }
    return best;
}
static void tick_attack(Game *g, float dt) {
    world_select(g->map);
    Hero *h = &g->hero;
    if (!h->attacking) return;
    h->attack_elapsed += dt;
    if (!h->attack_hit && h->attack_elapsed >= HERO_ATTACK_IMPACT) {
        h->attack_hit = true;
        h->impact_start = h->impact_origin = h->pos;
        bool hit = false;
        g->training.impact_damage = 0;
        Enemy *e = h->attack_target == -1 ? &g->boss
            : h->attack_target >= 0 && h->attack_target < BOAR_COUNT
                ? &g->boars[h->attack_target]
            : h->attack_target >= BOAR_COUNT && h->attack_target < BOAR_COUNT + HUNT_COUNT
                ? &g->hunt[h->attack_target - BOAR_COUNT] : unlocked_attack_target(g);
        if (e && active_enemy(g, e) && e->hp > 0 &&
            game_attack_contains(h->pos, h->attack_angle, game_attack_range(g, MOVE_ATTACK), MOVE_ATTACK, e->pos)) {
            float d = distance(h->pos, e->pos);
            if (d < blade_range(g))
                hit |= game_damage(g, e, combo_damage(g, MOVE_ATTACK, (g->augment == AUG_STEEL ? 13 : 9) + (g->sword ? 5 : 0)), FX_HIT);
            if (g->augment == AUG_WIND && d < WIND_RANGE)
                hit |= game_damage(g, e, 6, FX_WIND);
        }
        finish_impact(g, MOVE_ATTACK, hit);
    }
    if (h->attack_elapsed >= HERO_ATTACK_SECONDS) {
        h->attacking = false; h->attack_elapsed = 0;
    }
}
static bool technique_area(Game *g, float range, int damage) {
    world_select(g->map);
    bool hit = false;
    if (g->training.active || ((g->map == MAP_CAVE || g->map == MAP_TRAINING) && g->quest == Q_COMBAT)) {
        if (game_attack_path_contains(g->hero.impact_start, g->hero.impact_origin,
                g->hero.skill_angle, range, g->hero.technique, g->boss.pos))
            hit |= game_damage(g, &g->boss, damage, FX_HIT);
    } else {
        if (game_boars_active(g))
            for (int i = 0; i < BOAR_COUNT; i++)
                if (game_attack_path_contains(g->hero.impact_start, g->hero.impact_origin,
                        g->hero.skill_angle, range, g->hero.technique, g->boars[i].pos))
                    hit |= game_damage(g, &g->boars[i], damage, FX_HIT);
        if (game_hunt_active(g))
            for (int i = 0; i < HUNT_COUNT; i++)
                if (game_attack_path_contains(g->hero.impact_start, g->hero.impact_origin,
                        g->hero.skill_angle, range, g->hero.technique, g->hunt[i].pos))
                    hit |= game_damage(g, &g->hunt[i], damage, FX_HIT);
    }
    return hit;
}
static void tick_technique(Game *g, float dt) {
    world_select(g->map);
    Hero *h = &g->hero;
    if (!h->skilling) return;
    h->skill_elapsed += dt;
    if (!h->skill_hit && h->skill_elapsed >= HERO_SKILL_IMPACT) {
        h->skill_hit = true;
        g->training.impact_damage = 0;
        const TechniqueInfo *info = &TECHNIQUES[h->technique];
        h->impact_start = h->pos;
        float step = h->technique == TECH_CLEAVE && g->augment == AUG_FLOW ? 45 : info->step;
        if (step != 0)
            effect(g, (Effect){.kind = FX_DUST, .pos = h->pos, .life = .45f, .amount = 2,
                               .angle = step > 0 ? h->skill_angle : h->skill_angle + 3.14159265f});
        if (step > 0)
            h->pos = world_move(h->pos, (Vec2){cosf(h->skill_angle) * step,
                                             sinf(h->skill_angle) * step}, HERO_RADIUS);
        int base = h->technique == TECH_CLEAVE && g->augment == AUG_FLOW ? 32 : info->damage;
        h->impact_origin = h->pos;
        bool hit = technique_area(g, info->range, combo_damage(g, h->technique, base + (g->sword ? 5 : 0)));
        finish_impact(g, h->technique, hit);
        if (step < 0)
            h->pos = world_move(h->pos, (Vec2){cosf(h->skill_angle) * step,
                                             sinf(h->skill_angle) * step}, HERO_RADIUS);
        effect(g, (Effect){.kind = FX_SKILL, .pos = h->impact_origin,
                            .angle = h->skill_angle, .life = .30f});
    }
    if (h->skill_elapsed >= HERO_SKILL_SECONDS) {
        h->skilling = false; h->skill_elapsed = 0;
    }
}
int game_hero_skill_frame(const Game *g) {
    world_select(g->map);
    if (g->phase != PH_PLAY || !g->hero.skilling) return -1;
    const int cleave[] = {0,1,1,2,3,4,5,5};
    const int sweep[] = {0,0,1,2,3,3,4,5};
    const int thrust[] = {0,0,1,2,3,4,5,5};
    int tick = (int)clampf(g->hero.skill_elapsed / HERO_SKILL_SECONDS * 8, 0, 7);
    return g->hero.technique == TECH_SWEEP ? sweep[tick]
        : g->hero.technique == TECH_THRUST ? thrust[tick] : cleave[tick];
}
int game_hero_attack_frame(const Game *g) {
    world_select(g->map);
    if (g->phase != PH_PLAY || !g->hero.attacking)
        return -1;
    int frame = (int)(g->hero.attack_elapsed / HERO_ATTACK_SECONDS * HERO_ATTACK_FRAMES);
    return frame < HERO_ATTACK_FRAMES ? frame : HERO_ATTACK_FRAMES - 1;
}
int game_hero_frame(const Game *g) {
    world_select(g->map);
    if (g->phase != PH_PLAY || !g->hero.walking || g->hero.attacking || g->hero.skilling)
        return -1;
    return (int)(g->hero.walk_cycle * HERO_WALK_FRAMES) % HERO_WALK_FRAMES;
}
/* Falls play out behind story dialogue too; menus and hitstop hold them. */
static void tick_falls(Game *g, float dt) {
    world_select(g->map);
    g->boss.dying = fmaxf(0, g->boss.dying - dt);
    for (int i = 0; i < BOAR_COUNT; i++)
        g->boars[i].dying = fmaxf(0, g->boars[i].dying - dt);
    for (int i = 0; i < HUNT_COUNT; i++)
        g->hunt[i].dying = fmaxf(0, g->hunt[i].dying - dt);
}
/* Integrate the decaying push exactly over dt, through terrain collision. */
static void knock_back(Vec2 *pos, Vec2 *knock, float radius, float dt) {
    if (fabsf(knock->x) + fabsf(knock->y) < .5f) { *knock = (Vec2){0, 0}; return; }
    float keep = expf(-KNOCK_DECAY * dt), travel = (1 - keep) / KNOCK_DECAY;
    Vec2 before = *pos;
    *pos = world_move(*pos, (Vec2){knock->x * travel, knock->y * travel}, radius);
    if (distance(before, *pos) < .001f) { *knock = (Vec2){0, 0}; return; }
    knock->x *= keep;
    knock->y *= keep;
}
float game_arrival_strength(const Game *g, Vec2 direction, float distance, float dt) {
    world_select(g->map);
    dt = fmaxf(.001f, dt);
    float decay = expf(-dt / HERO_ACCEL_SECONDS), lag = HERO_ACCEL_SECONDS * (1 - decay);
    float speed = g->hero.vel.x * direction.x + g->hero.vel.y * direction.y;
    /* This step's travel plus the eased stop afterwards should cover the gap. */
    float reach = (dt - lag) + (1 - decay) * HERO_DECEL_SECONDS;
    float carried = speed * (lag + decay * HERO_DECEL_SECONDS);
    return clampf((distance - carried) / reach / HERO_SPEED, 0, 1);
}

void game_tick(Game *g, float dt, Vec2 input) {
    world_select(g->map);
    if (!isfinite(dt) || !isfinite(input.x) || !isfinite(input.y))
        return;
    dt = clampf(dt, 0, .05f);
    if (g->hitstop_time <= 0 && (g->phase == PH_PLAY || g->phase == PH_DIALOGUE ||
                                 g->phase == PH_AUGMENT || g->phase == PH_COMPLETE))
        tick_falls(g, dt);
    if (g->phase != PH_PLAY) {
        g->hero.walking = false;
        g->hero.walk_cycle = 0;
        g->queued_move = MOVE_NONE; g->buffer_time = 0;
        game_clear_hit_feedback(g);
        return;
    }
    /* Consume only the stopped part of this frame. A partial final frame must
     * not drop or double-count time, regardless of rendering frequency. */
    if (g->hitstop_time > 0) {
        float stopped = fminf(dt, g->hitstop_time);
        g->hitstop_time = fmaxf(0, g->hitstop_time - stopped);
        dt -= stopped;
        if (dt <= 0) return;
    }
    g->time += dt;
    /* Gwanpa slow motion: enemies run slower, the hero keeps full speed. */
    float world_dt = g->perfect_slow > 0 ? dt * PERFECT_SLOW_SCALE : dt;
    g->perfect_slow = fmaxf(0, g->perfect_slow - dt);
    g->toast_time = fmaxf(0, g->toast_time - dt);
    float length = hypotf(input.x, input.y);
    Vec2 before = g->hero.pos, target = {0, 0};
    if (length > .05f) {
        float strength = fminf(1, length);
        input.x /= length;
        input.y /= length;
        float speed = HERO_SPEED * strength *
            (g->hero.attacking || g->hero.skilling ? HERO_ATTACK_MOVE_SCALE : 1);
        target = (Vec2){input.x * speed, input.y * speed};
        g->hero.dir = input;
        if (fabsf(input.x) > .1f)
            g->hero.face = input.x > 0 ? 1 : -1;
    }
    /* Ease into and out of a run; facing still turns instantly. The
     * velocity curve is integrated exactly over the step, so the distance
     * covered does not depend on the frame rate. */
    float tau = length > .05f ? HERO_ACCEL_SECONDS : HERO_DECEL_SECONDS;
    float decay = expf(-dt / tau), lag = tau * (1 - decay);
    Vec2 was = g->hero.vel;
    g->hero.vel = (Vec2){target.x + (was.x - target.x) * decay, target.y + (was.y - target.y) * decay};
    Vec2 step = {target.x * dt + (was.x - target.x) * lag, target.y * dt + (was.y - target.y) * lag};
    if (length <= .05f && hypotf(g->hero.vel.x, g->hero.vel.y) < 2)
        g->hero.vel = (Vec2){0, 0};
    if (g->hero.dodging) {
        /* Ease-out dash: distance(t) = D * (1 - (1 - t)^2), exact per step.
         * It leaves at walking speed so the run carries on smoothly. */
        Hero *h = &g->hero;
        float t0 = h->dodge_elapsed / DODGE_SECONDS;
        h->dodge_elapsed = fminf(DODGE_SECONDS, h->dodge_elapsed + dt);
        float t1 = h->dodge_elapsed / DODGE_SECONDS;
        float covered = DODGE_DISTANCE * ((1 - t0) * (1 - t0) - (1 - t1) * (1 - t1));
        step = (Vec2){h->dodge_dir.x * covered, h->dodge_dir.y * covered};
        h->dir = h->dodge_dir;
        if (fabsf(h->dodge_dir.x) > .1f) h->face = h->dodge_dir.x > 0 ? 1 : -1;
        if (h->dodge_elapsed >= DODGE_SECONDS) {
            h->dodging = false;
            h->vel = (Vec2){h->dodge_dir.x * HERO_SPEED, h->dodge_dir.y * HERO_SPEED};
        } else h->vel = (Vec2){0, 0};
    }
    if (step.x != 0 || step.y != 0) {
        g->hero.pos = world_move(g->hero.pos, step, HERO_RADIUS);
        /* Keep only the motion the terrain allowed, so walls never store speed. */
        float moved_x = g->hero.pos.x - before.x, moved_y = g->hero.pos.y - before.y;
        if (fabsf(moved_x - step.x) > .0001f)
            g->hero.vel.x = step.x != 0 ? g->hero.vel.x * clampf(moved_x / step.x, 0, 1) : 0;
        if (fabsf(moved_y - step.y) > .0001f)
            g->hero.vel.y = step.y != 0 ? g->hero.vel.y * clampf(moved_y / step.y, 0, 1) : 0;
    }
    /* Collision-resolved displacement drives the feet, including wall slides.
     * Holding a key into a wall must not make the hero march on the spot. */
    float travelled = distance(before, g->hero.pos);
    g->hero.walking = travelled > .0001f;
    float stepped = g->hero.walk_cycle;
    g->hero.walk_cycle = g->hero.walking
        ? fmodf(g->hero.walk_cycle + travelled / HERO_WALK_DISTANCE, 1.0f) : 0;
    /* A small puff each time a foot lands (twice per stride). */
    if (g->hero.walking && (int)(stepped * 2) != (int)(g->hero.walk_cycle * 2))
        effect(g, (Effect){.kind = FX_DUST, .pos = g->hero.pos, .life = .35f, .amount = 1,
                           .angle = atan2f(g->hero.dir.y, g->hero.dir.x)});
    knock_back(&g->hero.pos, &g->hero.knock, HERO_RADIUS, dt);
    if (g->map == MAP_CAVE || g->map == MAP_TRAINING)
        knock_back(&g->boss.pos, &g->boss.knock, BOSS_RADIUS, world_dt);
    for (int i = 0; (g->map == MAP_PASS || g->map == MAP_TRAINING) && i < BOAR_COUNT; i++)
        knock_back(&g->boars[i].pos, &g->boars[i].knock, BOAR_RADIUS, world_dt);
    for (int i = 0; g->map == MAP_PASS && i < HUNT_COUNT; i++)
        knock_back(&g->hunt[i].pos, &g->hunt[i].knock, BOAR_RADIUS, world_dt);
    g->cooldown = fmaxf(0, g->cooldown - dt);
    for (int i = 0; i < TECH_COUNT; i++)
        g->technique_cooldowns[i] = fmaxf(0, g->technique_cooldowns[i] - dt);
    g->attack_cooldown = fmaxf(0, g->attack_cooldown - dt);
    g->combo_time = fmaxf(0, g->combo_time - dt);
    g->input_time = fmaxf(0, g->input_time - dt);
    g->request_time = fmaxf(0, g->request_time - dt);
    g->chain_pop = fmaxf(0, g->chain_pop - dt);
    g->perfect_bonus = fmaxf(0, g->perfect_bonus - dt);
    g->perfect_flash = fmaxf(0, g->perfect_flash - dt);
    if (g->dodge_charges < DODGE_CHARGES) {
        g->dodge_recharge -= dt;
        if (g->dodge_recharge <= 0) {
            g->dodge_charges++;
            g->dodge_recharge = g->dodge_charges < DODGE_CHARGES ? DODGE_RECHARGE_SECONDS : 0;
        }
    }
    g->chain_fade = fmaxf(0, g->chain_fade - dt);
    if (g->combo_time <= 0) { g->combo_count = 0; g->last_hit_move = MOVE_NONE; break_chain(g); }
    if (g->input_time <= 0) { g->input_count = 0; g->last_move = MOVE_NONE; }
    g->buffer_time = fmaxf(0, g->buffer_time - dt);
    if (g->buffer_time <= 0) g->queued_move = MOVE_NONE;
    g->item_cooldown = fmaxf(0, g->item_cooldown - dt);
    g->training.hit_time = fmaxf(0, g->training.hit_time - dt);
    g->hero.invul = fmaxf(0, g->hero.invul - dt);
    g->hero.hurt_time = fmaxf(0, g->hero.hurt_time - dt);
    g->boss.hit_time = fmaxf(0, g->boss.hit_time - dt);
    for (int i = 0; i < BOAR_COUNT; i++)
        g->boars[i].hit_time = fmaxf(0, g->boars[i].hit_time - dt);
    for (int i = 0; i < HUNT_COUNT; i++)
        g->hunt[i].hit_time = fmaxf(0, g->hunt[i].hit_time - dt);
    /* Compact in place, keeping spawn order so overlapping effects never
     * swap drawing order when an older one ends. */
    int kept = 0;
    for (int i = 0; i < g->effect_count; i++) {
        g->effects[i].life -= dt;
        if (g->effects[i].life > 0)
            g->effects[kept++] = g->effects[i];
    }
    g->effect_count = kept;
    if (!g->training.active && (g->map == MAP_CAVE || g->map == MAP_TRAINING) && g->quest == Q_HUNT && distance(g->hero.pos, g->boss.pos) < 165 &&
        world_segment(g->hero.pos, g->boss.pos, 0)) {
        g->phase = PH_DIALOGUE;
        g->notices |= NOTICE_BOSS;
        return;
    }
    if (g->hero.bleed > 0) {
        g->hero.bleed = fmaxf(0, g->hero.bleed - dt);
        g->hero.bleed_tick -= dt;
        if (g->hero.bleed_tick <= 0) {
            g->hero.bleed_tick = 1;
            g->hero.hp = (int)fmaxf(0, (float)g->hero.hp - 3);
            dead(g);
            if (g->phase == PH_DEAD)
                return;
        }
    }
    tick_attack(g, dt);
    tick_technique(g, dt);
    if (g->queued_move != MOVE_NONE && !g->hero.dodging &&
        ((!g->hero.attacking && !g->hero.skilling) || cancels_recovery(g, g->queued_move))) {
        int next = g->queued_move;
        g->queued_move = MOVE_NONE; g->buffer_time = 0;
        request_move(g, next);
    }
    if (g->training.active) return;
    if ((g->map == MAP_CAVE || g->map == MAP_TRAINING) && g->quest == Q_COMBAT)
        tick_boss(g, world_dt);
    else {
        if (game_boars_active(g))
            for (int i = 0; i < BOAR_COUNT && g->phase == PH_PLAY; i++)
                if (g->boars[i].hp > 0)
                    tick_boar(g, &g->boars[i], world_dt);
        if (game_hunt_active(g))
            for (int i = 0; i < HUNT_COUNT && g->phase == PH_PLAY; i++)
                if (g->hunt[i].hp > 0)
                    tick_boar(g, &g->hunt[i], world_dt);
        tick_hunt_respawn(g, dt);
    }
    separate_bodies(g, dt);
    tick_loot(g, dt);
    flush_material_notice(g);
}

const MapExit MAP_EXITS[4] = {
    {MAP_VILLAGE,MAP_PASS,{940*1.5f,92*1.5f},{768*1.5f,840*1.5f}},
    {MAP_VILLAGE,MAP_CAVE,{1450*1.5f,635*1.5f},{768*1.5f,850*1.5f}},
    {MAP_PASS,MAP_VILLAGE,{768*1.5f,940*1.5f},{940*1.5f,180*1.5f}},
    {MAP_CAVE,MAP_VILLAGE,{768*1.5f,930*1.5f},{1350*1.5f,635*1.5f}}
};
const char *game_map_name(MapId map) {
    static const char *names[MAP_COUNT]={"마을","솔바람재","탐식굴","비밀 수련장"};
    return map >= 0 && map < MAP_COUNT ? names[map] : names[0];
}
const char *game_map_asset(MapId map) {
    static const char *paths[MAP_COUNT]={"maps/village.png","maps/solbaramjae.png","maps/tamsikgul.png","village.png"};
    return map >= 0 && map < MAP_COUNT ? paths[map] : paths[0];
}
int game_exit_nearby(const Game *g) {
    world_select(g->map);
    if (g->training.active || g->phase != PH_PLAY || g->quest == Q_COMBAT || g->hero.hp <= 0) return -1;
    for(int i=0;i<4;i++)
        if(MAP_EXITS[i].from == g->map && distance(g->hero.pos,MAP_EXITS[i].pos)<90 &&
           world_segment(g->hero.pos,MAP_EXITS[i].pos,HERO_RADIUS)) return i;
    return -1;
}
bool game_travel(Game *g, int index) {
    world_select(g->map);
    if(index<0 || index>=4 || game_exit_nearby(g)!=index) return false;
    MapId from=g->map;
    world_select(MAP_EXITS[index].to);
    if(!world_clear(MAP_EXITS[index].arrival,HERO_RADIUS)) { world_select(from);return false; }
    g->map=MAP_EXITS[index].to;
    g->hero.pos=MAP_EXITS[index].arrival;
    g->hero.attacking=g->hero.skilling=g->hero.walking=g->hero.dodging=false;
    g->hero.dodge_elapsed=0;g->hero.dodge_perfect=false;
    g->perfect_slow=g->perfect_bonus=g->perfect_flash=0;
    g->hero.vel=g->hero.knock=(Vec2){0,0};
    g->hero.attack_elapsed=g->hero.skill_elapsed=g->hero.walk_cycle=0;
    g->hero.attack_target=-1;
    g->queued_move=g->last_move=g->last_hit_move=g->input_move=MOVE_NONE;
    g->combo_count=g->input_count=g->chain_hits=g->chain_last=0;
    g->chain_pop=g->chain_fade=0;
    g->combo_time=g->input_time=g->buffer_time=g->request_time=0;
    g->last_impact=IMPACT_NONE;g->combat_request=TRAIN_READY;
    g->effect_count=0;g->notices=0;
    game_clear_hit_feedback(g);
    for(int i=-1;i<BOAR_COUNT;i++) {
        Enemy *e=i<0?&g->boss:&g->boars[i];
        e->route=(Route){0};e->repath=0;e->walking=false;e->knock=(Vec2){0,0};
        if(e->hp>0) { e->pos=e->home;e->state=AI_IDLE;e->timer=.5f; }
    }
    for(int i=0;i<HUNT_COUNT;i++) {
        Enemy *e=&g->hunt[i];
        e->route=(Route){0};e->repath=0;e->walking=false;e->knock=(Vec2){0,0};
        if(e->hp>0) { e->pos=e->home;e->state=AI_IDLE;e->timer=.5f; }
    }
    /* Walking in from the village starts a new hunting round. */
    if(from==MAP_VILLAGE && g->map==MAP_PASS) new_hunt_round(g);
    for(int i=0;i<BOAR_COUNT;i++) { g->drops[i].attracting=false;g->drops[i].magnet_time=0; }
    for (int i=0;i<MATERIAL_DROP_MAX;i++) {
        g->material_drops[i].drop.attracting=false;
        g->material_drops[i].drop.magnet_time=0;
    }
    game_toast(g,game_map_name(g->map));
    return true;
}
