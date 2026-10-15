#include "game.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define P(x, y) {(x) * WORLD_W, (y) * WORLD_H}
const Vec2 HOME = P(.30f, .51f), BOSS_HOME = P(.81f, .46f);
const Npc NPCS[NPC_COUNT] = {{P(.29f, .43f), "마을 이장", "이야기"},
                             {P(.21f, .285f), "대장장이", "대장간"},
                             {P(.355f, .29f), "약방 주인", "약방"},
                             {{400 * 1.5f, 616 * 1.5f}, "주모", "주막"},
                             {{667 * 1.5f, 547 * 1.5f}, "윤겸", "한양의 전령"}};
const Trace TRACES[TRACE_COUNT] = {
    {P(.51f, .55f), "찢어진 옷자락",
     "뒤집힌 수레에 백정의 옷자락이 걸려 있다.\n곁에는 비어 있는 약재 바구니가 놓여 있다."},
    {{1070 * 1.5f, 612 * 1.5f},
     "깊게 패인 칼자국",
     "나무에 커다란 식칼 자국이 남아 있다.\n짐승 발자국과 흩어진 약재가 남동쪽 빈터로 이어진다."}};
const QuestInfo QUESTS[QUEST_COUNT] = {{"사라진 사람들", "마을 이장에게 실종 사건에 관해 듣기"},
                                       {"숲길의 흔적", "동쪽 수레와 숲길의 흔적 두 곳 조사"},
                                       {"빼앗긴 약재", "변이한 멧돼지 3마리 제압 · 약재 꾸러미 회수"},
                                       {"돌려받은 약재", "마을 약방 주인에게 약재 꾸러미 전달"},
                                       {"백정의 행방", "마을 이장에게 조사 결과 보고"},
                                       {"숲속의 울음", "동쪽 빈터에서 변이한 백정 찾기"},
                                       {"돌아오지 못한 사람", "붉은 식칼 공격 범위를 피해 백정 제압"},
                                       {"마을의 불빛", "이장에게 소탕 소식을 전하고 깨달음 선택"},
                                       {"한양에서 온 서신", "전령 윤겸과 대화"}};
const ItemInfo ITEMS[ITEM_COUNT] = {{"단련한 환도", "기본 공격과 갈라베기 피해 +5 · 즉시 장착", 60, SMITH},
                                    {"누비 갑옷", "직접 받는 피해 -3 · 즉시 장착", 45, SMITH},
                                    {"탕약", "체력 45 회복 · 가방 또는 하단 버튼으로 사용", 12, HERBALIST},
                                    {"지혈산", "출혈 즉시 해제 · 가방 또는 하단 버튼으로 사용", 8, HERBALIST},
                                    {"국밥과 휴식", "체력 모두 회복 · 출혈 해제", 10, INNKEEPER}};
const AugmentInfo AUGMENTS[3] = {
    {"강철 대검", "무기", "베는 범위가 넓어지고 한 번의 베기가 무거워집니다.\n공격 범위 +18 · 기본 피해 +4"},
    {"바람의 깃", "패시브", "검 끝에서 바람 칼날이 뻗어 나갑니다.\n거리 120 안의 적에게 바람 피해 6"},
    {"흐르는 발", "액티브", "갈라베기와 함께 앞으로 파고듭니다.\n스킬 재사용 4초 · 스킬 피해 +6"}};
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
    e->pos = world_move(e->pos, (Vec2){direction.x * step, direction.y * step}, radius);
}
static bool near_npc(const Game *g, int n) {
    return n >= 0 && n < NPC_COUNT && distance(g->hero.pos, NPCS[n].pos) < 70 &&
           world_segment(g->hero.pos, NPCS[n].pos, 0);
}
static Enemy make_enemy(Vec2 p, int hp) {
    Enemy e = {0};
    e.pos = e.home = p;
    e.hp = e.max_hp = hp;
    e.timer = .5f;
    return e;
}
static void effect(Game *g, Effect e) {
    if (g->effect_count < EFFECT_MAX)
        g->effects[g->effect_count++] = e;
}
static void dead(Game *g) {
    if (g->hero.hp <= 0 && g->phase != PH_DEAD) {
        g->phase = PH_DEAD;
        g->notices |= NOTICE_DEATH;
    }
}
void game_toast(Game *g, const char *s) {
    snprintf(g->toast, sizeof g->toast, "%s", s);
    g->toast_time = TOAST_SECONDS;
}
void game_reset(Game *g) {
    memset(g, 0, sizeof *g);
    g->phase = PH_TITLE;
    g->quest = Q_MEET;
    g->hero = (Hero){.pos = HOME, .dir = {1, 0}, .hp = 100, .max_hp = 100, .face = 1};
    g->boss = make_enemy(BOSS_HOME, 240);
    const Vec2 positions[3] = {P(.62f, .69f), P(.66f, .74f), P(.70f, .66f)};
    for (int i = 0; i < BOAR_COUNT; i++)
        g->boars[i] = make_enemy(positions[i], 38);
    g->coins = 55;
    g->tonics = 2;
    g->bandages = 1;
    g->shop = -1;
    g->auto_timer = .2f;
}
void game_start(Game *g) {
    g->phase = PH_PLAY;
    game_toast(g, "여정 시작 · 마을 이장과 대화");
}
bool game_boars_active(const Game *g) {
    return g->traces[0] && g->traces[1];
}
Nearby game_nearby(const Game *g) {
    Nearby result = {NEAR_NONE, -1};
    float best = 66;
    if (g->phase != PH_PLAY || g->quest == Q_COMBAT)
        return result;
    for (int i = 0; i < (g->quest == Q_MESSENGER ? NPC_COUNT : NPC_COUNT - 1); i++) {
        float d = distance(g->hero.pos, NPCS[i].pos);
        if (d < best && world_segment(g->hero.pos, NPCS[i].pos, 0)) {
            best = d;
            result = (Nearby){NEAR_NPC, i};
        }
    }
    if (g->quest == Q_TRACES)
        for (int i = 0; i < TRACE_COUNT; i++) {
            float d = distance(g->hero.pos, TRACES[i].pos);
            if (!g->traces[i] && d < best && world_segment(g->hero.pos, TRACES[i].pos, 0)) {
                best = d;
                result = (Nearby){NEAR_TRACE, i};
            }
        }
    return result;
}
/* The optional keyboard pickup never competes with NPC interaction. */
static bool can_pickup(const Game *g, int i) {
    return i >= 0 && i < BOAR_COUNT && g->phase == PH_PLAY && g->quest == Q_RECOVER &&
           g->drops[i].exists && !g->drops[i].collected &&
           distance(g->hero.pos, g->drops[i].pos) < PICKUP_RADIUS &&
           world_segment(g->hero.pos, g->drops[i].pos, 0);
}
int game_pickup_count(const Game *g) {
    int count = 0;
    for (int i = 0; i < BOAR_COUNT; i++)
        if (can_pickup(g, i))
            count++;
    return count;
}
int game_pickup_nearby(Game *g) {
    int count = 0;
    for (int i = 0; i < BOAR_COUNT; i++)
        if (game_collect(g, i))
            count++;
    if (count) {
        char text[128];
        snprintf(text, sizeof text, "약재 +%d · 회수 %d/3", count, g->herbs);
        game_toast(g, text);
    }
    return count;
}
Vec2 game_objective(const Game *g) {
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
    if (g->quest == Q_TRACES)
        snprintf(out, size, "%s (%d/2)", QUESTS[g->quest].detail, g->traces[0] + g->traces[1]);
    else if (g->quest == Q_RECOVER)
        snprintf(out, size, "%s (%d/3)", QUESTS[g->quest].detail, g->herbs);
    else
        snprintf(out, size, "%s", QUESTS[g->quest].detail);
}
void game_objective_brief(const Game *g, char *out, size_t size) {
    static const char *brief[QUEST_COUNT] = {
        "마을 이장과 대화", "동쪽 숲길의 흔적 조사", "멧돼지 제압 · 약재 회수",
        "약방에 약재 전달", "이장에게 조사 결과 보고", "동쪽 도축터로 이동",
        "공격을 피하며 백정 제압", "이장에게 소식 전하기", "전령 윤겸과 대화"};
    if (g->quest == Q_TRACES)
        snprintf(out, size, "%s  %d/2", brief[g->quest], g->traces[0] + g->traces[1]);
    else if (g->quest == Q_RECOVER)
        snprintf(out, size, "%s  %d/3", brief[g->quest], g->herbs);
    else
        snprintf(out, size, "%s", brief[g->quest]);
}
bool game_accept(Game *g) {
    if (g->phase != PH_PLAY || g->quest != Q_MEET || !near_npc(g, ELDER))
        return false;
    g->quest = Q_TRACES;
    game_toast(g, "의뢰 수락 · 숲길의 흔적");
    return true;
}
bool game_inspect(Game *g, int i) {
    if (i < 0 || i >= TRACE_COUNT || g->phase != PH_PLAY || g->quest != Q_TRACES || g->traces[i] ||
        distance(g->hero.pos, TRACES[i].pos) >= 70 || !world_segment(g->hero.pos, TRACES[i].pos, 0))
        return false;
    g->traces[i] = true;
    if (game_boars_active(g))
        g->quest = Q_RECOVER;
    return true;
}
bool game_collect(Game *g, int i) {
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
    if (g->phase != PH_PLAY || g->quest != Q_DELIVER || g->herbs < 3 || g->supplies_delivered ||
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
    if (g->phase != PH_PLAY || g->quest != Q_REPORT || !g->supplies_delivered || !near_npc(g, ELDER))
        return false;
    g->quest = Q_HUNT;
    game_toast(g, "목표 갱신 · 동쪽 도축터");
    return true;
}
bool game_open_shop(Game *g, int n) {
    if (g->phase != PH_PLAY || g->quest == Q_COMBAT || n < SMITH || n > INNKEEPER || !near_npc(g, n))
        return false;
    g->shop = n;
    g->phase = PH_SHOP;
    return true;
}
void game_close_panel(Game *g) {
    if (g->phase == PH_SHOP || g->phase == PH_BAG || g->phase == PH_JOURNAL || g->phase == PH_PAUSED) {
        g->shop = -1;
        g->phase = PH_PLAY;
    }
}
const char *game_purchase_error(const Game *g, Item item) {
    if (item < 0 || item >= ITEM_COUNT || g->phase != PH_SHOP || g->shop != (int)ITEMS[item].shop ||
        !near_npc(g, g->shop))
        return "상점에서 구입하세요";
    if ((item == ITEM_SWORD && g->sword) || (item == ITEM_ARMOR && g->armor))
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
    if (game_purchase_error(g, item))
        return false;
    g->coins -= ITEMS[item].price;
    switch (item) {
    case ITEM_SWORD:
        g->sword = true;
        break;
    case ITEM_ARMOR:
        g->armor = true;
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
    if ((g->phase != PH_PLAY && g->phase != PH_BAG) || (g->phase == PH_PLAY && g->item_cooldown > 0))
        return false;
    if (item == ITEM_TONIC)
        return g->tonics > 0 && g->hero.hp < g->hero.max_hp;
    return item == ITEM_BANDAGE && g->bandages > 0 && g->hero.bleed > 0;
}
bool game_use(Game *g, Item item) {
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
    if (g->quest == Q_COMBAT)
        return e == &g->boss;
    for (int i = 0; i < BOAR_COUNT; i++)
        if (e == &g->boars[i])
            return game_boars_active(g);
    return false;
}
bool game_damage(Game *g, Enemy *e, int amount, EffectKind kind) {
    if (g->phase != PH_PLAY || e->hp <= 0 || amount <= 0 || !active_enemy(g, e))
        return false;
    e->hp = (int)fmaxf(0, (float)e->hp - (float)amount);
    effect(g, (Effect){.kind = FX_NUMBER, .pos = add(e->pos, (Vec2){0, -48}), .life = .7f, .amount = amount});
    effect(g, (Effect){.kind = kind, .pos = g->hero.pos, .target = e->pos, .life = .25f});
    if (e->hp == 0) {
        e->state = AI_DEAD;
        if (e == &g->boss) {
            g->hero.bleed = 0;
            g->quest = Q_RETURN;
            if (!g->boss_rewarded && !g->replay) {
                g->coins += 70;
                g->boss_rewarded = true;
            }
            g->notices |= NOTICE_VICTORY;
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
void game_hurt(Game *g, int amount, bool bleed) {
    if (g->phase != PH_PLAY || g->hero.invul > 0)
        return;
    amount = (int)fmaxf(1, (float)amount - (g->armor ? 3.0f : 0));
    g->hero.hp = (int)fmaxf(0, (float)g->hero.hp - amount);
    g->hero.invul = .75f;
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
    dead(g);
}
static Enemy *nearest_enemy(Game *g) {
    Enemy *e = NULL;
    float best = 1e9f;
    if (g->quest == Q_COMBAT && g->boss.hp > 0 && world_segment(g->hero.pos, g->boss.pos, 0))
        return &g->boss;
    if (game_boars_active(g))
        for (int i = 0; i < BOAR_COUNT; i++) {
            float d = distance(g->hero.pos, g->boars[i].pos);
            if (g->boars[i].hp > 0 && d < best && world_segment(g->hero.pos, g->boars[i].pos, 0)) {
                best = d;
                e = &g->boars[i];
            }
        }
    return e;
}
bool game_skill(Game *g) {
    if (g->phase != PH_PLAY || g->cooldown > 0)
        return false;
    g->cooldown = g->augment == AUG_FLOW ? 4 : 5.5f;
    if (g->augment == AUG_FLOW) {
        Enemy *e = nearest_enemy(g);
        float a = e && distance(e->pos, g->hero.pos) < 180
                      ? atan2f(e->pos.y - g->hero.pos.y, e->pos.x - g->hero.pos.x)
                      : atan2f(g->hero.dir.y, g->hero.dir.x);
        g->hero.pos = world_move(g->hero.pos, (Vec2){cosf(a) * 45, sinf(a) * 45}, HERO_RADIUS);
        g->hero.invul = .45f;
    }
    effect(g, (Effect){.kind = FX_SKILL, .pos = g->hero.pos, .life = .4f});
    int damage = (g->augment == AUG_FLOW ? 32 : 26) + (g->sword ? 5 : 0);
    if (g->quest == Q_COMBAT) {
        if (distance(g->hero.pos, g->boss.pos) < 105 && world_segment(g->hero.pos, g->boss.pos, 0))
            game_damage(g, &g->boss, damage, FX_HIT);
    } else if (game_boars_active(g))
        for (int i = 0; i < BOAR_COUNT; i++)
            if (distance(g->hero.pos, g->boars[i].pos) < 105 &&
                world_segment(g->hero.pos, g->boars[i].pos, 0))
                game_damage(g, &g->boars[i], damage, FX_HIT);
    return true;
}
bool game_begin_boss(Game *g) {
    if (g->quest != Q_HUNT)
        return false;
    g->quest = Q_COMBAT;
    g->phase = PH_PLAY;
    g->boss.state = AI_CHASE;
    g->boss.timer = 1.2f;
    game_toast(g, "붉은 범위 밖으로 · 공격 뒤가 기회");
    return true;
}
void game_recover(Game *g) {
    g->hero.pos = HOME;
    g->hero.hp = g->hero.max_hp;
    g->hero.bleed = g->hero.invul = 0;
    if (g->quest == Q_COMBAT) {
        g->boss = make_enemy(BOSS_HOME, 240);
        g->quest = Q_HUNT;
    }
    for (int i = 0; i < BOAR_COUNT; i++)
        if (g->boars[i].hp > 0)
            g->boars[i] = make_enemy(g->boars[i].home, 38);
    g->phase = PH_PLAY;
    g->shop = -1;
    g->cooldown = g->item_cooldown = 0;
    g->effect_count = 0;
    g->notices = 0;
    game_toast(g, "마을에서 회복 · 여정 진행 유지");
}
void game_retry(Game *g) {
    g->hero.pos = add(BOSS_HOME, (Vec2){-145, 0});
    g->hero.hp = 100;
    g->hero.bleed = g->hero.invul = 0;
    g->boss = make_enemy(BOSS_HOME, 240);
    g->cooldown = g->item_cooldown = 0;
    g->auto_timer = .4f;
    g->effect_count = 0;
    g->notices = 0;
    g->quest = Q_HUNT;
    game_begin_boss(g);
}
bool game_choose(Game *g, Augment a) {
    if (g->phase != PH_AUGMENT || a < AUG_STEEL || a > AUG_FLOW)
        return false;
    g->augment = a;
    g->quest = Q_MESSENGER;
    g->phase = PH_PLAY;
    game_toast(g, "한양의 전령 도착 · 윤겸과 대화");
    return true;
}
static void tick_boar(Game *g, Enemy *e, float dt) {
    float d = distance(e->pos, g->hero.pos);
    e->timer -= dt;
    switch (e->state) {
    case AI_IDLE:
        if (d < 180)
            e->state = AI_CHASE;
        break;
    case AI_CHASE:
        if (d > 270) {
            e->state = AI_RETURN;
            break;
        }
        if (d > 43)
            move_toward(e, g->hero.pos, 64 * dt, BOAR_RADIUS, dt);
        if (d < 65 && e->timer <= 0 && world_segment(e->pos, g->hero.pos, 0)) {
            e->state = AI_WINDUP;
            e->angle = atan2f(g->hero.pos.y - e->pos.y, g->hero.pos.x - e->pos.x);
            e->timer = .6f;
        }
        break;
    case AI_WINDUP:
        if (e->timer <= 0) {
            if (d < 73 && world_segment(e->pos, g->hero.pos, 0))
                game_hurt(g, 9, false);
            e->state = AI_RECOVER;
            e->timer = 1.15f;
        }
        break;
    case AI_RECOVER:
        if (e->timer <= 0) {
            e->state = AI_CHASE;
            e->timer = .4f;
        }
        break;
    case AI_RETURN:
        if (distance(e->pos, e->home) < 4)
            e->state = AI_IDLE;
        else
            move_toward(e, e->home, 64 * dt, BOAR_RADIUS, dt);
        break;
    default:
        break;
    }
}
static void tick_boss(Game *g, float dt) {
    Enemy *e = &g->boss;
    float d = distance(g->hero.pos, e->pos);
    e->timer -= dt;
    switch (e->state) {
    case AI_CHASE:
        if (d > 57)
            move_toward(e, g->hero.pos, 45 * dt, BOSS_RADIUS, dt);
        if (e->timer <= 0 && d < 138 && world_segment(e->pos, g->hero.pos, 0)) {
            e->state = AI_WINDUP;
            e->timer = .95f;
            e->angle = atan2f(g->hero.pos.y - e->pos.y, g->hero.pos.x - e->pos.x);
        }
        break;
    case AI_WINDUP:
        if (e->timer <= 0) {
            e->state = AI_SWING;
            e->timer = .2f;
            effect(g, (Effect){.kind = FX_BOSS_SWING, .pos = e->pos, .angle = e->angle, .life = .25f});
            float a = atan2f(g->hero.pos.y - e->pos.y, g->hero.pos.x - e->pos.x) - e->angle;
            if (d < 109 && fabsf(atan2f(sinf(a), cosf(a))) < 1.15f && world_segment(e->pos, g->hero.pos, 0))
                game_hurt(g, 18, true);
        }
        break;
    case AI_SWING:
        if (e->timer <= 0) {
            e->state = AI_RECOVER;
            e->timer = 1.25f;
        }
        break;
    case AI_RECOVER:
        if (e->timer <= 0) {
            e->state = AI_CHASE;
            e->timer = .9f;
        }
        break;
    default:
        break;
    }
}
/* Follow only an unobstructed segment. Menus pause this with the rest of the world. */
static void tick_loot(Game *g, float dt) {
    if (g->phase != PH_PLAY || g->quest != Q_RECOVER || dt <= 0)
        return;
    for (int i = 0; i < BOAR_COUNT; i++) {
        Drop *drop = &g->drops[i];
        drop->attracting = false;
        if (!drop->exists || drop->collected)
            continue;
        float d = distance(g->hero.pos, drop->pos);
        if (d > MAGNET_RADIUS || !world_segment(drop->pos, g->hero.pos, 0))
            continue;
        drop->attracting = true;
        if (d > COLLECT_RADIUS) {
            float step = fminf(d, (180 + (MAGNET_RADIUS - d) * 2.4f) * dt);
            drop->pos = world_move(drop->pos,
                (Vec2){(g->hero.pos.x - drop->pos.x) * step / d,
                       (g->hero.pos.y - drop->pos.y) * step / d}, 0);
        }
        if (distance(g->hero.pos, drop->pos) <= COLLECT_RADIUS)
            game_collect(g, i);
    }
}
void game_tick(Game *g, float dt, Vec2 input) {
    if (!isfinite(dt) || !isfinite(input.x) || !isfinite(input.y))
        return;
    dt = clampf(dt, 0, .05f);
    if (g->phase != PH_PLAY)
        return;
    g->time += dt;
    g->toast_time = fmaxf(0, g->toast_time - dt);
    float length = hypotf(input.x, input.y);
    if (length > .05f) {
        float strength = fminf(1, length);
        input.x /= length;
        input.y /= length;
        g->hero.pos = world_move(
            g->hero.pos, (Vec2){input.x * 155 * strength * dt, input.y * 155 * strength * dt}, HERO_RADIUS);
        g->hero.dir = input;
        if (fabsf(input.x) > .1f)
            g->hero.face = input.x > 0 ? 1 : -1;
    }
    g->cooldown = fmaxf(0, g->cooldown - dt);
    g->item_cooldown = fmaxf(0, g->item_cooldown - dt);
    g->hero.invul = fmaxf(0, g->hero.invul - dt);
    for (int i = 0; i < g->effect_count;) {
        g->effects[i].life -= dt;
        if (g->effects[i].life <= 0)
            g->effects[i] = g->effects[--g->effect_count];
        else
            i++;
    }
    if (g->quest == Q_HUNT && distance(g->hero.pos, g->boss.pos) < 165 &&
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
    g->auto_timer -= dt;
    Enemy *e = nearest_enemy(g);
    float range = g->augment == AUG_STEEL ? 76 : 58;
    if (e && g->auto_timer <= 0 && distance(g->hero.pos, e->pos) < (g->augment == AUG_WIND ? 120 : range)) {
        g->auto_timer = .75f;
        if (distance(g->hero.pos, e->pos) < range)
            game_damage(g, e, (g->augment == AUG_STEEL ? 13 : 9) + (g->sword ? 5 : 0), FX_HIT);
        if (g->augment == AUG_WIND)
            game_damage(g, e, 6, FX_WIND);
    }
    if (g->quest == Q_COMBAT)
        tick_boss(g, dt);
    else if (game_boars_active(g))
        for (int i = 0; i < BOAR_COUNT && g->phase == PH_PLAY; i++)
            if (g->boars[i].hp > 0)
                tick_boar(g, &g->boars[i], dt);
    tick_loot(g, dt);
}
