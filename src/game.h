#ifndef PALDO_GAME_H
#define PALDO_GAME_H
#include <stdbool.h>
#include <stddef.h>

#define WORLD_W 2304.0f
#define WORLD_H 1536.0f
#define BOAR_COUNT 3
#define NPC_COUNT 5
#define TRACE_COUNT 2
#define EFFECT_MAX 48
#define TOAST_SECONDS 2.8f
#define PICKUP_RADIUS 70.0f
#define MAGNET_RADIUS 156.0f
#define COLLECT_RADIUS 16.0f
#define DROP_REST_SECONDS .70f
#define HERO_WALK_FRAMES 8
#define HERO_WALK_COLUMNS 4
/* One full left/right stride in world units, independent of frame rate. */
#define HERO_WALK_DISTANCE 112.0f

typedef struct {
    float x, y;
} Vec2;
#define ROUTE_MAX 64
typedef struct {
    Vec2 points[ROUTE_MAX];
    int count, cursor;
} Route;
typedef enum {
    PH_TITLE,
    PH_PROLOGUE,
    PH_PLAY,
    PH_DIALOGUE,
    PH_SHOP,
    PH_BAG,
    PH_JOURNAL,
    PH_AUGMENT,
    PH_COMPLETE,
    PH_DEAD,
    PH_PAUSED
} Phase;
typedef enum {
    Q_MEET,
    Q_TRACES,
    Q_RECOVER,
    Q_DELIVER,
    Q_REPORT,
    Q_HUNT,
    Q_COMBAT,
    Q_RETURN,
    Q_MESSENGER,
    QUEST_COUNT
} Quest;
typedef enum { AUG_NONE, AUG_STEEL, AUG_WIND, AUG_FLOW } Augment;
typedef enum { ELDER, SMITH, HERBALIST, INNKEEPER, MESSENGER } NpcId;
typedef enum { ITEM_SWORD, ITEM_ARMOR, ITEM_TONIC, ITEM_BANDAGE, ITEM_MEAL, ITEM_COUNT } Item;
typedef enum { AI_IDLE, AI_CHASE, AI_WINDUP, AI_SWING, AI_RECOVER, AI_RETURN, AI_DEAD } AiState;
typedef enum { NEAR_NONE, NEAR_NPC, NEAR_TRACE, NEAR_DROP } NearKind;
typedef enum { FX_NUMBER, FX_HIT, FX_WIND, FX_SKILL, FX_BOSS_SWING } EffectKind;
typedef enum { NOTICE_NONE = 0, NOTICE_BOSS = 1, NOTICE_DEATH = 2, NOTICE_VICTORY = 4 } Notice;
typedef struct {
    Vec2 pos, dir;
    int hp, max_hp, face;
    float bleed, bleed_tick, invul;
    float walk_cycle;
    bool walking;
} Hero;
typedef struct {
    Vec2 pos, home;
    int hp, max_hp;
    AiState state;
    float timer, angle;
    Route route;
    float repath;
} Enemy;
typedef struct {
    Vec2 pos;
    bool exists, collected, attracting;
    float age, magnet_time;
} Drop;
typedef struct {
    Vec2 pos;
    const char *name, *role;
} Npc;
typedef struct {
    Vec2 pos;
    const char *name, *text;
} Trace;
typedef struct {
    NearKind kind;
    int index;
} Nearby;
typedef struct {
    const char *title, *detail;
} QuestInfo;
typedef struct {
    const char *name, *description;
    int price;
    NpcId shop;
} ItemInfo;
typedef struct {
    const char *name, *kind, *description;
} AugmentInfo;
typedef struct {
    EffectKind kind;
    Vec2 pos, target;
    float life, angle;
    int amount;
    bool damage;
} Effect;
typedef struct {
    Phase phase;
    Quest quest;
    Hero hero;
    Enemy boss, boars[BOAR_COUNT];
    Drop drops[BOAR_COUNT];
    bool traces[TRACE_COUNT], sword, armor, supplies_delivered, boss_rewarded, replay;
    int coins, tonics, bandages, herbs, shop;
    Augment augment;
    float cooldown, item_cooldown, auto_timer, time, toast_time;
    unsigned notices;
    char toast[384];
    Effect effects[EFFECT_MAX];
    int effect_count;
} Game;

extern const Vec2 HOME, BOSS_HOME;
extern const Npc NPCS[NPC_COUNT];
extern const Trace TRACES[TRACE_COUNT];
extern const QuestInfo QUESTS[QUEST_COUNT];
extern const ItemInfo ITEMS[ITEM_COUNT];
extern const AugmentInfo AUGMENTS[3];

float distance(Vec2 a, Vec2 b);
float clampf(float value, float min, float max);
void game_reset(Game *g);
void game_start(Game *g);
void game_tick(Game *g, float dt, Vec2 movement);
/* -1 means the approved standing pose; 0..7 select walking atlas cells. */
int game_hero_frame(const Game *g);
void game_toast(Game *g, const char *text);
Nearby game_nearby(const Game *g);
int game_pickup_count(const Game *g);
int game_pickup_nearby(Game *g);
Vec2 game_objective(const Game *g);
void game_quest_text(const Game *g, char *out, size_t size);
void game_objective_brief(const Game *g, char *out, size_t size);
bool game_accept(Game *g);
bool game_inspect(Game *g, int index);
bool game_collect(Game *g, int index);
bool game_deliver(Game *g);
bool game_report(Game *g);
bool game_begin_boss(Game *g);
bool game_open_shop(Game *g, int npc);
void game_close_panel(Game *g);
const char *game_purchase_error(const Game *g, Item item);
bool game_buy(Game *g, Item item);
bool game_can_use(const Game *g, Item item);
bool game_use(Game *g, Item item);
bool game_skill(Game *g);
void game_hurt(Game *g, int amount, bool bleed);
bool game_damage(Game *g, Enemy *enemy, int amount, EffectKind kind);
void game_recover(Game *g);
void game_retry(Game *g);
bool game_choose(Game *g, Augment augment);
bool game_boars_active(const Game *g);
#endif
