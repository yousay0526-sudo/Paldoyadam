#include "raid.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

const RaidAugmentInfo RAID_AUGMENTS[RAID_AUG_COUNT] = {
    {"넓은 베기", "기본 공격 범위 +6 · 피해 +2"},
    {"검풍", "기본 공격에 바람 칼날 · 바람 피해 +4"},
    {"파고들기", "무공 피해 +3"},
    {"피의 기운", "적을 쓰러뜨릴 때마다 체력 +3"},
    {"무공 단련", "무공 재사용 시간 -15%"},
    {"날랜 보법", "보법 재충전 -0.2초"}};

static int stack(const Game *g, RaidAugment a) {
    return g->raid.active ? g->raid.stacks[a] : 0;
}
static uint32_t next(Raid *r) {
    uint32_t x = r->rng ? r->rng : 0x52414944u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return r->rng = x;
}
bool raid_available(const Game *g) {
    return !g->training.active && !g->raid.active && g->map == MAP_VILLAGE &&
           g->traces[0] && g->traces[1] && g->quest != Q_COMBAT && g->hero.hp > 0;
}
bool raid_offer(Game *g) {
    if (g->phase != PH_PLAY || !raid_available(g)) return false;
    memset(&g->raid, 0, sizeof g->raid);
    g->raid.stage = RAID_OFFER;
    g->phase = PH_RAID;
    return true;
}
void raid_decline(Game *g) {
    if (g->phase == PH_RAID && !g->raid.active && g->raid.stage == RAID_OFFER) g->phase = PH_PLAY;
}
bool raid_enter(Game *g, Game *saved) {
    if (g == saved || g->phase != PH_RAID || g->raid.stage != RAID_OFFER || !raid_available(g)) return false;
    *saved = *g;
    world_select(MAP_PASS);
    g->map = MAP_PASS;
    g->phase = PH_PLAY;
    Hero *h = &g->hero;
    h->pos = MAP_EXITS[0].arrival;
    h->hp = h->max_hp; h->bleed = h->bleed_tick = 0;
    h->attacking = h->skilling = h->walking = h->dodging = false;
    h->vel = h->knock = (Vec2){0, 0};
    h->attack_target = -1;
    g->queued_move = g->last_move = g->last_hit_move = g->input_move = MOVE_NONE;
    g->combo_count = g->input_count = g->chain_hits = g->chain_last = 0;
    g->cooldown = g->attack_cooldown = g->item_cooldown = 0;
    memset(g->technique_cooldowns, 0, sizeof g->technique_cooldowns);
    g->dodge_charges = DODGE_CHARGES; g->dodge_recharge = 0;
    g->perfect_slow = g->perfect_bonus = g->perfect_flash = 0;
    g->effect_count = 0; g->toast_time = 0;
    /* Quest boars and herb drops sit out the run; ordinary slots are the arena. */
    for (int i = 0; i < BOAR_COUNT; i++) { g->boars[i].hp = 0; g->boars[i].state = AI_DEAD; g->drops[i].exists = false; }
    for (int i = 0; i < HUNT_COUNT; i++) {
        g->hunt[i].hp = 0; g->hunt[i].state = AI_DEAD; g->hunt[i].dying = 0; g->hunt_respawn[i] = 0;
    }
    memset(g->material_drops, 0, sizeof g->material_drops);
    memset(&g->raid, 0, sizeof g->raid);
    g->raid.active = true;
    g->raid.elite = -1;
    g->raid.energy_need = RAID_FIRST_ENERGY;
    g->raid.spawn_timer = .5f;
    g->raid.rng = saved->loot_rng ^ 0x7A3Bu;
    game_toast(g, "짧은 토벌 시작 · 3분을 버티거나 정예를 쓰러뜨리십시오");
    return true;
}
static void finish(Game *g, bool success) {
    if (!g->raid.active || g->raid.stage == RAID_RESULT) return;
    g->raid.success = success;
    g->raid.stage = RAID_RESULT;
    g->phase = PH_RAID;
}
static int free_slot(const Game *g) {
    for (int i = 0; i < HUNT_COUNT; i++)
        if (g->hunt[i].hp <= 0 && g->hunt[i].dying <= 0) return i;
    return -1;
}
static int alive(const Game *g) {
    int n = 0;
    for (int i = 0; i < HUNT_COUNT; i++) n += g->hunt[i].hp > 0;
    return n;
}
static void spawn(Game *g, int slot, int hp) {
    /* A spawn point out of reach but close enough to arrive soon: a random one
     * 220..560 away, else the nearest beyond 220, else the farthest. */
    int pick = -1, near = -1, best = 0; float far = -1, near_d = 1e9f;
    int start = (int)(next(&g->raid) % HUNT_COUNT);
    for (int k = 0; k < HUNT_COUNT; k++) {
        int i = (start + k) % HUNT_COUNT;
        float d = distance(HUNT_SPAWNS[i], g->hero.pos);
        if (d > far) { far = d; best = i; }
        if (d >= 220 && d < near_d) { near_d = d; near = i; }
        if (pick < 0 && d >= 220 && d <= 560) pick = i;
    }
    if (pick < 0) pick = near >= 0 ? near : best;
    Enemy e = {0};
    e.pos = e.home = HUNT_SPAWNS[pick];
    e.hp = e.max_hp = hp;
    e.timer = .5f;
    e.facing = (Vec2){0, 1};
    e.state = AI_CHASE;
    g->hunt[slot] = e;
}
void raid_tick(Game *g, float dt) {
    Raid *r = &g->raid;
    if (!r->active || g->phase != PH_PLAY || dt <= 0) return;
    r->time += dt;
    /* The pack hunts the hero across the whole pass instead of guarding spawns. */
    for (int i = 0; i < HUNT_COUNT; i++) {
        Enemy *e = &g->hunt[i];
        if (e->hp <= 0) continue;
        e->home = g->hero.pos;
        /* A far boar walks in through AI_RETURN (home is the hero); a near one chases. */
        if (e->state == AI_IDLE) e->state = AI_CHASE;
    }
    if (r->time >= RAID_SECONDS) { r->time = RAID_SECONDS; finish(g, true); return; }
    if (!r->elite_spawned && r->time >= RAID_ELITE_AT) {
        int slot = free_slot(g);
        if (slot < 0) {
            /* Make room: the elite replaces the boar farthest from the hero. */
            float far = -1;
            for (int i = 0; i < HUNT_COUNT; i++) {
                float d = distance(g->hunt[i].pos, g->hero.pos);
                if (d > far) { far = d; slot = i; }
            }
        }
        spawn(g, slot, RAID_ELITE_HP);
        r->elite = slot; r->elite_spawned = true;
        game_toast(g, "정예 멧돼지 출현 · 쓰러뜨리면 토벌 성공");
        return;
    }
    /* Packs grow from 2 to 6 at once; spawns come faster as time goes on. */
    int cap = 2 + (int)(r->time / 36);
    if (cap > HUNT_COUNT) cap = HUNT_COUNT;
    r->spawn_timer -= dt;
    if (r->spawn_timer > 0) return;
    r->spawn_timer = fmaxf(1.2f, 3.0f - r->time / 75);
    int slot = free_slot(g);
    if (slot >= 0 && alive(g) < cap) spawn(g, slot, BOAR_HP);
}
static void open_pick(Game *g) {
    Raid *r = &g->raid;
    int open[RAID_AUG_COUNT], n = 0;
    for (int i = 0; i < RAID_AUG_COUNT; i++)
        if (r->stacks[i] < RAID_AUG_MAX_STACK) open[n++] = i;
    if (n == 0) return;
    r->offer_count = n < 3 ? n : 3;
    for (int k = 0; k < r->offer_count; k++) {
        int j = k + (int)(next(r) % (uint32_t)(n - k));
        int t = open[k]; open[k] = open[j]; open[j] = t;
        r->offer[k] = open[k];
    }
    r->stage = RAID_PICK;
    g->phase = PH_RAID;
}
void raid_on_kill(Game *g, Enemy *e) {
    Raid *r = &g->raid;
    if (!r->active) return;
    r->kills++;
    r->coins += RAID_KILL_COINS;
    int heal = 3 * stack(g, RAUG_DRAIN);
    if (heal) g->hero.hp = g->hero.hp + heal > g->hero.max_hp ? g->hero.max_hp : g->hero.hp + heal;
    if (raid_is_elite(g, e)) { r->elite = -1; finish(g, true); return; }
    if (++r->energy >= r->energy_need) {
        r->energy = 0;
        r->energy_need += RAID_ENERGY_STEP;
        r->level++;
        open_pick(g);
    }
}
bool raid_choose(Game *g, int offer_index) {
    Raid *r = &g->raid;
    if (!r->active || g->phase != PH_RAID || r->stage != RAID_PICK ||
        offer_index < 0 || offer_index >= r->offer_count) return false;
    int a = r->offer[offer_index];
    if (r->stacks[a] >= RAID_AUG_MAX_STACK) return false;
    r->stacks[a]++;
    r->offer_count = 0;
    g->phase = PH_PLAY;
    char text[160];
    snprintf(text, sizeof text, "%s %d단계", RAID_AUGMENTS[a].name, r->stacks[a]);
    game_toast(g, text);
    return true;
}
bool raid_abandon(Game *g) {
    if (!g->raid.active || g->raid.stage == RAID_RESULT) return false;
    finish(g, false);
    return true;
}
void raid_hero_down(Game *g) {
    if (g->raid.active) finish(g, false);
}
int raid_reward_coins(const Raid *r) {
    return r->success ? r->coins + RAID_WIN_COINS : r->coins / 2;
}
int raid_reward_shards(const Raid *r) {
    return r->success ? RAID_WIN_SHARDS : 0;
}
bool raid_leave(Game *g, const Game *saved) {
    if (g == saved || !g->raid.active || g->raid.stage != RAID_RESULT || g->phase != PH_RAID ||
        saved->raid.active || saved->phase != PH_RAID) return false;
    Raid result = g->raid;
    *g = *saved;
    memset(&g->raid, 0, sizeof g->raid);
    world_select(g->map);
    g->phase = PH_PLAY;
    int coins = raid_reward_coins(&result), shards = raid_reward_shards(&result);
    g->coins = g->coins > COIN_LIMIT - coins ? COIN_LIMIT : g->coins + coins;
    g->spirit_shards = g->spirit_shards > SPIRIT_LIMIT - shards ? SPIRIT_LIMIT : g->spirit_shards + shards;
    char text[192];
    if (shards)
        snprintf(text, sizeof text, "토벌 성공 · 처치 %d · +%d문 · 정혼주 파편 +%d", result.kills, coins, shards);
    else
        snprintf(text, sizeof text, "토벌 실패 · 처치 %d · +%d문", result.kills, coins);
    game_toast(g, text);
    return true;
}
bool raid_cancel(Game *g, const Game *saved) {
    if (g == saved || !g->raid.active || saved->raid.active) return false;
    *g = *saved;
    memset(&g->raid, 0, sizeof g->raid);
    world_select(g->map);
    g->phase = PH_PLAY;
    return true;
}
bool raid_is_elite(const Game *g, const Enemy *e) {
    return g->raid.active && g->raid.elite >= 0 && e == &g->hunt[g->raid.elite];
}
int raid_basic_bonus(const Game *g) { return 2 * stack(g, RAUG_STEEL); }
float raid_range_bonus(const Game *g) { return 6.0f * (float)stack(g, RAUG_STEEL); }
int raid_wind_damage(const Game *g) { return 4 * stack(g, RAUG_WIND); }
int raid_technique_bonus(const Game *g) { return 3 * stack(g, RAUG_FLOW); }
float raid_cooldown(const Game *g, float cooldown) {
    return cooldown * (1 - .15f * (float)stack(g, RAUG_FOCUS));
}
float raid_dodge_recharge(const Game *g, float seconds) {
    return fmaxf(.4f, seconds - .2f * (float)stack(g, RAUG_STEP));
}
