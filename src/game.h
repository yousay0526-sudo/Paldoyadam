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
#define HERO_ATTACK_FRAMES 6
#define HERO_ATTACK_COLUMNS 3
#define HERO_ATTACK_SECONDS .42f
#define HERO_ATTACK_IMPACT .14f
#define HERO_SKILL_SECONDS .64f
#define HERO_SKILL_IMPACT .24f
#define COMBO_SECONDS 1.05f
#define HIT_FLASH_SECONDS .12f
#define HITSTOP_LIGHT_SECONDS .035f
#define HITSTOP_HEAVY_SECONDS .055f
#define HITSTOP_WIND_SECONDS .020f
#define HITSTOP_HURT_SECONDS .045f
/* A boar windup lands within this centre distance; its warning uses the same reach. */
#define BOAR_STRIKE_RANGE 73.0f
/* Windup lengths and the butcher's cleaver arc, shared with the warnings. */
#define BOAR_WINDUP_SECONDS .6f
#define BOSS_WINDUP_SECONDS .95f
#define BOSS_STRIKE_RANGE 109.0f
#define BOSS_STRIKE_HALF 1.15f
#define BOAR_RECOVER_SECONDS 1.15f
#define BOSS_SWING_SECONDS .2f
/* A defeated enemy stays on screen this long while it falls. */
#define ENEMY_DEATH_SECONDS .6f
/* Knockback: initial speeds (world units/s) decaying at KNOCK_DECAY per
 * second, so the total push is speed / KNOCK_DECAY. The butcher is heavy. */
#define KNOCK_DECAY 10.0f
#define KNOCK_BOSS_SCALE .25f
#define KNOCK_HERO_BOAR 120.0f
#define KNOCK_HERO_BOSS 200.0f
/* A technique that lands during a boar's windup breaks it into recovery. */
#define BOAR_STAGGER_SECONDS .6f
/* Bodies: overlapping circles separate with this time constant, split by
 * mass, and at most this many boars wind up at once. */
#define BODY_SEPARATE_SECONDS .06f
#define BODY_MASS_HERO 1.0f
#define BODY_MASS_BOAR 2.0f
#define BODY_MASS_BOSS 6.0f
#define BOAR_ATTACK_TOKENS 2
#define MOVE_NONE -2
#define MOVE_ATTACK -1
/* One full left/right stride in world units, independent of frame rate. */
#define HERO_WALK_DISTANCE 112.0f
/* Movement: top speed, exponential time constants for starting and stopping
 * (about 95% after three of them), and the speed kept while swinging. */
#define HERO_SPEED 155.0f
#define HERO_ACCEL_SECONDS .02f
#define HERO_DECEL_SECONDS .027f
#define HERO_ATTACK_MOVE_SCALE .55f

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
    PH_PAUSED,
    PH_TECHNIQUES
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
typedef enum { TECH_CLEAVE, TECH_SWEEP, TECH_THRUST, TECH_RETREAT, TECH_COUNT } Technique;
typedef enum { SWIPE_UP, SWIPE_RIGHT, SWIPE_DOWN, SWIPE_LEFT, SWIPE_COUNT } Swipe;
typedef enum { ELDER, SMITH, HERBALIST, INNKEEPER, MESSENGER } NpcId;
typedef enum { ITEM_SWORD, ITEM_ARMOR, ITEM_TONIC, ITEM_BANDAGE, ITEM_MEAL, ITEM_COUNT } Item;
typedef enum { AI_IDLE, AI_CHASE, AI_WINDUP, AI_SWING, AI_RECOVER, AI_RETURN, AI_DEAD } AiState;
typedef enum { NEAR_NONE, NEAR_NPC, NEAR_TRACE, NEAR_DROP } NearKind;
/* FX_DUST: amount 1 footstep, 2 dash, 3 fall; angle is the travel heading. */
typedef enum { FX_NUMBER, FX_HIT, FX_WIND, FX_SKILL, FX_BOSS_SWING, FX_DUST } EffectKind;
typedef enum { NOTICE_NONE = 0, NOTICE_BOSS = 1, NOTICE_DEATH = 2, NOTICE_VICTORY = 4 } Notice;
typedef struct {
    Vec2 pos, dir;
    int hp, max_hp, face;
    float bleed, bleed_tick, invul, hurt_time;
    float walk_cycle;
    bool walking;
    /* Knockback velocity from the last hit taken; decays every tick. */
    Vec2 knock;
    /* Smoothed walking velocity (world units/s), wall-clamped each tick. */
    Vec2 vel;
    /* The swing keeps its own target/facing while movement remains responsive. */
    float attack_elapsed, attack_angle;
    Vec2 impact_origin, impact_start;
    int attack_target, attack_face;
    bool attacking, attack_hit;
    float skill_elapsed, skill_angle;
    Technique technique;
    bool skilling, skill_hit;
} Hero;
typedef struct {
    Vec2 pos, home;
    int hp, max_hp;
    AiState state;
    float timer, angle, hit_time;
    Route route;
    float repath;
    /* Presentation only: distance walked (drives the gait) and fall time. */
    float stride, dying;
    /* Authored view: last self-propelled heading, and whether this tick walked.
     * These never steer AI, knockback, body separation or hit tests. */
    Vec2 facing;
    bool walking;
    /* Knockback velocity, decaying; it moves pos through terrain collision. */
    Vec2 knock;
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
    const char *name, *description;
    float cooldown, range, step;
    int damage;
} TechniqueInfo;
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
    bool owned_sword, owned_armor;
    int coins, tonics, bandages, herbs, shop;
    Augment augment;
    float cooldown, technique_cooldowns[TECH_COUNT], item_cooldown, attack_cooldown, time, toast_time;
    Technique swipe_bindings[SWIPE_COUNT];
    /* Input links never award damage. Confirmed hits advance once per impact. */
    int input_count, last_move, combo_count, last_hit_move, queued_move;
    float input_time, combo_time, buffer_time;
    enum { IMPACT_NONE, IMPACT_HIT, IMPACT_MISS } last_impact;
    enum { TRAIN_READY, TRAIN_STARTED, TRAIN_QUEUED, TRAIN_COOLDOWN,
           TRAIN_INVALID, TRAIN_BLOCKED } combat_request;
    int input_move;
    float request_time;
    /* Wall time counts down the brief stop; all gameplay clocks share its
     * remaining simulation time. Input requests are still accepted. */
    float hitstop_time;
    unsigned notices;
    char toast[384];
    Effect effects[EFFECT_MAX];
    int effect_count;
    struct {
        bool active;
        unsigned hits, misses;
        int last_damage, impact_damage;
        float hit_time;
    } training;
} Game;

extern const Vec2 HOME, BOSS_HOME;
extern const Npc NPCS[NPC_COUNT];
extern const Trace TRACES[TRACE_COUNT];
extern const QuestInfo QUESTS[QUEST_COUNT];
extern const ItemInfo ITEMS[ITEM_COUNT];
extern const AugmentInfo AUGMENTS[3];
extern const TechniqueInfo TECHNIQUES[TECH_COUNT];

float distance(Vec2 a, Vec2 b);
float clampf(float value, float min, float max);
void game_reset(Game *g);
void game_start(Game *g);
bool game_enter_training(Game *game, Game *saved);
bool game_leave_training(Game *game, const Game *saved);
void game_tick(Game *g, float dt, Vec2 movement);
void game_clear_hit_feedback(Game *g);
/* -1 means the approved standing pose; 0..7 select walking atlas cells. */
int game_hero_frame(const Game *g);
int game_hero_attack_frame(const Game *g);
int game_hero_skill_frame(const Game *g);
bool game_attack(Game *g);
/* Shared geometry for simulation and the existing procedural sword trail.
 * Radius is exclusive, angle inclusive; targets use their existing centers. */
float game_attack_range(const Game *g, int move);
float game_attack_half_angle(int move);
bool game_attack_contains(Vec2 origin, float angle, float range, int move, Vec2 target);
bool game_attack_path_contains(Vec2 start, Vec2 end, float angle, float range, int move, Vec2 target);
void game_invalid_direction(Game *g);
bool game_technique(Game *g, Technique technique);
float game_technique_cooldown(const Game *g, Technique technique);
bool game_bind_swipe(Game *g, Swipe direction, Technique technique);
bool game_equip(Game *g, Item item, bool equipped);
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
/* Input strength (0..1) along direction that lets the eased hero stop
 * `distance` ahead after one dt step, given its current velocity. */
float game_arrival_strength(const Game *g, Vec2 direction, float distance, float dt);
bool game_damage(Game *g, Enemy *enemy, int amount, EffectKind kind);
void game_recover(Game *g);
void game_retry(Game *g);
bool game_choose(Game *g, Augment augment);
bool game_boars_active(const Game *g);
#endif
