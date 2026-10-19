#include "draw.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
EM_JS(void, web_ready, (void), { if (Module.paldoReady) Module.paldoReady(); });
EM_JS(void, web_fullscreen, (void), { if (Module.paldoToggleFullscreen) Module.paldoToggleFullscreen(); });
#endif

#define BUTTON_MAX 64
#define PI 3.14159265358979323846f

typedef enum {
    ACT_START,
    ACT_SKIP,
    ACT_NEXT,
    ACT_INTERACT,
    ACT_SKILL,
    ACT_PAUSE,
    ACT_FULLSCREEN,
    ACT_BAG,
    ACT_JOURNAL,
    ACT_CLOSE,
    ACT_TITLE,
    ACT_RECOVER,
    ACT_REPLAY,
    ACT_BUY,
    ACT_USE,
    ACT_CHOOSE,
    ACT_PICKUP, ACT_ATTACK, ACT_TECHNIQUE, ACT_TECHNIQUES, ACT_SWIPE_SELECT,
    ACT_TECH_BIND, ACT_BIND_RESET, ACT_BAG_TAB, ACT_BAG_SELECT, ACT_EQUIP, ACT_GEAR_SELECT,
    ACT_TRAINING_EXIT
} Action;
typedef enum {
    DONE_NONE,
    DONE_ACCEPT,
    DONE_DELIVER,
    DONE_REPORT,
    DONE_AUGMENT,
    DONE_BOSS,
    DONE_FINISH
} DialogueDone;
typedef struct {
    const char *speaker, *text;
} Line;
typedef struct {
    SDL_FRect rect;
    Action action;
    int value;
    bool enabled;
} Button;
typedef struct {
    Game game;
    Game training_return;
    Gfx gfx;
    SDL_Window *window;
    SDL_Renderer *renderer;
    float width, height, density;
    Vec2 camera;
    Button buttons[BUTTON_MAX];
    int button_count;
    Line dialogue[5];
    int dialogue_count, dialogue_index, story_index;
    DialogueDone dialogue_done;
    bool touch_input, running, target_set, pointer_down, pointer_moved, headless, debug_collision;
    Sint64 pointer_id;
    Vec2 pointer_start, pointer_delta, target;
    float journal_scroll, story_elapsed;
    int bag_tab, bag_selected, selected_swipe;
    bool attack_pointer_down, attack_swiping, attack_repeated, attack_key_down;
    Sint64 attack_pointer_id;
    Vec2 attack_start, attack_anchor;
    int attack_direction;
    float attack_hold;
    bool secret_down, secret_moved, movement_blocked;
    Sint64 secret_id;
    Vec2 secret_start;
    Uint64 secret_started, secret_last;
    int secret_taps;
    Route route;
    Uint64 previous_frame;
    double timer_frequency;
} App;
static int combat_direction(Vec2 delta);
static void combat_tick(App *a, float dt);
#if defined(__EMSCRIPTEN__) && defined(PALDO_WEB_TESTING)
static App *web_test_app;
EMSCRIPTEN_KEEPALIVE const char *paldo_test_state(void) {
    static char state[1024];
    App *a = web_test_app;
    if (!a)
        return "{}";
    snprintf(state, sizeof state,
             "{\"phase\":%d,\"quest\":%d,\"story\":%d,\"x\":%.3f,\"y\":%.3f,"
             "\"width\":%.3f,\"height\":%.3f,\"density\":%.3f,\"clear\":%s,"
             "\"dialogue\":%d,\"pickups\":%d,\"herbs\":%d,\"cooldown\":%.3f,"
             "\"heroFrame\":%d,\"walkCycle\":%.6f,\"face\":%d,"
             "\"training\":%s,\"hits\":%u,\"misses\":%u,\"damage\":%d,\"combo\":%d,"
             "\"queued\":%d,\"hp\":%d,\"coins\":%d,\"dummyHp\":%d,\"attackDown\":%s,"
             "\"secretTaps\":%d,\"pointerX\":%.2f,\"pointerY\":%.2f,\"secretDown\":%s}",
             a->game.phase, a->game.quest, a->story_index, a->game.hero.pos.x, a->game.hero.pos.y,
             a->width, a->height, a->density,
             world_clear(a->game.hero.pos, HERO_RADIUS) ? "true" : "false",
             a->dialogue_index, game_pickup_count(&a->game), a->game.herbs, a->game.cooldown,
             game_hero_frame(&a->game), a->game.hero.walk_cycle, a->game.hero.face,
             a->game.training.active ? "true" : "false", a->game.training.hits, a->game.training.misses,
             a->game.training.last_damage, a->game.combo_count, a->game.queued_move, a->game.hero.hp,
             a->game.coins, a->game.boss.hp, a->attack_pointer_down ? "true" : "false",
             a->secret_taps, a->pointer_start.x, a->pointer_start.y, a->secret_down ? "true" : "false");
    return state;
}
#endif

static const char *STORY_CAPS[] = {"귀향길", "깊은 산속", "붉은 열매", "물에 비친 얼굴",
                                   "10년 뒤 · 경상도 남부"};
static const char *STORY_TEXT[] = {
    "전쟁이 끝났다.\n장군과 무사들은 집으로 돌아가기 위해 산길을 올랐다.",
    "갑자기 견딜 수 없는 허기와 갈증이 몰려왔다.\n한 사람이 칼을 내려놓자, 다른 이들도 무장을 벗어 던졌다.",
    "그들은 사과를 움켜쥐고, 시냇물에 얼굴을 묻었다.\n아무리 먹고 마셔도 허기는 가시지 않았다.",
    "마침내 정신이 돌아왔을 때,\n물 위에는 사람이 아닌 늑대의 얼굴이 비치고 있었다.",
    "사람이 사라지고, 가축이 사라졌다.\n왕명을 받은 젊은 장군이 경상도 남부의 작은 마을에 도착했다."};
static const Line MEET_LINES[] = {
    {"마을 이장", "밤마다 사람과 가축이 사라집니다.\n어제는 백정마저 집에 돌아오지 않았습니다."},
    {"마을 이장", "동쪽 숲길의 약재 수레가 습격당했습니다.\n그 곁에서 백정의 옷자락을 봤다는 이가 있습니다."},
    {"장군", "남겨진 흔적을 살피겠소.\n약재도 되찾아 오리다."}};
static const Line DELIVER_LINES[] = {
    {"약방 주인", "이 약재를 전부 되찾아 주셨군요!\n상처 입은 마을 사람들을 돌볼 수 있겠어요."},
    {"약방 주인", "숲에 가셨다면 이장님께도 알려 주세요.\n탕약과 지혈산을 챙겨 드릴게요."}};
static const Line REPORT_LINES[] = {
    {"장군", "수레에서 백정의 옷자락을 찾았소.\n식칼 자국 곁에는 짐승 발자국이 이어져 있었지."},
    {"마을 이장", "백정이… 짐승으로 변한 겁니까?\n동쪽 숲의 옛 도축터를 살펴 주십시오."},
    {"마을 이장", "대장간에서 채비를 하시고, 약방에도 들르십시오.\n부디 무사히 돌아오셔야 합니다."}};
static const Line RETURN_LINES[] = {
    {"마을 이장", "숲에서 들리던 소리가 멎었습니다.\n약재와 마을을 되찾아 주셔서 고맙습니다."},
    {"장군", "그 짐승은… 이 마을 사람이었소.\n아직 끝난 일이 아니오."},
    {"깨달음", "칼을 드는 이유를 다시 생각한다.\n이번 싸움이 몸에 남긴 감각을 붙잡는다."}};
static const Line BOSS_LINES[] = {{"변이한 백정", "으르르르… 크허엉!"},
                                  {"장군", "그 손의 식칼…\n그대가 이 마을의 백정인가."}};
static const Line MESSENGER_LINES[] = {
    {"윤겸 · 금위영 종사관", "장군! 전하의 서신입니다.\n전라도에서도 같은 일이 벌어지고 있습니다."},
    {"윤겸 · 금위영 종사관", "사람의 모습을 잃은 자들이 밤마다 고을을 헤집는다고 합니다."},
    {"장군", "말을 준비하게.\n다음 고을로 간다."}};
static void stop_input(App *a) {
    a->secret_down = a->secret_moved = false;
    a->secret_taps = 0;
    a->secret_last = 0;
    a->pointer_down = false;
    a->pointer_delta = (Vec2){0, 0};
    a->target_set = false;
    a->attack_pointer_down = a->attack_key_down = false;
    a->attack_swiping = a->attack_repeated = false;
    a->attack_direction = -1;
    a->attack_start = a->attack_anchor = (Vec2){0, 0};
    a->attack_hold = 0;
    a->game.queued_move = MOVE_NONE; a->game.buffer_time = 0;
    a->game.request_time = 0;
}
static void toggle_training(App *app) {
    if (app->width <= app->height) return;
    bool changed = app->game.training.active
        ? game_leave_training(&app->game, &app->training_return)
        : game_enter_training(&app->game, &app->training_return);
    if (!changed) return;
    stop_input(app);
    app->movement_blocked = true;
    app->button_count = 0;
}
static void say(App *a, const Line *lines, int count, DialogueDone done) {
    if (count > 5)
        count = 5;
    memcpy(a->dialogue, lines, (size_t)count * sizeof *lines);
    a->dialogue_count = count;
    a->dialogue_index = 0;
    a->dialogue_done = done;
    a->game.phase = PH_DIALOGUE;
    stop_input(a);
}
static void next_dialogue(App *a) {
    if (a->game.phase == PH_PROLOGUE) {
        a->story_elapsed = 0;
        if (++a->story_index == 5)
            game_start(&a->game);
        return;
    }
    if (a->game.phase != PH_DIALOGUE)
        return;
    if (++a->dialogue_index < a->dialogue_count)
        return;
    a->game.phase = PH_PLAY;
    switch (a->dialogue_done) {
    case DONE_ACCEPT:
        game_accept(&a->game);
        break;
    case DONE_DELIVER:
        game_deliver(&a->game);
        break;
    case DONE_REPORT:
        game_report(&a->game);
        break;
    case DONE_AUGMENT:
        a->game.phase = PH_AUGMENT;
        break;
    case DONE_BOSS:
        game_begin_boss(&a->game);
        break;
    case DONE_FINISH:
        a->game.phase = PH_COMPLETE;
        break;
    default:
        break;
    }
}
static void interact(App *a) {
    Game *g = &a->game;
    Nearby n = game_nearby(g);
    if (n.kind == NEAR_NONE)
        return;
    if (n.kind == NEAR_TRACE) {
        if (game_inspect(g, n.index)) {
            Line lines[2] = {{TRACES[n.index].name, TRACES[n.index].text},
                             {"장군", "짐승들이 약재를 물고 달아났군.\n남동쪽 빈터를 살펴보자."}};
            say(a, lines, g->quest == Q_RECOVER ? 2 : 1, DONE_NONE);
        }
        return;
    }
    if (n.index == HERBALIST && g->quest == Q_DELIVER) {
        say(a, DELIVER_LINES, 2, DONE_DELIVER);
        return;
    }
    if (n.index >= SMITH && n.index <= INNKEEPER) {
        if (game_open_shop(g, n.index))
            stop_input(a);
        return;
    }
    if (n.index == MESSENGER) {
        say(a, MESSENGER_LINES, 3, DONE_FINISH);
        return;
    }
    if (g->quest == Q_MEET)
        say(a, MEET_LINES, 3, DONE_ACCEPT);
    else if (g->quest == Q_REPORT)
        say(a, REPORT_LINES, 3, DONE_REPORT);
    else if (g->quest == Q_RETURN)
        say(a, RETURN_LINES, 3, DONE_AUGMENT);
    else {
        const char *s =
            g->quest == Q_TRACES ? "수레는 마을 동쪽에 있습니다.\n옷자락과 식칼 자국을 찾아 주십시오."
            : g->quest == Q_RECOVER
                ? "약재를 되찾으시면 약방으로 가져다주십시오.\n다친 이들이 기다리고 있습니다."
            : g->quest == Q_DELIVER   ? "약방 주인이 꾸러미를 기다리고 있습니다."
            : g->quest == Q_MESSENGER ? "한양에서 전령이 왔습니다.\n마을 어귀에서 장군을 기다리고 있지요."
                                      : "동쪽 도축터에서 울음소리가 들립니다.\n몸을 잘 챙기십시오.";
        Line line = {"마을 이장", s};
        say(a, &line, 1, DONE_NONE);
    }
}
static void dispatch(App *a, Action action, int value) {
    Game *g = &a->game;
    switch (action) {
    case ACT_START:
        if (g->phase == PH_TITLE) {
            g->phase = PH_PROLOGUE;
            a->story_index = 0;
            a->story_elapsed = 0;
            stop_input(a);
        }
        break;
    case ACT_SKIP:
        if (g->phase == PH_TITLE || g->phase == PH_PROLOGUE) {
            game_start(g);
            stop_input(a);
        }
        break;
    case ACT_NEXT:
        next_dialogue(a);
        break;
    case ACT_INTERACT:
        interact(a);
        break;
    case ACT_PICKUP:
        game_pickup_nearby(g);
        break;
    case ACT_SKILL:
        dispatch(a, ACT_TECHNIQUE, g->swipe_bindings[SWIPE_UP]);
        break;
    case ACT_ATTACK:
        if (a->width <= a->height) break;
        a->target_set = false;
        game_attack(g);
        break;
    case ACT_TECHNIQUE:
        if (a->width <= a->height) break;
        a->target_set = false;
        if (!game_technique(g, (Technique)value) && g->phase == PH_PLAY &&
            value >= 0 && value < TECH_COUNT && game_technique_cooldown(g, (Technique)value) > 0) {
            char message[96];
            snprintf(message, sizeof message, "%s · %.1f초 후", TECHNIQUES[value].name,
                     game_technique_cooldown(g, (Technique)value));
            game_toast(g, message);
        }
        break;
    case ACT_TECHNIQUES:
        if (g->phase == PH_PLAY || g->phase == PH_BAG) {
            g->phase = PH_TECHNIQUES; stop_input(a);
        } else if (g->phase == PH_TECHNIQUES) game_close_panel(g);
        break;
    case ACT_SWIPE_SELECT:
        if (g->phase == PH_TECHNIQUES && value >= 0 && value < SWIPE_COUNT)
            a->selected_swipe = value;
        break;
    case ACT_TECH_BIND:
        game_bind_swipe(g, (Swipe)a->selected_swipe, (Technique)value);
        break;
    case ACT_BIND_RESET:
        if (g->phase == PH_TECHNIQUES) {
            g->swipe_bindings[SWIPE_UP] = TECH_CLEAVE;
            g->swipe_bindings[SWIPE_RIGHT] = TECH_SWEEP;
            g->swipe_bindings[SWIPE_DOWN] = TECH_RETREAT;
            g->swipe_bindings[SWIPE_LEFT] = TECH_THRUST;
        }
        break;
    case ACT_BAG_TAB:
        if (g->phase == PH_BAG && value >= 0 && value < 4) {
            a->bag_tab = value; a->bag_selected = -999;
        }
        break;
    case ACT_BAG_SELECT:
        if (g->phase == PH_BAG) a->bag_selected = value;
        break;
    case ACT_GEAR_SELECT:
        if (g->phase == PH_BAG) {
            a->bag_tab = 0;
            a->bag_selected = value == 0 ? (g->sword ? ITEM_SWORD : 100)
                : value == 2 ? (g->armor ? ITEM_ARMOR : 101) : 200 + value;
        }
        break;
    case ACT_EQUIP:
        game_equip(g, (Item)value, value == ITEM_SWORD ? !g->sword : !g->armor);
        break;
    case ACT_FULLSCREEN:
#ifdef __EMSCRIPTEN__
        web_fullscreen();
#else
        SDL_SetWindowFullscreen(a->window, SDL_GetWindowFlags(a->window) & SDL_WINDOW_FULLSCREEN_DESKTOP
                                               ? 0
                                               : SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
        stop_input(a);
        break;
    case ACT_PAUSE:
        if (g->phase == PH_PLAY) {
            g->phase = PH_PAUSED;
            stop_input(a);
        } else
            game_close_panel(g);
        break;
    case ACT_BAG:
        if (g->training.active) break;
        if (g->phase == PH_PLAY) {
            g->phase = PH_BAG;
            a->bag_tab = 0; a->bag_selected = 100;
            stop_input(a);
        } else if (g->phase == PH_BAG)
            game_close_panel(g);
        break;
    case ACT_JOURNAL:
        if (g->training.active) break;
        if (g->phase == PH_PLAY) {
            g->phase = PH_JOURNAL;
            a->journal_scroll = 0;
            stop_input(a);
        } else if (g->phase == PH_JOURNAL)
            game_close_panel(g);
        break;
    case ACT_CLOSE:
        game_close_panel(g);
        stop_input(a);
        break;
    case ACT_TITLE:
        if (g->training.active) {
            toggle_training(a);
            break;
        }
        game_reset(g);
        stop_input(a);
        break;
    case ACT_RECOVER:
        if (g->phase == PH_DEAD) {
            if (g->replay)
                game_retry(g);
            else
                game_recover(g);
            stop_input(a);
        }
        break;
    case ACT_REPLAY:
        if (g->phase == PH_COMPLETE) {
            g->replay = true;
            game_retry(g);
            stop_input(a);
        }
        break;
    case ACT_BUY:
        game_buy(g, (Item)value);
        break;
    case ACT_USE:
        game_use(g, (Item)value);
        break;
    case ACT_CHOOSE:
        game_choose(g, (Augment)value);
        break;
    case ACT_TRAINING_EXIT:
        if (g->training.active) toggle_training(a);
        break;
    }
}
static void surface(Gfx *f, SDL_FRect r, float radius, SDL_Color fill) {
    gfx_round(f, (SDL_FRect){r.x, r.y + 3, r.w, r.h}, radius, (SDL_Color){3, 12, 15, 50});
    gfx_round(f, r, radius, fill);
    gfx_round_border(f, r, radius, (SDL_Color){207, 185, 139, 65});
}
static void button(App *a, SDL_FRect r, const char *label, Action action, int value, bool enabled,
                   bool primary) {
    if (a->button_count < BUTTON_MAX)
        a->buttons[a->button_count++] = (Button){r, action, value, enabled};
    SDL_Color fill = primary ? (SDL_Color){223, 201, 156, 255} : (SDL_Color){21, 39, 40, 235};
    if (!enabled) fill = (SDL_Color){31, 46, 44, 220};
    /* Keep the touch hitbox; only reduce the visible control and padding. */
    SDL_FRect visual = {r.x + 3, r.y + 5, r.w - 6, r.h - 10};
    surface(&a->gfx, visual, fminf(12, visual.h / 2), fill);
    gfx_label(&a->gfx, label, visual, 14, enabled ? (primary ? INK : PAPER) : (SDL_Color){153, 166, 151, 255});
}
static SDL_FRect panel(App *a, const char *kicker, const char *title, float desired_h) {
    float width = fminf(640, a->width - 24), height = fminf(desired_h, a->height - 20);
    SDL_FRect p = {(a->width - width) / 2, (a->height - height) / 2, width, height};
    gfx_rect(&a->gfx, (SDL_FRect){0, 0, a->width, a->height}, (SDL_Color){4, 17, 20, 150});
    surface(&a->gfx, p, 16, INK);
    gfx_text(&a->gfx, kicker, p.x + 18, p.y + 9, 11, p.w - 150, GOLD);
    gfx_heading(&a->gfx, title, (SDL_FRect){p.x + 12, p.y + 25, 132, 30}, 29, PAPER);
    button(a, (SDL_FRect){p.x + p.w - 87, p.y + 10, 72, 44}, "닫기", ACT_CLOSE, 0, true, false);
    gfx_line(&a->gfx, (Vec2){p.x + 18, p.y + 60}, (Vec2){p.x + p.w - 18, p.y + 60},
             (SDL_Color){207, 185, 139, 42});
    return p;
}
static void hud_card(Gfx *f, SDL_FRect r, Uint8 alpha) {
    gfx_round(f, r, 11, (SDL_Color){12, 28, 31, alpha});
}
static const char *move_name(int move) {
    return move == MOVE_ATTACK ? "기본 공격"
        : move >= 0 && move < TECH_COUNT ? TECHNIQUES[move].name : "없음";
}
static const char *request_name(const Game *game) {
    const char *names[] = {"준비", "발동", "예약", "재사용 중", "방향 불명", "입력 불가"};
    return names[game->combat_request];
}
static void draw_training_status(App *app) {
    Game *game = &app->game;
    char lines[10][96];
    snprintf(lines[0], sizeof lines[0], "타격 %u · 미스 %u", game->training.hits, game->training.misses);
    snprintf(lines[1], sizeof lines[1], "피해 %d · 입력 %d/3", game->training.last_damage, game->input_count);
    snprintf(lines[2], sizeof lines[2], "명중 %d/3 · %.1f초", game->combo_count, game->combo_time);
    snprintf(lines[3], sizeof lines[3], "예약: %s", move_name(game->queued_move));
    snprintf(lines[4], sizeof lines[4], "%s · %s", move_name(game->input_move), request_name(game));
    snprintf(lines[5], sizeof lines[5], "기본 공격 %.1f초", game->attack_cooldown);
    const char *directions[] = {"상", "우", "하", "좌"};
    for (int direction = 0; direction < SWIPE_COUNT; direction++) {
        Technique technique = game->swipe_bindings[direction];
        snprintf(lines[6 + direction], sizeof lines[6 + direction], "%s %s %.1f초", directions[direction],
                 TECHNIQUES[technique].name, game_technique_cooldown(game, technique));
    }
    hud_card(&app->gfx, (SDL_FRect){12, 58, 148, 146}, 225);
    for (int line = 0; line < 10; line++)
        gfx_text(&app->gfx, lines[line], 18, 61 + line * 14, 12, 134, line < 5 ? PAPER : GOLD);
}
static void draw_quest_tracker(App *a) {
    Game *g = &a->game;
    Gfx *f = &a->gfx;
    SDL_FRect r = {12, 62, fminf(250, a->width * .52f), 44};
    if (a->button_count < BUTTON_MAX)
        a->buttons[a->button_count++] = (Button){r, ACT_JOURNAL, 0, true};
    hud_card(f, r, 196);
    gfx_rect(f, (SDL_FRect){r.x, r.y + 10, 2, r.h - 20}, (SDL_Color){227, 193, 131, 205});
    gfx_text(f, QUESTS[g->quest].title, r.x + 12, r.y + 5, 11, 0, GOLD);
    gfx_text(f, a->touch_input ? "여정" : "여정 J", r.x + r.w - 54, r.y + 5, 10, 0, MUTED);
    char text[160];
    game_objective_brief(g, text, sizeof text);
    float size = 13;
    while (size > 11 && gfx_measure(f, text, size) > r.w - 24)
        size -= .5f;
    gfx_text_box(f, text, (SDL_FRect){r.x + 10, r.y + 23, r.w - 20, 19}, size, 11, PAPER);
}
static void draw_notification(App *a) {
    Game *g = &a->game;
    if (g->phase != PH_PLAY || g->toast_time <= 0)
        return;
    Gfx *f = &a->gfx;
    float fade = fminf(clampf(g->toast_time / .30f, 0, 1),
                       clampf((TOAST_SECONDS - g->toast_time) / .16f, 0, 1));
    float size = 12, limit = fminf(316, a->width * .49f);
    while (size > 10 && gfx_measure(f, g->toast, size) > limit - 24)
        size -= .5f;
    SDL_FRect r = {12, 112 + (1 - fade) * 4, fminf(limit, gfx_measure(f, g->toast, size) + 24), 26};
    hud_card(f, r, (Uint8)(182 * fade));
    /* Clip custom/developer notices too: a long message must never grow into the map. */
    SDL_Rect clip = {(int)r.x + 8, (int)r.y, (int)r.w - 16, (int)r.h};
    SDL_RenderSetClipRect(a->renderer, &clip);
    gfx_label(f, g->toast, r, size, (SDL_Color){228, 218, 192, (Uint8)(255 * fade)});
    SDL_RenderSetClipRect(a->renderer, NULL);
}
static void draw_hud(App *a) {
    Gfx *f = &a->gfx;
    Game *g = &a->game;
    float w = a->width, h = a->height;
    char text[512];
    hud_card(f, (SDL_FRect){12, 8, 190, 44}, 214);
    gfx_text(f, g->training.active ? "비밀 수련장 · 격리된 빈터" : "제1장 · 경상도 남부", 24, 13, 11, 0, GOLD);
    gfx_round(f, (SDL_FRect){53, 38, 85, 4}, 2.5f, (SDL_Color){25, 18, 19, 255});
    gfx_round(f, (SDL_FRect){53, 38, 85 * (float)g->hero.hp / 100, 4}, 2.5f, RED);
    gfx_text(f, "체력", 24, 32, 10, 0, MUTED);
    snprintf(text, sizeof text, "%d/100", g->hero.hp);
    gfx_text(f, text, 149, 32, 10, 0, MUTED);
    if (g->phase == PH_PLAY) {
        button(a, (SDL_FRect){w - 52, 4, 44, 44}, "정지", ACT_PAUSE, 0, true, false);
#ifndef __ANDROID__
        button(a, (SDL_FRect){w - 100, 4, 44, 44}, "화면", ACT_FULLSCREEN, 0, true, false);
#endif
        if (g->training.active) {
            button(a, (SDL_FRect){w - 196, 4, 92, 44}, "훈련 종료", ACT_TRAINING_EXIT, 0, true, false);
            draw_training_status(a);
        } else {
            snprintf(text, sizeof text, "가방 · %d문", g->coins);
            button(a, (SDL_FRect){w - 196, 4, 92, 44}, text, ACT_BAG, 0, true, false);
            draw_quest_tracker(a);
        }
        const char *weapon = g->augment == AUG_STEEL  ? "강철 대검"
                             : g->augment == AUG_WIND ? "환도 · 바람의 깃"
                             : g->sword               ? "단련한 환도"
                                                      : "환도";
        SDL_FRect weapon_card = {12, h - 44, w < 800 ? 104 : 218, 32};
        if (g->training.active)
            button(a, (SDL_FRect){12, h - 54, 104, 44}, "무공 배치", ACT_TECHNIQUES, 0, true, false);
        else {
            hud_card(f, weapon_card, 205);
            if (w < 800)
                gfx_label(f, weapon, weapon_card, 13, GOLD);
            else {
                gfx_text(f, weapon, 24, h - 43, 13, 0, GOLD);
                gfx_text(f, "Z 공격 · Q/R/G/T 무공", 24, h - 26, 10, 0, MUTED);
            }
        }
        /* Keep the mobile attack disc far enough from both edges for a
         * 32-pixel down/right swipe, and shift quick items out of its way. */
        float item_x = a->touch_input ? w - 260 : w - 198;
        if (!g->training.active) {
            snprintf(text, sizeof text, "탕약 %d", g->tonics);
            button(a, (SDL_FRect){item_x, a->touch_input ? h - 115 : h - 98, 78, 44},
                   text, ACT_USE, ITEM_TONIC, game_can_use(g, ITEM_TONIC), false);
            snprintf(text, sizeof text, "지혈산 %d", g->bandages);
            button(a, (SDL_FRect){item_x, a->touch_input ? h - 66 : h - 54, 78, 44},
                   text, ACT_USE, ITEM_BANDAGE, game_can_use(g, ITEM_BANDAGE), false);
            button(a, (SDL_FRect){item_x, a->touch_input ? h - 164 : h - 142, 78, 44},
                   a->touch_input ? "무공" : "무공 K", ACT_TECHNIQUES, 0, true, false);
        }
        SDL_FRect attack = a->touch_input ? (SDL_FRect){w - 160, h - 160, 88, 88}
                                           : (SDL_FRect){w - 116, h - 116, 88, 88};
        if (a->button_count < BUTTON_MAX)
            a->buttons[a->button_count++] = (Button){attack, ACT_ATTACK, 0, true};
        Vec2 center = {attack.x + 44, attack.y + 44};
        gfx_disc(f, center, 42, a->attack_pointer_down
            ? (SDL_Color){217, 191, 139, 245} : (SDL_Color){30, 49, 48, 240}, GOLD);
        gfx_label(f, "공격", (SDL_FRect){attack.x + 3, attack.y + 16, 82, 30}, 21,
                  a->attack_pointer_down ? INK : PAPER);
        if (g->attack_cooldown > 0) snprintf(text, sizeof text, "%.1f", g->attack_cooldown);
        else snprintf(text, sizeof text, "%s", a->touch_input ? "탭 / 밀기" : "Z / 밀기");
        gfx_label(f, text, (SDL_FRect){attack.x + 3, attack.y + 48, 82, 20}, 11,
                  a->attack_pointer_down ? INK : GOLD);
        int direction = a->attack_pointer_down ? a->attack_direction : -1;
        if (g->request_time > 0 && g->combat_request >= TRAIN_QUEUED) {
            snprintf(text, sizeof text, "%s", request_name(g));
        } else if (g->last_impact == IMPACT_MISS && g->input_time > 0) {
            snprintf(text, sizeof text, "미스 · 명중 0/3");
        } else if (direction >= 0) {
            Technique tech = g->swipe_bindings[direction];
            float cd = game_technique_cooldown(g, tech);
            snprintf(text, sizeof text, cd > 0 ? "%s · %.1f초" : "%s", TECHNIQUES[tech].name, cd);
        } else if (g->combo_count > 1 && g->combo_time > 0)
            snprintf(text, sizeof text, "명중 %d연계", g->combo_count);
        else snprintf(text, sizeof text, "밀어서 무공");
        SDL_FRect attack_hint = {center.x - 54, attack.y - 37, 108, 26};
        hud_card(f, attack_hint, 205);
        gfx_label(f, text, attack_hint, 11, GOLD);
        if (a->attack_pointer_down) {
            const Vec2 vectors[] = {{0,-1},{1,0},{0,1},{-1,0}};
            for (int i = 0; i < SWIPE_COUNT; i++) {
                Vec2 n = vectors[i], tip = {center.x + n.x * 60, center.y + n.y * 60};
                SDL_Color c = i == direction ? PAPER : GOLD;
                gfx_line(f, (Vec2){center.x + n.x * 48, center.y + n.y * 48}, tip, c);
                gfx_line(f, tip, (Vec2){tip.x - n.x * 6 - n.y * 4, tip.y - n.y * 6 + n.x * 4}, c);
                gfx_line(f, tip, (Vec2){tip.x - n.x * 6 + n.y * 4, tip.y - n.y * 6 - n.x * 4}, c);
            }
        }
        Nearby near = game_nearby(g);
        if (near.kind != NEAR_NONE) {
            if (near.kind == NEAR_TRACE)
                snprintf(text, sizeof text, "흔적 조사 · Space");
            else if (near.index == HERBALIST && g->quest == Q_DELIVER)
                snprintf(text, sizeof text, "약재 전달 · Space");
            else if (near.index >= SMITH && near.index <= INNKEEPER)
                snprintf(text, sizeof text, "%s · Space", NPCS[near.index].role);
            else
                snprintf(text, sizeof text, "대화 · Space");
            if (a->touch_input || w < 680) {
                char *hint = strstr(text, " · Space");
                if (hint) *hint = '\0';
            }
            if (a->touch_input && w < 680) {
                /* Compact landscape: avoid the potion column entirely. */
                button(a, (SDL_FRect){12, h - 100, 104, 44}, text, ACT_INTERACT, 0, true, false);
            } else {
                float left = w < 800 ? 122 : 242;
                float right = w - (a->touch_input ? 280 : 210);
                float bw = fminf(178, right - left);
                button(a, (SDL_FRect){left + (right - left - bw) / 2, h - 54, bw, 44},
                       text, ACT_INTERACT, 0, true, false);
            }
        }
    }
    if (g->quest == Q_COMBAT) {
        float bw = fminf(230, w * .32f);
        SDL_FRect r = {w - bw - 14, 54, bw, 42};
        hud_card(f, r, 220);
        gfx_text(f, "변이한 백정", r.x + 12, r.y + 7, 14, 0, PAPER);
        snprintf(text, sizeof text, "%d", g->boss.hp);
        gfx_text(f, text, r.x + bw - 44, r.y + 7, 13, 0, MUTED);
        gfx_rect(f, (SDL_FRect){r.x + 12, r.y + 30, bw - 24, 4}, (SDL_Color){57, 38, 35, 255});
        gfx_rect(f, (SDL_FRect){r.x + 12, r.y + 30, (bw - 24) * (float)g->boss.hp / 240, 4}, RED);
    }
    if (g->hero.bleed > 0) {
        snprintf(text, sizeof text, "출혈 · %.0f초", ceilf(g->hero.bleed));
        gfx_text(f, text, 18, h - 85, 15, 0, (SDL_Color){255, 198, 175, 255});
    }
}
static void draw_shop(App *a) {
    Game *g = &a->game;
    Gfx *f = &a->gfx;
    const char *line = g->shop == SMITH ? "검날을 벼리고, 돌아올 몸을 지키시오."
        : g->shop == HERBALIST ? "탕약과 지혈산을 챙기세요. 숲에서는 상처를 돌볼 틈이 적으니까요."
                               : "따뜻한 국밥 한 그릇 드시고 기운 차리세요.";
    SDL_FRect p = panel(a, NPCS[g->shop].name, NPCS[g->shop].role, 264);
    char s[256];
    gfx_text_box(f, line, (SDL_FRect){p.x + 16, p.y + 64, p.w - 32, 32}, 13, 12, MUTED);
    snprintf(s, sizeof s, "소지금 %d문", g->coins);
    gfx_text_box(f, s, (SDL_FRect){p.x + 16, p.y + 96, p.w - 32, 18}, 13, 12, GOLD);
    int row = 0;
    float row_h = fminf(62, (p.h - 132) / 2);
    for (int i = 0; i < ITEM_COUNT; i++)
        if ((int)ITEMS[i].shop == g->shop) {
            float y = p.y + 118 + (row_h + 6) * (float)row++;
            gfx_round(f, (SDL_FRect){p.x + 16, y, p.w - 32, row_h}, 10, (SDL_Color){34, 53, 49, 220});
            gfx_text_box(f, ITEMS[i].name, (SDL_FRect){p.x + 25, y + 6, p.w - 158, 21}, 15, 13, PAPER);
            gfx_text_box(f, ITEMS[i].description, (SDL_FRect){p.x + 25, y + 28, p.w - 158, row_h - 30}, 12, 11, MUTED);
            const char *error = game_purchase_error(g, (Item)i);
            if (error) snprintf(s, sizeof s, "%s", error);
            else snprintf(s, sizeof s, "%d문 · %s", ITEMS[i].price, i == ITEM_MEAL ? "쉬기" : "구입");
            button(a, (SDL_FRect){p.x + p.w - 123, y + (row_h - 44) / 2, 102, 44}, s, ACT_BUY, i,
                   error == NULL, true);
        }
}
typedef struct {
    int key, count;
    const char *name, *description;
} BagEntry;
static int bag_entries(const Game *g, int tab, BagEntry entries[8]) {
    int n = 0;
    if (tab == 0) {
        entries[n++] = (BagEntry){100, 1, "환도", "기본 무기 · 단련한 환도 해제 시 사용"};
        entries[n++] = (BagEntry){101, 1, "군영 갑주", "기본 방어구 · 누비 갑옷 해제 시 사용"};
        if (g->owned_sword || g->sword)
            entries[n++] = (BagEntry){ITEM_SWORD, 1, "단련한 환도", "무기 · 공격 피해 +5"};
        if (g->owned_armor || g->armor)
            entries[n++] = (BagEntry){ITEM_ARMOR, 1, "누비 갑옷", "방어구 · 받는 피해 -3"};
    } else if (tab == 1) {
        if (g->tonics > 0) entries[n++] = (BagEntry){ITEM_TONIC, g->tonics, "탕약", "체력 45 회복"};
        if (g->bandages > 0) entries[n++] = (BagEntry){ITEM_BANDAGE, g->bandages, "지혈산", "출혈 즉시 해제"};
    } else if (tab == 3 && g->herbs > 0)
        entries[n++] = (BagEntry){102, g->herbs, "약재 꾸러미", "의뢰품 · 약방 주인에게 전달할 약재"};
    return n;
}
static void bag_icon(Gfx *f, int key, SDL_FRect r, SDL_Color c) {
    Vec2 p = {r.x + r.w / 2, r.y + r.h / 2};
    float z = fminf(r.w, r.h) * .34f;
    if (key == ITEM_SWORD || key == 100) {
        gfx_line(f, (Vec2){p.x-z,p.y+z}, (Vec2){p.x+z,p.y-z}, c);
        gfx_line(f, (Vec2){p.x-z*.7f,p.y+z*1.1f}, (Vec2){p.x-z*1.1f,p.y+z*.7f}, c);
        gfx_line(f, (Vec2){p.x-z*.6f,p.y+z*.2f}, (Vec2){p.x-z*.2f,p.y+z*.6f}, c);
        gfx_line(f, (Vec2){p.x+z,p.y-z}, (Vec2){p.x+z*.4f,p.y-z*.8f}, c);
    } else if (key == ITEM_ARMOR || key == 101) {
        gfx_line(f, (Vec2){p.x-z,p.y-z*.7f}, (Vec2){p.x-z*.45f,p.y-z}, c);
        gfx_line(f, (Vec2){p.x-z*.45f,p.y-z}, (Vec2){p.x,p.y-z*.35f}, c);
        gfx_line(f, (Vec2){p.x,p.y-z*.35f}, (Vec2){p.x+z*.45f,p.y-z}, c);
        gfx_line(f, (Vec2){p.x+z*.45f,p.y-z}, (Vec2){p.x+z,p.y-z*.7f}, c);
        gfx_border(f, (SDL_FRect){p.x-z*.65f,p.y-z*.35f,z*1.3f,z*1.35f}, c);
    } else if (key == ITEM_TONIC || key == ITEM_BANDAGE || key == 102) {
        gfx_round_border(f, (SDL_FRect){p.x-z*.7f,p.y-z*.5f,z*1.4f,z*1.5f}, 4, c);
        gfx_line(f, (Vec2){p.x-z*.35f,p.y-z}, (Vec2){p.x+z*.35f,p.y-z}, c);
        gfx_line(f, (Vec2){p.x,p.y-z*.2f}, (Vec2){p.x,p.y+z*.6f}, c);
        gfx_line(f, (Vec2){p.x-z*.4f,p.y+z*.2f}, (Vec2){p.x+z*.4f,p.y+z*.2f}, c);
    } else gfx_circle(f, p, z*.65f, c);
}
static bool bag_equipped(const Game *g, int key) {
    return key == ITEM_SWORD ? g->sword : key == ITEM_ARMOR ? g->armor
        : key == 100 ? !g->sword : key == 101 ? !g->armor : false;
}
static void draw_bag(App *a) {
    Gfx *f = &a->gfx; Game *g = &a->game;
    float w = fminf(900, a->width-20), h = fminf(520, a->height-20);
    SDL_FRect p = {(a->width-w)/2,(a->height-h)/2,w,h};
    gfx_rect(f, (SDL_FRect){0,0,a->width,a->height}, (SDL_Color){4,17,20,175});
    surface(f,p,16,INK);
    gfx_heading(f,"가방",(SDL_FRect){p.x+16,p.y+8,90,32},28,PAPER);
    char text[192]; snprintf(text,sizeof text,"%d문 · 체력 %d/100",g->coins,g->hero.hp);
    gfx_text_box(f,text,(SDL_FRect){p.x+106,p.y+17,p.w-204,20},13,11,GOLD);
    button(a,(SDL_FRect){p.x+p.w-86,p.y+3,76,44},"닫기",ACT_CLOSE,0,true,false);
    float top = p.y+51, content = p.h-63, lw = fminf(240,(p.w-32)*.31f);
    SDL_FRect left = {p.x+12,top,lw,content};
    float rx = left.x+lw+12, rw = p.x+p.w-12-rx;
    gfx_round(f,left,11,(SDL_Color){24,43,43,255});
    float portrait = content<220 ? 42 : fminf(130,content*.30f);
    gfx_sprite(f,&f->hero,(Vec2){left.x+lw/2,top+portrait-2},lw*.75f,portrait-4,true);
    const char *labels[] = {"무기","투구","갑옷","신발","목걸이","반지"};
    float sy = top+portrait+3, sh = (content-portrait-30)/3, sw = (lw-18)/2;
    for (int i=0;i<6;i++) {
        SDL_FRect slot = {left.x+6+(sw+6)*(i%2),sy+sh*(i/2),sw,sh-4};
        bool active = (i==0 || i==2);
        gfx_round(f,slot,6,active ? (SDL_Color){39,61,56,255} : (SDL_Color){19,36,38,255});
        gfx_text_box(f,labels[i],(SDL_FRect){slot.x+5,slot.y+2,slot.w-10,13},10,9,GOLD);
        const char *name = i==0 ? (g->sword?(lw<150?"환도 +":"단련한 환도"):"환도")
            : i==2 ? (g->armor?(lw<150?"누비옷":"누비 갑옷"):(lw<150?"군영복":"군영 갑주")) : "미장착";
        gfx_text_box(f,name,(SDL_FRect){slot.x+5,slot.y+16,slot.w-10,slot.h-17},12,10,active?PAPER:MUTED);
        if(a->button_count<BUTTON_MAX) a->buttons[a->button_count++] = (Button){slot,ACT_GEAR_SELECT,i,true};
    }
    snprintf(text,sizeof text,"공격 +%d · 방어 %d",g->sword?5:0,g->armor?3:0);
    gfx_label(f,text,(SDL_FRect){left.x+4,top+content-24,lw-8,20},11,GOLD);
    const char *tabs[] = {"장비","소비","기타","의뢰품"};
    for(int i=0;i<4;i++) {
        SDL_FRect tab = {rx+(rw/4)*i,top,rw/4-3,29};
        gfx_round(f,tab,6,a->bag_tab==i ? GOLD : (SDL_Color){27,47,46,255});
        gfx_label(f,tabs[i],tab,12,a->bag_tab==i?INK:MUTED);
        if(a->button_count<BUTTON_MAX) a->buttons[a->button_count++] = (Button){tab,ACT_BAG_TAB,i,true};
    }
    BagEntry entries[8]; int n = bag_entries(g,a->bag_tab,entries), selected=-1;
    for(int i=0;i<n;i++) if(entries[i].key==a->bag_selected) selected=i;
    if(selected<0 && n>0 && a->bag_selected<200) { selected=0; a->bag_selected=entries[0].key; }
    float detail_h = a->height<340?61:88;
    SDL_FRect grid = {rx,top+37,rw,content-37-detail_h-6};
    int columns = rw>=430?4:2;
    float gap=6, cellw=(rw-gap*(columns-1))/columns;
    int rows = (int)(grid.h/78); if(rows<2) rows=2;
    if(rows<(n+columns-1)/columns) rows=(n+columns-1)/columns;
    float cellh = fminf(92,(grid.h-gap*(rows-1))/rows);
    for(int i=0;i<columns*rows;i++) {
        SDL_FRect cell={rx+(cellw+gap)*(i%columns),grid.y+(cellh+gap)*(i/columns),cellw,cellh};
        if(i>=n) {
            gfx_round(f,cell,8,(SDL_Color){19,36,38,255});
            gfx_round_border(f,cell,8,(SDL_Color){56,76,67,140});
            continue;
        }
        BagEntry e=entries[i]; bool equipped=bag_equipped(g,e.key);
        gfx_round(f,cell,8,i==selected?(SDL_Color){49,68,58,255}:(SDL_Color){24,44,45,255});
        gfx_round_border(f,cell,8,i==selected?GOLD:(SDL_Color){77,96,82,160});
        float icon=fminf(32,cell.h-8);
        bag_icon(f,e.key,(SDL_FRect){cell.x+4,cell.y+4,icon,icon},GOLD);
        float tx=cell.x+icon+5, tw=cell.w-icon-10;
        gfx_text_box(f,e.name,(SDL_FRect){tx,cell.y+5,tw,cell.h<50?cell.h-9:28},13,10,PAPER);
        if(cell.h<50 && a->bag_tab!=0) {
            snprintf(text,sizeof text,"%d",e.count);
            gfx_text_box(f,text,(SDL_FRect){cell.x+cell.w-23,cell.y+cell.h-15,20,13},10,10,GOLD);
        }
        if(cell.h>=50) {
            snprintf(text,sizeof text,equipped?"장착 중":a->bag_tab==0?"장비":"× %d",e.count);
            gfx_text_box(f,text,(SDL_FRect){tx,cell.y+cell.h-23,tw,18},11,10,equipped?GOLD:MUTED);
        }
        if(a->button_count<BUTTON_MAX) a->buttons[a->button_count++] = (Button){cell,ACT_BAG_SELECT,e.key,true};
    }
    if(n==0) gfx_label(f,a->bag_tab==2?"보관 중인 기타 물품이 없습니다":"보관 중인 물품이 없습니다",grid,13,MUTED);
    SDL_FRect detail={rx,top+content-detail_h,rw,detail_h};
    gfx_round(f,detail,8,(SDL_Color){24,43,42,255});
    if(selected>=0) {
        BagEntry e=entries[selected]; bool actionable=e.key>=0 && e.key<=ITEM_BANDAGE;
        float reserved=actionable?80:8;
        gfx_text_box(f,e.name,(SDL_FRect){rx+9,detail.y+6,rw-reserved-16,22},14,11,PAPER);
        gfx_text_box(f,e.description,(SDL_FRect){rx+9,detail.y+29,rw-reserved-16,detail.h-34},12,10,MUTED);
        if(actionable) {
            bool gear=e.key==ITEM_SWORD || e.key==ITEM_ARMOR;
            button(a,(SDL_FRect){rx+rw-80,detail.y+(detail_h-44)/2,76,44},gear?(bag_equipped(g,e.key)?"해제":"장착"):"사용",
                   gear?ACT_EQUIP:ACT_USE,e.key,gear||game_can_use(g,(Item)e.key),true);
        }
    } else gfx_text_box(f,a->bag_selected>=200?"장착된 장비가 없는 부위입니다":"물품을 선택하면 자세히 볼 수 있습니다",
            (SDL_FRect){rx+10,detail.y+9,rw-20,detail.h-18},13,11,MUTED);
}
static void draw_techniques(App *a) {
    Game *g=&a->game; Gfx *f=&a->gfx;
    SDL_FRect p=panel(a,"공격 버튼을 밀어 무공 연결","무공 설정",430);
    float top=p.y+69, bottom=p.y+p.h-42;
    float lw=fminf(200,(p.w-42)*.39f), rx=p.x+18+lw+12, rw=p.x+p.w-18-rx;
    const char *directions[]={"↑ 위 · Q","→ 오른쪽 · R","↓ 아래 · G","← 왼쪽 · T"};
    float rowh=(bottom-top)/4;
    for(int i=0;i<SWIPE_COUNT;i++) {
        SDL_FRect r={p.x+18,top+rowh*i,lw,rowh-5};
        gfx_round(f,r,7,i==a->selected_swipe?(SDL_Color){59,70,55,255}:(SDL_Color){25,44,44,255});
        if(i==a->selected_swipe) gfx_round_border(f,r,7,GOLD);
        float suby=rowh<48?16:24;
        gfx_text_box(f,directions[i],(SDL_FRect){r.x+8,r.y+2,r.w-16,suby-2},12,10,GOLD);
        gfx_text_box(f,TECHNIQUES[g->swipe_bindings[i]].name,(SDL_FRect){r.x+8,r.y+suby,r.w-16,r.h-suby},14,10,PAPER);
        if(a->button_count<BUTTON_MAX) a->buttons[a->button_count++] = (Button){r,ACT_SWIPE_SELECT,i,true};
    }
    float ch=(bottom-top-6)/2, cw=(rw-6)/2;
    for(int i=0;i<TECH_COUNT;i++) {
        SDL_FRect r={rx+(cw+6)*(i%2),top+(ch+6)*(i/2),cw,ch};
        bool selected=g->swipe_bindings[a->selected_swipe]==(Technique)i;
        gfx_round(f,r,8,selected?(SDL_Color){53,67,51,255}:(SDL_Color){24,43,43,255});
        gfx_round_border(f,r,8,selected?GOLD:(SDL_Color){82,95,76,150});
        gfx_text_box(f,TECHNIQUES[i].name,(SDL_FRect){r.x+8,r.y+7,r.w-16,24},16,12,PAPER);
        char s[100]; snprintf(s,sizeof s,"피해 %d · %.1f초",TECHNIQUES[i].damage,TECHNIQUES[i].cooldown);
        gfx_text_box(f,s,(SDL_FRect){r.x+8,r.y+33,r.w-16,18},11,10,GOLD);
        if(ch>85) gfx_text_box(f,TECHNIQUES[i].description,(SDL_FRect){r.x+8,r.y+57,r.w-16,r.h-62},12,11,MUTED);
        if(a->button_count<BUTTON_MAX) a->buttons[a->button_count++] = (Button){r,ACT_TECH_BIND,i,true};
    }
    button(a,(SDL_FRect){p.x+p.w-180,p.y+10,90,44},"기본 배치",ACT_BIND_RESET,0,true,false);
    gfx_text_box(f,"방향 선택 → 기술 선택 · 서로 다른 기술을 이어 쓰면 2/3연계 +10/20%",
                 (SDL_FRect){p.x+18,p.y+p.h-34,p.w-36,26},12,10,MUTED);
}
static void draw_journal(App *a) {
    Gfx *f = &a->gfx;
    Game *g = &a->game;
    SDL_FRect p = panel(a, "제1장 · 경상도 남부", "여정 기록", 440);
    float y = p.y + 65;
    char s[512];
    game_quest_text(g, s, sizeof s);
    gfx_text_box(f, s, (SDL_FRect){p.x + 16, y, p.w - 32, 38}, 13, 12, GOLD);
    y += 42;
    float available = p.h - 146, row_h = 28;
    int visible = (int)(available / row_h);
    if (visible < 1)
        visible = 1;
    int start = (int)a->journal_scroll;
    if (start < 0)
        start = 0;
    if (start > QUEST_COUNT - visible)
        start = QUEST_COUNT - visible;
    if (start < 0)
        start = 0;
    a->journal_scroll = (float)start;
    for (int i = start; i < QUEST_COUNT && i < start + visible; i++) {
        snprintf(s, sizeof s, "%s %02d  %s",
                 i < (int)g->quest    ? "✓"
                 : i == (int)g->quest ? "◇"
                                      : "·",
                 i + 1, QUESTS[i].title);
        gfx_text_box(f, s, (SDL_FRect){p.x + 20, y, p.w - 114, 25}, 14, 12,
                 i == (int)g->quest  ? GOLD
                 : i < (int)g->quest ? PAPER
                                     : MUTED);
        y += row_h;
    }
    gfx_text_box(f, "위·아래로 기록 이동 · 금빛 화살표는 목적지",
                 (SDL_FRect){p.x + 16, p.y + p.h - 25, p.w - 32, 20}, 11, 11, MUTED);
    if (start > 0)
        button(a, (SDL_FRect){p.x + p.w - 89, p.y + 125, 58, 44}, "↑", ACT_JOURNAL, -1, true, false);
    if (start + visible < QUEST_COUNT)
        button(a, (SDL_FRect){p.x + p.w - 89, p.y + p.h - 79, 58, 44}, "↓", ACT_JOURNAL, 1, true, false);
}
static SDL_FRect narrative_rect(App *a, const char *text) {
    float width = fminf(780, a->width - 20), size = a->width < 600 ? 16 : 17;
    float height = ceilf(gfx_text_height(&a->gfx, text, size, width - 30)) + 48;
    height = fminf(fmaxf(78, height), a->height - 88);
    return (SDL_FRect){(a->width - width) / 2, a->height - height - 10, width, height};
}
static void narrative_body(App *a, SDL_FRect p, const char *text) {
    gfx_text_box(&a->gfx, text, (SDL_FRect){p.x + 14, p.y + 39, p.w - 28, p.h - 47},
                 a->width < 600 ? 16 : 17, 14, PAPER);
}
static void draw_overlay(App *a) {
    Gfx *f = &a->gfx;
    Game *g = &a->game;
    float w = a->width, h = a->height;
    char s[160];
    if (g->phase == PH_TITLE) {
        gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){6, 21, 27, 178});
        gfx_center(f, "증강", w * .29f, h * .26f, 20, GOLD);
        gfx_heading(f, "팔도야담", (SDL_FRect){w * .06f, h * .33f, w * .46f, h * .22f}, fminf(90, h * .24f), PAPER);
        gfx_center(f, "사람이 짐승이 되는 밤.", w * .29f, h * .59f, 17, PAPER);
        gfx_center(f, "팔도에 남겨진 이야기를 따라.", w * .29f, h * .68f, 15, MUTED);
        float x = w * .58f, bw = fminf(260, w * .36f);
        button(a, (SDL_FRect){x, h * .35f, bw, 52}, "이야기 시작", ACT_START, 0, true, true);
        button(a, (SDL_FRect){x, h * .35f + 54, bw, 45}, "바로 마을로", ACT_SKIP, 0, true, false);
        gfx_center(f, "제1장 · 경상도 남부", x + bw / 2, h * .35f + 108, 13, MUTED);
        return;
    }
    if (g->phase == PH_PROLOGUE) {
        if (a->story_index == 4) {
            gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){5, 14, 20, 225});
            float top = fmaxf(12, (narrative_rect(a, STORY_TEXT[4]).y - 100) / 2);
            gfx_heading(f, "10년 뒤", (SDL_FRect){w / 2 - 140, top, 280, 58}, 50, PAPER);
            gfx_center(f, "경상도 남부 · 사라진 사람들", w / 2, top + 64, 15, GOLD);
        }
        SDL_FRect p = narrative_rect(a, STORY_TEXT[a->story_index]);
        surface(f, p, 15, INK);
        snprintf(s, sizeof s, "서장 %02d / 05", a->story_index + 1);
        gfx_text_box(f, s, (SDL_FRect){p.x + 14, p.y + 10, 77, 18}, 11, 11, MUTED);
        gfx_label(f, a->story_index == 4 && w < 600 ? "10년 뒤" : STORY_CAPS[a->story_index],
                  (SDL_FRect){p.x + 92, p.y + 4, p.w - 200, 28}, 14, GOLD);
        narrative_body(a, p, STORY_TEXT[a->story_index]);
        button(a, (SDL_FRect){p.x + p.w - 104, p.y, 96, 44},
               a->story_index == 4 ? "마을로" : "계속", ACT_NEXT, 0, true, true);
        button(a, (SDL_FRect){w - 104, 4, 96, 44}, "건너뛰기", ACT_SKIP, 0, true, false);
        return;
    }
    if (g->phase == PH_DIALOGUE) {
        Line line = a->dialogue[a->dialogue_index];
        SDL_FRect p = narrative_rect(a, line.text);
        surface(f, p, 15, INK);
        gfx_heading(f, line.speaker, (SDL_FRect){p.x + 10, p.y + 3, 160, 29}, 24, GOLD);
        narrative_body(a, p, line.text);
        button(a, (SDL_FRect){p.x + p.w - 104, p.y, 96, 44},
               a->dialogue_index == a->dialogue_count - 1 ? "확인" : "계속", ACT_NEXT, 0, true, true);
        return;
    }
    if (g->phase == PH_SHOP) {
        draw_shop(a);
        return;
    }
    if (g->phase == PH_BAG) {
        draw_bag(a);
        return;
    }
    if (g->phase == PH_TECHNIQUES) {
        draw_techniques(a);
        return;
    }
    if (g->phase == PH_JOURNAL) {
        draw_journal(a);
        return;
    }
    if (g->phase == PH_AUGMENT) {
        gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){7, 22, 26, 232});
        gfx_center(f, "마을을 구하고 얻은 깨달음", w / 2, 12, 13, GOLD);
        gfx_heading(f, "무엇을 새길 것인가", (SDL_FRect){w / 2 - 180, 28, 360, 40}, 33, PAPER);
        float cw = (fminf(w - 32, 840) - 24) / 3, x = (w - (cw * 3 + 24)) / 2, top = 76, ch = fminf(270, h - top - 12);
        for (int i = 0; i < 3; i++) {
            SDL_FRect r = {x + (cw + 12) * (float)i, top, cw, ch};
            surface(f, r, 14, (SDL_Color){25, 46, 44, 250});
            gfx_text(f, AUGMENTS[i].kind, r.x + 15, r.y + 10, 12, cw - 30, GOLD);
            gfx_heading(f, AUGMENTS[i].name, (SDL_FRect){r.x + 8, r.y + 30, cw - 16, 30}, 26, PAPER);
            static const char *compact[] = {
                "넓고 무거운 베기\n공격 범위 +18\n기본 피해 +4",
                "검 끝의 바람 칼날\n거리 120 안의 적\n바람 피해 6",
                "베며 앞으로 돌진\n재사용 4초\n스킬 피해 +6"};
            gfx_text_box(f, w < 740 || h < 360 ? compact[i] : AUGMENTS[i].description,
                         (SDL_FRect){r.x + 12, r.y + 65, cw - 24, ch - 119},
                         w < 740 ? 12 : 14, 11, MUTED);
            button(a, (SDL_FRect){r.x + 12, r.y + ch - 48, cw - 24, 44}, "새기기", ACT_CHOOSE, i + 1, true,
                   true);
        }
        return;
    }
    if (g->phase == PH_PAUSED || g->phase == PH_DEAD || g->phase == PH_COMPLETE) {
        gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){7, 22, 26, 237});
        const char *title = g->phase == PH_PAUSED ? "잠시 숨 고르기"
                            : g->phase == PH_DEAD ? "다시 칼을 쥐다"
                                                  : "길은 계속된다";
        gfx_heading(f, title, (SDL_FRect){w / 2 - 220, 24, 440, 50}, 42, PAPER);
        if (g->phase == PH_PAUSED) {
            gfx_center(f, g->training.active ? "공격 버튼 탭 / 연속 밀기 · Z 기본 공격"
                       : "끌기 / WASD 이동 · 공격 버튼 탭 또는 Z · 누르면 연속 공격", w / 2, 84, 16, MUTED);
            gfx_center(f, g->training.active ? "Q/R/G/T 무공 · K 방향별 배치"
                       : "공격 버튼 밀기: ↑ Q · → R · ↓ G · ← T · K 무공 배치", w / 2, 115, 15, MUTED);
            gfx_center(f, g->training.active ? "훈련 보상 없음 · F8 또는 아래 버튼으로 복귀"
                       : "I 가방 · J 여정 기록 · 1 탕약 · 2 지혈산 · F11 전체화면", w / 2, 146, 14, MUTED);
        } else if (g->phase == PH_DEAD) {
            gfx_center(f, "마을에서 회복하고 다시 떠납니다.", w / 2, 88, 18, MUTED);
            gfx_center(f, "조사한 흔적, 장비와 회수한 물품은 유지됩니다.", w / 2, 122, 16, MUTED);
        } else {
            gfx_center(f, "마을의 불빛을 뒤로하고, 장군은 전라도로 향한다.", w / 2, 86, 18, MUTED);
            gfx_center(f, g->augment > AUG_NONE ? AUGMENTS[g->augment - 1].name : "첫 여정 완료", w / 2, 124,
                       20, GOLD);
        }
        float top = fminf(h - 110, fmaxf(186, h * .57f)), bw = fminf(380, w - 56);
        button(a, (SDL_FRect){(w - bw) / 2, top, bw, 46},
               g->phase == PH_PAUSED ? "계속하기"
               : g->phase == PH_DEAD ? (g->replay ? "백정전 다시 도전" : "마을에서 다시 일어나기")
                                     : "선택한 증강으로 백정전 다시 하기",
               g->phase == PH_PAUSED ? ACT_CLOSE
               : g->phase == PH_DEAD ? ACT_RECOVER
                                     : ACT_REPLAY,
               0, true, true);
        button(a, (SDL_FRect){(w - bw) / 2, top + 57, bw, 42},
               g->training.active ? "훈련 종료 · 원래 여정으로" : "처음으로", ACT_TITLE, 0, true, false);
        return;
    }
}
static void resize(App *a) {
    float previous_width = a->width, previous_height = a->height;
    int ww, wh, ow, oh;
    SDL_GetWindowSize(a->window, &ww, &wh);
    SDL_GetRendererOutputSize(a->renderer, &ow, &oh);
    a->density = 1;
#ifdef __EMSCRIPTEN__
    a->density = (float)clampf((float)emscripten_get_device_pixel_ratio(), 1, 3);
#endif
#ifdef __ANDROID__
    float dpi;
    if (SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(a->window), &dpi, NULL, NULL) == 0 && dpi > 120)
        a->density = clampf(dpi / 160, 1, 4);
#endif
#ifdef __EMSCRIPTEN__
    /* Safari browser bars can change the CSS viewport without a window
     * resize event. Keep the SDL backing buffer at the canvas' real size
     * every frame, so CSS never stretches an old frame non-uniformly. */
    double css_w = 0, css_h = 0;
    if (emscripten_get_element_css_size("#canvas", &css_w, &css_h) == EMSCRIPTEN_RESULT_SUCCESS &&
        css_w > 0 && css_h > 0) {
        int desired_w = (int)lround(css_w * a->density);
        int desired_h = (int)lround(css_h * a->density);
        if (abs(ww - desired_w) > 1 || abs(wh - desired_h) > 1) {
            SDL_SetWindowSize(a->window, desired_w, desired_h);
            SDL_GetWindowSize(a->window, &ww, &wh);
            SDL_GetRendererOutputSize(a->renderer, &ow, &oh);
        }
    }
#endif
    a->width = (float)ww / a->density;
    a->height = (float)wh / a->density;
    if (fabsf(previous_width - a->width) > .5f || fabsf(previous_height - a->height) > .5f)
        stop_input(a);
    SDL_RenderSetScale(a->renderer, (float)ow / a->width, (float)oh / a->height);
}
static void draw(App *a) {
    resize(a);
    a->button_count = 0;
    Vec2 focus = a->game.phase == PH_TITLE || a->game.phase == PH_PROLOGUE ? HOME : a->game.hero.pos;
    if (a->game.training.active) {
        focus.x += a->width * .02f / WORLD_ZOOM;
        focus.y -= a->height * (a->width < 680 ? .30f : .12f) / WORLD_ZOOM;
    }
    float scale_x, scale_y;
    SDL_RenderGetScale(a->renderer, &scale_x, &scale_y);
    SDL_RenderSetScale(a->renderer, scale_x * WORLD_ZOOM, scale_y * WORLD_ZOOM);
    a->camera = world_camera(a->width / WORLD_ZOOM, a->height / WORLD_ZOOM, focus);
    gfx_world(&a->gfx, &a->game, a->width / WORLD_ZOOM, a->height / WORLD_ZOOM,
              a->camera, a->target_set, a->target);
    if (a->debug_collision) {
        gfx_collision(&a->gfx, a->camera);
        gfx_circle(&a->gfx, (Vec2){a->game.hero.pos.x - a->camera.x, a->game.hero.pos.y - a->camera.y},
                   HERO_RADIUS, GOLD);
    }
    SDL_RenderSetScale(a->renderer, scale_x, scale_y);
    if (a->game.phase == PH_PROLOGUE && a->story_index < 4) {
        const int pictures[4] = {0, 1, 1, 2};
        gfx_story(&a->gfx, pictures[a->story_index], a->width, a->height, a->story_elapsed, narrative_rect(a, STORY_TEXT[a->story_index]).h);
    }
    if (a->game.phase == PH_PLAY || a->game.phase == PH_DIALOGUE)
        draw_hud(a);
    draw_overlay(a);
    draw_notification(a);
    if (a->pointer_down) {
        gfx_circle(&a->gfx, a->pointer_start, 38, (SDL_Color){231, 214, 156, 150});
        gfx_sector(&a->gfx,
                   (Vec2){a->pointer_start.x + a->pointer_delta.x * 29,
                          a->pointer_start.y + a->pointer_delta.y * 29},
                   13, 0, PI, (SDL_Color){227, 193, 131, 150});
    }
    if (a->width <= a->height) {
        gfx_rect(&a->gfx, (SDL_FRect){0, 0, a->width, a->height}, (SDL_Color){10, 25, 33, 247});
        gfx_center(&a->gfx, "휴대폰을 가로로 돌려 주세요", a->width / 2, a->height / 2 - 25, 23, PAPER);
        gfx_center(&a->gfx, "회전 후 같은 여정에서 이어집니다.", a->width / 2, a->height / 2 + 20, 15, MUTED);
        a->button_count = 0;
    }
}
static bool inside(SDL_FRect r, Vec2 p) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}
static int combat_direction(Vec2 delta) {
    float x=fabsf(delta.x), y=fabsf(delta.y);
    if(hypotf(x,y)<32) return -1;
    if(x>y*1.15f) return delta.x>0 ? SWIPE_RIGHT : SWIPE_LEFT;
    if(y>x*1.15f) return delta.y>0 ? SWIPE_DOWN : SWIPE_UP;
    return -1;
}
static void combat_tick(App *a, float dt) {
    if(a->game.phase!=PH_PLAY || a->width<=a->height) return;
    if(a->attack_key_down) {
        a->target_set=false;
        game_attack(&a->game);
    }
    if(a->attack_pointer_down && !a->attack_swiping) {
        a->attack_hold+=clampf(dt,0,.05f);
        if(a->attack_hold>=.24f) {
            a->target_set=false;
            if(game_attack(&a->game)) a->attack_repeated=true;
        }
    }
}
static void pointer_down(App *a, Sint64 id, Vec2 p) {
    if(a->width<=a->height) return;
    if (a->secret_down && a->secret_id == id) return;
    if (!a->game.training.active && a->game.phase == PH_PLAY && !a->pointer_down &&
        !a->attack_pointer_down && !a->secret_down && inside((SDL_FRect){12, 8, 190, 22}, p)) {
        a->secret_down = true;
        a->secret_moved = false;
        a->secret_id = id;
        a->secret_start = p;
        a->secret_started = SDL_GetTicks64();
        return;
    }
    a->secret_taps = 0;
    for (int i = a->button_count - 1; i >= 0; i--) {
        Button b = a->buttons[i];
        if (inside(b.rect, p)) {
            if (b.enabled) {
                if(b.action==ACT_ATTACK && a->game.phase==PH_PLAY) {
                    if(!a->attack_pointer_down) {
                        a->attack_pointer_down=true; a->attack_pointer_id=id;
                        a->attack_start=a->attack_anchor=p; a->attack_hold=0;
                        a->attack_direction=-1;
                        a->attack_swiping=a->attack_repeated=false;
                        a->target_set=false;
                    }
                } else if (b.action == ACT_JOURNAL && a->game.phase == PH_JOURNAL && b.value != 0)
                    a->journal_scroll += (float)b.value;
                else dispatch(a, b.action, b.value);
            }
            return;
        }
    }
    if (a->game.training.active && (inside((SDL_FRect){12, 58, 148, 146}, p) ||
        inside((SDL_FRect){12, 8, 190, 44}, p))) return;
    if (a->game.phase != PH_PLAY || a->pointer_down ||
        (a->attack_pointer_down && a->attack_pointer_id==id)) return;
    a->pointer_down = true; a->pointer_id = id;
    a->pointer_start = p; a->pointer_delta = (Vec2){0, 0};
    a->pointer_moved = false; a->target_set = false;
}
static void pointer_move(App *a, Sint64 id, Vec2 p) {
    if (a->secret_down && a->secret_id == id) {
        if (distance(p, a->secret_start) > 8) a->secret_moved = true;
        return;
    }
    if(a->attack_pointer_down && a->attack_pointer_id==id) {
        if(a->game.phase!=PH_PLAY || a->width<=a->height) {
            stop_input(a);
            return;
        }
        if(distance(p,a->attack_start)>=32) a->attack_swiping=true;
        Vec2 delta={p.x-a->attack_anchor.x,p.y-a->attack_anchor.y};
        int direction=combat_direction(delta);
        if (direction < 0 && hypotf(delta.x, delta.y) >= 32)
            game_invalid_direction(&a->game);
        if(direction>=0 && direction!=a->attack_direction) {
            /* One request per new stroke, even if timing/cooldown rejects it.
             * The game's existing single-slot buffer owns all combat timing. */
            a->attack_swiping=true;
            a->attack_direction=direction;
            a->attack_anchor=p;
            dispatch(a,ACT_TECHNIQUE,a->game.swipe_bindings[direction]);
        } else {
            /* Follow forward progress, so reversals start at the stroke's tip.
             * Do not rebase on lateral jitter: a gradual turn must accumulate. */
            float forward= a->attack_direction==SWIPE_UP ? -delta.y
                : a->attack_direction==SWIPE_DOWN ? delta.y
                : a->attack_direction==SWIPE_LEFT ? -delta.x : delta.x;
            float side= a->attack_direction==SWIPE_UP || a->attack_direction==SWIPE_DOWN
                ? fabsf(delta.x) : fabsf(delta.y);
            if(a->attack_direction>=0 && forward>side*1.15f) a->attack_anchor=p;
        }
        return;
    }
    if (!a->pointer_down || a->pointer_id != id) return;
    float dx = p.x - a->pointer_start.x, dy = p.y - a->pointer_start.y, d = hypotf(dx, dy), m = fmaxf(34, d);
    a->pointer_delta = (Vec2){dx / m, dy / m};
    if (d > 8) a->pointer_moved = true;
}
static void pointer_up(App *a, Sint64 id, Vec2 p) {
    if (a->secret_down && a->secret_id == id) {
        pointer_move(a, id, p);
        Uint64 now = SDL_GetTicks64();
        a->secret_down = false;
        if (a->game.phase != PH_PLAY || a->secret_moved || now - a->secret_started > 300) {
            a->secret_taps = 0;
            return;
        }
        a->secret_taps = a->secret_taps > 0 && now - a->secret_last <= 400 ? a->secret_taps + 1 : 1;
        a->secret_last = now;
        if (a->secret_taps == 7) toggle_training(a);
        return;
    }
    if(a->attack_pointer_down && a->attack_pointer_id==id) {
        /* Some platforms coalesce the final motion into the release event. */
        pointer_move(a,id,p);
        if(a->attack_pointer_down && !a->attack_swiping && !a->attack_repeated)
            dispatch(a,ACT_ATTACK,0);
        a->attack_pointer_down=false; a->attack_hold=0;
        a->attack_swiping=a->attack_repeated=false;
        a->attack_direction=-1;
        a->attack_start=a->attack_anchor=(Vec2){0,0};
        return;
    }
    if (!a->pointer_down || a->pointer_id != id) return;
    if (!a->pointer_moved && a->game.phase == PH_PLAY) {
        a->target = world_from_screen(p, a->camera);
        a->target.x = clampf(a->target.x, 28, WORLD_W - 28);
        a->target.y = clampf(a->target.y, 28, WORLD_H - 28);
        a->target_set = world_route(a->game.hero.pos, a->target, HERO_RADIUS, &a->route);
        if (a->target_set) a->target = a->route.points[a->route.count - 1];
        else game_toast(&a->game, "막힌 지형 · 다른 길을 선택해 주세요");
    }
    a->pointer_down = false; a->pointer_delta = (Vec2){0, 0};
}
static void handle_events(App *a) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            a->running = false;
            continue;
        }
        if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST || e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                if (a->game.phase == PH_PLAY)
                    a->game.phase = PH_PAUSED;
                stop_input(a);
            }
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                resize(a);
                stop_input(a);
            }
            continue;
        }
        if (e.type == SDL_APP_WILLENTERBACKGROUND) {
            if (a->game.phase == PH_PLAY)
                a->game.phase = PH_PAUSED;
            stop_input(a);
            continue;
        }
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            a->touch_input = false;
            SDL_Keycode key = e.key.keysym.sym;
            if (key == SDLK_ESCAPE || key == SDLK_AC_BACK)
                dispatch(a, ACT_PAUSE, 0);
            else if (key == SDLK_F11)
                dispatch(a, ACT_FULLSCREEN, 0);
            else if (key == SDLK_F3)
                a->debug_collision = !a->debug_collision;
            else if (key == SDLK_F8)
                toggle_training(a);
            else if (key == SDLK_SPACE || key == SDLK_RETURN) {
                if (a->game.phase == PH_TITLE)
                    dispatch(a, ACT_START, 0);
                else if (a->game.phase == PH_DIALOGUE || a->game.phase == PH_PROLOGUE)
                    dispatch(a, ACT_NEXT, 0);
                else if (a->game.phase == PH_PLAY)
                    dispatch(a, ACT_INTERACT, 0);
            } else if (key == SDLK_q)
                dispatch(a, ACT_SKILL, 0);
            else if (key == SDLK_r)
                dispatch(a, ACT_TECHNIQUE, a->game.swipe_bindings[SWIPE_RIGHT]);
            else if (key == SDLK_g)
                dispatch(a, ACT_TECHNIQUE, a->game.swipe_bindings[SWIPE_DOWN]);
            else if (key == SDLK_t)
                dispatch(a, ACT_TECHNIQUE, a->game.swipe_bindings[SWIPE_LEFT]);
            else if (key == SDLK_k)
                dispatch(a, ACT_TECHNIQUES, 0);
            else if (key == SDLK_z && a->game.phase == PH_PLAY) {
                a->attack_key_down = true;
                dispatch(a, ACT_ATTACK, 0);
            } else if (key == SDLK_f)
                dispatch(a, ACT_PICKUP, 0);
            else if (key == SDLK_e)
                dispatch(a, ACT_INTERACT, 0);
            else if (key == SDLK_i)
                dispatch(a, ACT_BAG, 0);
            else if (key == SDLK_j)
                dispatch(a, ACT_JOURNAL, 0);
            else if (key == SDLK_1)
                dispatch(a, ACT_USE, ITEM_TONIC);
            else if (key == SDLK_2)
                dispatch(a, ACT_USE, ITEM_BANDAGE);
            if (a->game.phase == PH_JOURNAL) {
                if (key == SDLK_DOWN)
                    a->journal_scroll++;
                else if (key == SDLK_UP)
                    a->journal_scroll--;
            }
        }
        if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_z) a->attack_key_down = false;
        if (e.type == SDL_MOUSEWHEEL && a->game.phase == PH_JOURNAL)
            a->journal_scroll -= (float)e.wheel.y;
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.which != SDL_TOUCH_MOUSEID &&
            e.button.button == SDL_BUTTON_LEFT)
            pointer_down(a, -1, (Vec2){(float)e.button.x / a->density, (float)e.button.y / a->density});
        if (e.type == SDL_MOUSEMOTION && e.motion.which != SDL_TOUCH_MOUSEID)
            pointer_move(a, -1, (Vec2){(float)e.motion.x / a->density, (float)e.motion.y / a->density});
        if (e.type == SDL_MOUSEBUTTONUP && e.button.which != SDL_TOUCH_MOUSEID &&
            e.button.button == SDL_BUTTON_LEFT)
            pointer_up(a, -1, (Vec2){(float)e.button.x / a->density, (float)e.button.y / a->density});
        if (e.type == SDL_FINGERDOWN) a->touch_input = true;
        if (e.type == SDL_FINGERDOWN)
            pointer_down(a, e.tfinger.fingerId, (Vec2){e.tfinger.x * a->width, e.tfinger.y * a->height});
        if (e.type == SDL_FINGERMOTION)
            pointer_move(a, e.tfinger.fingerId, (Vec2){e.tfinger.x * a->width, e.tfinger.y * a->height});
        if (e.type == SDL_FINGERUP)
            pointer_up(a, e.tfinger.fingerId, (Vec2){e.tfinger.x * a->width, e.tfinger.y * a->height});
    }
}
static Vec2 input(App *a, float dt) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if (a->movement_blocked) {
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] ||
            keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
            return (Vec2){0, 0};
        a->movement_blocked = false;
    }
    Vec2 v = a->pointer_delta;
    v.x += (float)(keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) -
           (float)(keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]);
    v.y += (float)(keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) -
           (float)(keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]);
    if (hypotf(v.x, v.y) > .01f) {
        a->target_set = false;
        return v;
    }
    if (a->target_set) {
        Vec2 direction = world_follow(&a->route, a->game.hero.pos);
        if (a->route.cursor >= a->route.count) {
            a->target_set = false;
            return (Vec2){0, 0};
        }
        float d = distance(a->route.points[a->route.cursor], a->game.hero.pos);
        float strength = fminf(1, d / (155 * fmaxf(.001f, clampf(dt, 0, .05f))));
        return (Vec2){direction.x * strength, direction.y * strength};
    }
    return (Vec2){0, 0};
}
#ifndef __EMSCRIPTEN__
static bool save_frame(App *a, const char *path) {
    int w, h;
    SDL_GetRendererOutputSize(a->renderer, &w, &h);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s)
        return false;
    bool ok = SDL_RenderReadPixels(a->renderer, NULL, s->format->format, s->pixels, s->pitch) == 0 &&
              SDL_SaveBMP(s, path) == 0;
    SDL_FreeSurface(s);
    return ok;
}
#endif
static void smoke_scene(App *a, const char *scene) {
    if (!strcmp(scene, "title"))
        return;
    game_start(&a->game);
    a->game.toast_time = 0;
    if (!strncmp(scene, "prologue", 8)) {
        a->game.phase = PH_PROLOGUE;
        a->story_index = (int)clampf((float)atoi(scene + 8) - 1, 0, 4);
        a->story_elapsed = 1;
    } else if (!strcmp(scene, "training")) {
        toggle_training(a);
        a->touch_input = true;
    } else if (!strcmp(scene, "collision")) {
        a->debug_collision = true;
    } else if (!strcmp(scene, "canopy")) {
        a->game.hero.pos = (Vec2){779 * 1.5f, 867 * 1.5f};
    } else if (!strcmp(scene, "roof")) {
        a->game.hero.pos = (Vec2){295 * 1.5f, 136 * 1.5f};
    } else if (!strcmp(scene, "shop")) {
        a->game.hero.pos = NPCS[SMITH].pos;
        a->game.coins = 137;
        game_open_shop(&a->game, SMITH);
    } else if (!strncmp(scene, "bag", 3)) {
        a->game.hero.hp = 45;
        a->game.owned_sword = a->game.sword = true;
        a->game.owned_armor = a->game.armor = true;
        a->game.herbs = 2; a->bag_selected = ITEM_SWORD;
        a->bag_tab = !strcmp(scene,"bag-consumables") ? 1 : !strcmp(scene,"bag-quest") ? 3 : 0;
        a->game.phase = PH_BAG;
    } else if (!strcmp(scene,"techniques")) {
        a->game.phase = PH_TECHNIQUES;
    } else if (!strncmp(scene,"skill",5)) {
        a->game.hero.skilling = true; a->game.hero.skill_hit = true;
        a->game.hero.technique = (Technique)(int)clampf((float)atoi(scene+5)-1,0,TECH_COUNT-1);
        a->game.hero.skill_elapsed = .30f; a->game.hero.skill_angle = 0;
        a->game.hero.impact_start = a->game.hero.impact_origin = a->game.hero.pos;
        a->game.hero.attack_face = a->game.hero.face = 1;
    } else if (!strcmp(scene, "dialogue")) {
        say(a, MEET_LINES, 3, DONE_ACCEPT);
    } else if (!strcmp(scene, "augment")) {
        a->game.phase = PH_AUGMENT;
    } else if (!strcmp(scene, "journal")) {
        a->game.phase = PH_JOURNAL;
        a->game.quest = Q_RECOVER;
    } else if (!strcmp(scene, "boss")) {
        a->game.hero.pos = (Vec2){BOSS_HOME.x - 90, BOSS_HOME.y};
        a->game.quest = Q_HUNT;
        game_begin_boss(&a->game);
        a->game.boss.state = AI_WINDUP;
        a->game.boss.angle = PI;
        a->game.boss.timer = .4f;
        a->game.toast_time = 0;
    } else if (!strcmp(scene, "recover")) {
        a->game.traces[0] = a->game.traces[1] = true;
        a->game.quest = Q_RECOVER;
        a->game.hero.pos = (Vec2){a->game.boars[0].pos.x - 100, a->game.boars[0].pos.y};
    } else if (!strncmp(scene, "attack", 6)) {
        a->game.traces[0] = a->game.traces[1] = true;
        a->game.quest = Q_RECOVER;
        a->game.hero.pos = (Vec2){a->game.boars[0].pos.x - 45, a->game.boars[0].pos.y};
        int pose = (int)clampf((float)atoi(scene + 6) - 1, 0, HERO_ATTACK_FRAMES - 1);
        a->game.hero.attacking = true;
        a->game.hero.attack_face = 1;
        a->game.hero.attack_elapsed = (pose + .1f) * HERO_ATTACK_SECONDS / HERO_ATTACK_FRAMES;
        a->game.attack_cooldown = 1;
    } else if (!strcmp(scene, "loot")) {
        a->game.traces[0] = a->game.traces[1] = true;
        a->game.quest = Q_RECOVER;
        a->game.hero.pos = a->game.boars[0].pos;
        a->game.boars[0].hp = 0;
        a->game.drops[0] = (Drop){.pos = {a->game.hero.pos.x + 100, a->game.hero.pos.y + 12}, .exists = true};
        a->touch_input = true;
        game_toast(&a->game, "가까운 약재는 자동으로 회수합니다");
        a->game.toast_time = 2;
    } else if (!strcmp(scene, "notification")) {
        game_toast(&a->game, "여정 시작 · 마을 이장과 대화");
        a->game.toast_time = 2;
    } else if (!strcmp(scene, "npc")) {
        a->game.hero.pos = (Vec2){NPCS[ELDER].pos.x + 25, NPCS[ELDER].pos.y + 25};
    }
}
/* Native and browser builds execute this exact same gameplay/rendering step. */
static void frame(App *a) {
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)((double)(now - a->previous_frame) / a->timer_frequency);
    a->previous_frame = now;
    if (!a->headless)
        handle_events(a);
    if (a->width > a->height) {
        if (a->game.phase == PH_PROLOGUE)
            a->story_elapsed += clampf(dt, 0, .05f);
        combat_tick(a, dt);
        game_tick(&a->game, dt, input(a, dt));
    }
    unsigned notices = a->game.notices;
    a->game.notices = 0;
    if (notices & NOTICE_BOSS)
        say(a, BOSS_LINES, 2, DONE_BOSS);
    if (notices & NOTICE_DEATH)
        stop_input(a);
    if (notices & NOTICE_VICTORY) {
        stop_input(a);
        if (a->game.replay)
            a->game.phase = PH_COMPLETE;
        else
            game_toast(&a->game, "백정 제압 +70문 · 이장에게 보고");
    }
    draw(a);
}
static void destroy_app(App *a) {
    gfx_free(&a->gfx);
    SDL_DestroyRenderer(a->renderer);
    SDL_DestroyWindow(a->window);
    free(a);
    SDL_Quit();
}
#ifdef __EMSCRIPTEN__
static EM_BOOL browser_resize(int event, const EmscriptenUiEvent *info, void *user) {
    (void)event;
    (void)info;
    App *a = user;
    /* resize() also reconciles CSS dimensions after browser chrome changes. */
    resize(a);
    stop_input(a);
    return EM_FALSE;
}
static void browser_frame(void *user) {
    App *a = user;
    if (!a->running) {
        emscripten_cancel_main_loop();
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_FALSE, NULL);
        destroy_app(a);
        return;
    }
    frame(a);
    SDL_RenderPresent(a->renderer);
}
#endif

int main(int argc, char **argv) {
    const char *asset_root = NULL, *smoke = NULL, *scene = "village";
    int width = 1100, height = 650;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--assets") && i + 1 < argc)
            asset_root = argv[++i];
        else if (!strcmp(argv[i], "--smoke") && i + 1 < argc)
            smoke = argv[++i];
        else if (!strcmp(argv[i], "--scene") && i + 1 < argc)
            scene = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &width, &height) != 2 || width < 320 || height < 240 ||
                width > 4096 || height > 4096) {
                fprintf(stderr, "Invalid --size (320x240 to 4096x4096)\n");
                return 2;
            }
        } else if (!strcmp(argv[i], "--help")) {
            puts("paldo-yadam [--assets PATH] [--smoke frame.bmp] [--scene "
                 "title|village|shop|bag|dialogue|augment|journal|boss|recover|prologue1..5|roof|canopy|"
                 "collision|loot|notification|npc|training] "
                 "[--size 844x390]");
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 2;
        }
    }
    if (smoke)
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    App *a = calloc(1, sizeof *a);
    if (!a) {
        SDL_Quit();
        return 1;
    }
    a->headless = smoke != NULL;
    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __ANDROID__
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
    if (smoke)
        flags |= SDL_WINDOW_HIDDEN;
    a->window = SDL_CreateWindow("증강: 팔도야담", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                                 height, flags);
    if (a->window)
        a->renderer = SDL_CreateRenderer(a->window, -1,
                                         smoke ? SDL_RENDERER_SOFTWARE
                                               : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (a->window && !a->renderer)
        a->renderer = SDL_CreateRenderer(a->window, -1, SDL_RENDERER_SOFTWARE);
    if (!a->window || !a->renderer) {
        fprintf(stderr, "Window/renderer: %s\n", SDL_GetError());
        if (a->window)
            SDL_DestroyWindow(a->window);
        free(a);
        SDL_Quit();
        return 1;
    }
    bool loaded = false;
#if defined(__EMSCRIPTEN__)
    loaded = gfx_load(&a->gfx, a->renderer, asset_root ? asset_root : "/assets");
#elif defined(__ANDROID__)
    loaded = gfx_load(&a->gfx, a->renderer, asset_root ? asset_root : "");
#else
    if (asset_root)
        loaded = gfx_load(&a->gfx, a->renderer, asset_root);
    else {
        char *base = SDL_GetBasePath();
        if (base) {
            char path[2048];
            snprintf(path, sizeof path, "%sassets", base);
            loaded = gfx_load(&a->gfx, a->renderer, path);
            SDL_free(base);
        }
        if (!loaded)
            loaded = gfx_load(&a->gfx, a->renderer, "assets");
    }
#endif
    if (!loaded) {
        fprintf(stderr, "Game assets missing. Run from the project directory or use --assets PATH.\n");
        SDL_DestroyRenderer(a->renderer);
        SDL_DestroyWindow(a->window);
        free(a);
        SDL_Quit();
        return 1;
    }
    game_reset(&a->game);
    a->running = true;
    resize(a);
    if (smoke)
        smoke_scene(a, scene);
    a->previous_frame = SDL_GetPerformanceCounter();
    a->timer_frequency = (double)SDL_GetPerformanceFrequency();
#ifdef __EMSCRIPTEN__
    #ifdef PALDO_WEB_TESTING
    web_test_app = a;
    #endif
    browser_resize(0, NULL, a);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, a, EM_FALSE, browser_resize);
    draw(a);
    SDL_RenderPresent(a->renderer);
    web_ready();
    emscripten_set_main_loop_arg(browser_frame, a, 0, 1);
    return 0;
#else
    int status = 0;
    draw(a);
    while (a->running) {
        frame(a);
        if (smoke) {
            status = save_frame(a, smoke) ? 0 : 1;
            printf("Native render: %s, %.0fx%.0f, %d Korean/Latin glyphs, %s\n", scene, a->width, a->height,
                   a->gfx.fonts[0].glyph_count + a->gfx.fonts[1].glyph_count, status ? "FAILED" : "OK");
            a->running = false;
        }
        SDL_RenderPresent(a->renderer);
        if (!smoke)
            SDL_Delay(1);
    }
    destroy_app(a);
    return status;
#endif
}
