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

#define BUTTON_MAX 36
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
    ACT_CHOOSE
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
    bool running, target_set, pointer_down, pointer_moved, headless, debug_collision;
    Sint64 pointer_id;
    Vec2 pointer_start, pointer_delta, target;
    float journal_scroll, story_elapsed;
    Route route;
    Uint64 previous_frame;
    double timer_frequency;
} App;
#if defined(__EMSCRIPTEN__) && defined(PALDO_WEB_TESTING)
static App *web_test_app;
EMSCRIPTEN_KEEPALIVE const char *paldo_test_state(void) {
    static char state[512];
    App *a = web_test_app;
    if (!a)
        return "{}";
    snprintf(state, sizeof state,
             "{\"phase\":%d,\"quest\":%d,\"story\":%d,\"x\":%.3f,\"y\":%.3f,"
             "\"width\":%.3f,\"height\":%.3f,\"density\":%.3f,\"clear\":%s}",
             a->game.phase, a->game.quest, a->story_index, a->game.hero.pos.x, a->game.hero.pos.y,
             a->width, a->height, a->density,
             world_clear(a->game.hero.pos, HERO_RADIUS) ? "true" : "false");
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
    a->pointer_down = false;
    a->pointer_delta = (Vec2){0, 0};
    a->target_set = false;
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
    if (n.kind == NEAR_DROP) {
        game_collect(g, n.index);
        return;
    }
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
    case ACT_SKILL:
        if (game_skill(g) && a->target_set)
            a->target_set = world_route(g->hero.pos, a->target, HERO_RADIUS, &a->route);
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
        if (g->phase == PH_PLAY) {
            g->phase = PH_BAG;
            stop_input(a);
        } else if (g->phase == PH_BAG)
            game_close_panel(g);
        break;
    case ACT_JOURNAL:
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
        if (game_use(g, (Item)value) && g->phase == PH_BAG)
            game_close_panel(g);
        break;
    case ACT_CHOOSE:
        game_choose(g, (Augment)value);
        break;
    }
}
static void button(App *a, SDL_FRect r, const char *label, Action action, int value, bool enabled,
                   bool primary) {
    if (a->button_count < BUTTON_MAX)
        a->buttons[a->button_count++] = (Button){r, action, value, enabled};
    SDL_Color fill = primary ? (SDL_Color){212, 187, 130, 250} : INK;
    if (!enabled)
        fill = (SDL_Color){38, 57, 49, 238};
    gfx_rect(&a->gfx, r, fill);
    gfx_border(&a->gfx, r, enabled ? GOLD : (SDL_Color){92, 107, 83, 255});
    float size = 15;
    while (size > 10 && gfx_measure(&a->gfx, label, size) > r.w - 12)
        size -= 1;
    gfx_center(&a->gfx, label, r.x + r.w / 2, r.y + (r.h - size) / 2 - 1, size,
               enabled ? (primary ? INK : PAPER) : MUTED);
}
static SDL_FRect panel(App *a, const char *kicker, const char *title, float desired_h) {
    float width = fminf(740, a->width - 44), height = fminf(desired_h, a->height - 30);
    SDL_FRect p = {(a->width - width) / 2, (a->height - height) / 2, width, height};
    gfx_rect(&a->gfx, (SDL_FRect){0, 0, a->width, a->height}, (SDL_Color){4, 17, 20, 150});
    gfx_rect(&a->gfx, p, INK);
    gfx_border(&a->gfx, p, GOLD);
    gfx_text(&a->gfx, kicker, p.x + 22, p.y + 14, 13, p.w - 150, GOLD);
    gfx_text(&a->gfx, title, p.x + 22, p.y + 34, 27, p.w - 150, PAPER);
    button(a, (SDL_FRect){p.x + p.w - 98, p.y + 17, 76, 40}, "닫기", ACT_CLOSE, 0, true, false);
    return p;
}
static void draw_hud(App *a) {
    Gfx *f = &a->gfx;
    Game *g = &a->game;
    float w = a->width, h = a->height;
    char text[512];
    gfx_rect(f, (SDL_FRect){14, 12, 270, 65}, INK);
    gfx_border(f, (SDL_FRect){14, 12, 270, 65}, (SDL_Color){104, 118, 90, 180});
    gfx_text(f, "제1장 · 경상도 남부", 26, 21, 12, 0, GOLD);
    gfx_text(f, "사라진 사람들", 160, 19, 16, 0, PAPER);
    gfx_rect(f, (SDL_FRect){57, 56, 140, 6}, (SDL_Color){25, 18, 19, 255});
    gfx_rect(f, (SDL_FRect){57, 56, 140 * (float)g->hero.hp / 100, 6}, RED);
    gfx_text(f, "체력", 26, 48, 12, 0, MUTED);
    snprintf(text, sizeof text, "%d / 100", g->hero.hp);
    gfx_text(f, text, 205, 48, 12, 0, MUTED);
    if (g->phase == PH_PLAY) {
        button(a, (SDL_FRect){w - 55, 12, 41, 41}, "정지", ACT_PAUSE, 0, true, false);
#ifndef __ANDROID__
        button(a, (SDL_FRect){w - 104, 12, 41, 41}, "화면", ACT_FULLSCREEN, 0, true, false);
#endif
        snprintf(text, sizeof text, "가방 · %d문", g->coins);
        button(a, (SDL_FRect){w - 237, 12, 125, 41}, text, ACT_BAG, 0, true, false);
        game_quest_text(g, text, sizeof text);
        button(a, (SDL_FRect){14, 87, fminf(w * .56f, 490), 39}, text, ACT_JOURNAL, 0, true, false);
        const char *weapon = g->augment == AUG_STEEL  ? "강철 대검"
                             : g->augment == AUG_WIND ? "환도 · 바람의 깃"
                             : g->sword               ? "단련한 환도"
                                                      : "환도";
        gfx_rect(f, (SDL_FRect){14, h - 55, w < 800 ? 136 : 245, 40}, INK);
        gfx_text(f, weapon, 25, h - 46, 15, 230, GOLD);
        if (w >= 800)
            gfx_text(f, "끌어 이동 · 가까운 적 자동 공격", 25, h - 27, 11, 230, MUTED);
        snprintf(text, sizeof text, "탕약 %d", g->tonics);
        button(a, (SDL_FRect){w - 209, h - 92, 82, 35}, text, ACT_USE, ITEM_TONIC,
               game_can_use(g, ITEM_TONIC), false);
        snprintf(text, sizeof text, "지혈산 %d", g->bandages);
        button(a, (SDL_FRect){w - 209, h - 51, 82, 35}, text, ACT_USE, ITEM_BANDAGE,
               game_can_use(g, ITEM_BANDAGE), false);
        snprintf(text, sizeof text,
                 g->cooldown > 0          ? "%.1f초"
                 : g->augment == AUG_FLOW ? "파고들기"
                                          : "갈라베기",
                 g->cooldown);
        button(a, (SDL_FRect){w - 118, h - 92, 104, 76}, text, ACT_SKILL, 0, g->cooldown <= 0, false);
        Nearby near = game_nearby(g);
        if (near.kind != NEAR_NONE) {
            if (near.kind == NEAR_DROP)
                snprintf(text, sizeof text, "약재 꾸러미 줍기");
            else if (near.kind == NEAR_TRACE)
                snprintf(text, sizeof text, "%s 조사", TRACES[near.index].name);
            else if (near.index == HERBALIST && g->quest == Q_DELIVER)
                snprintf(text, sizeof text, "약재 꾸러미 전달");
            else if (near.index >= SMITH && near.index <= INNKEEPER)
                snprintf(text, sizeof text, "%s 이용", NPCS[near.index].role);
            else
                snprintf(text, sizeof text, "%s와 대화", NPCS[near.index].name);
            float bw = fminf(228, w * .31f);
            button(a, (SDL_FRect){w * .48f - bw / 2, h - 60, bw, 44}, text, ACT_INTERACT, 0, true, false);
        }
    }
    if (g->quest == Q_COMBAT) {
        float bw = fminf(270, w * .32f);
        SDL_FRect r = {w - bw - 14, 66, bw, 48};
        gfx_rect(f, r, INK);
        gfx_text(f, "변이한 백정", r.x + 12, r.y + 7, 14, 0, PAPER);
        snprintf(text, sizeof text, "%d", g->boss.hp);
        gfx_text(f, text, r.x + bw - 44, r.y + 7, 13, 0, MUTED);
        gfx_rect(f, (SDL_FRect){r.x + 12, r.y + 34, bw - 24, 5}, (SDL_Color){57, 38, 35, 255});
        gfx_rect(f, (SDL_FRect){r.x + 12, r.y + 34, (bw - 24) * (float)g->boss.hp / 240, 5}, RED);
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
                       : g->shop == HERBALIST
                           ? "탕약과 지혈산을 챙기세요. 숲에서는 상처를 돌볼 틈이 적으니까요."
                           : "따뜻한 국밥 한 그릇 드시고 기운 차리세요.";
    SDL_FRect p = panel(a, NPCS[g->shop].name, NPCS[g->shop].role, 365);
    char s[256];
    gfx_text(f, line, p.x + 22, p.y + 76, 14, p.w - 44, MUTED);
    snprintf(s, sizeof s, "소지금 %d문", g->coins);
    gfx_text(f, s, p.x + 22, p.y + 107, 14, 0, GOLD);
    int row = 0;
    float row_h = fminf(79, (p.h - 153) / 2);
    for (int i = 0; i < ITEM_COUNT; i++)
        if ((int)ITEMS[i].shop == g->shop) {
            float y = p.y + 135 + (row_h + 8) * (float)row++;
            gfx_rect(f, (SDL_FRect){p.x + 22, y, p.w - 44, row_h}, (SDL_Color){32, 57, 49, 220});
            gfx_text(f, ITEMS[i].name, p.x + 34, y + 9, 18, p.w - 220, PAPER);
            gfx_text(f, ITEMS[i].description, p.x + 34, y + 34, 12, p.w - 225, MUTED);
            const char *error = game_purchase_error(g, (Item)i);
            if (error)
                snprintf(s, sizeof s, "%s", error);
            else
                snprintf(s, sizeof s, "%d문 · %s", ITEMS[i].price, i == ITEM_MEAL ? "쉬기" : "구입");
            button(a, (SDL_FRect){p.x + p.w - 167, y + (row_h - 42) / 2, 132, 42}, s, ACT_BUY, i,
                   error == NULL, true);
        }
}
static void draw_bag(App *a) {
    Gfx *f = &a->gfx;
    Game *g = &a->game;
    SDL_FRect p = panel(a, "여정의 채비", "가방", 365);
    char s[256];
    snprintf(s, sizeof s, "소지금 %d문 · 체력 %d/100", g->coins, g->hero.hp);
    gfx_text(f, s, p.x + 22, p.y + 77, 14, 0, GOLD);
    snprintf(s, sizeof s, "%s   /   %s", g->sword ? "단련한 환도 · 피해 +5" : "환도",
             g->armor ? "누비 갑옷 · 피해 -3" : "군복");
    gfx_text(f, s, p.x + 22, p.y + 103, 15, p.w - 44, PAPER);
    float row_h = fminf(74, (p.h - 178) / 2);
    for (int i = 0; i < 2; i++) {
        Item item = i == 0 ? ITEM_TONIC : ITEM_BANDAGE;
        float y = p.y + 132 + (row_h + 8) * (float)i;
        gfx_rect(f, (SDL_FRect){p.x + 22, y, p.w - 44, row_h}, (SDL_Color){32, 57, 49, 220});
        snprintf(s, sizeof s, "%s × %d", ITEMS[item].name, i == 0 ? g->tonics : g->bandages);
        gfx_text(f, s, p.x + 34, y + 8, 18, p.w - 210, PAPER);
        gfx_text(f, i == 0 ? "체력 45 회복" : "출혈 즉시 해제", p.x + 34, y + 32, 13, p.w - 210, MUTED);
        button(a, (SDL_FRect){p.x + p.w - 147, y + (row_h - 40) / 2, 112, 40}, "사용", ACT_USE, item,
               game_can_use(g, item), true);
    }
    snprintf(s, sizeof s, "의뢰 물품 · 약재 꾸러미 %d/3%s", g->herbs,
             g->supplies_delivered ? " · 약방에 전달 완료" : "");
    gfx_text(f, s, p.x + 22, p.y + p.h - 26, 12, p.w - 44, MUTED);
}
static void draw_journal(App *a) {
    Gfx *f = &a->gfx;
    Game *g = &a->game;
    SDL_FRect p = panel(a, "제1장 · 경상도 남부", "여정 기록", 620);
    float y = p.y + 78;
    char s[512];
    game_quest_text(g, s, sizeof s);
    gfx_text(f, s, p.x + 22, y, 15, p.w - 44, GOLD);
    y += 42;
    float available = p.h - 152, row_h = 31;
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
        gfx_text(f, s, p.x + 28, y, 16, p.w - 120,
                 i == (int)g->quest  ? GOLD
                 : i < (int)g->quest ? PAPER
                                     : MUTED);
        y += row_h;
    }
    gfx_text(f, "휠 / 위·아래로 기록 이동 · 금빛 화살표가 목적지를 가리킵니다.", p.x + 22, p.y + p.h - 26, 12,
             p.w - 44, MUTED);
    if (start > 0)
        button(a, (SDL_FRect){p.x + p.w - 89, p.y + 127, 58, 38}, "↑", ACT_JOURNAL, -1, true, false);
    if (start + visible < QUEST_COUNT)
        button(a, (SDL_FRect){p.x + p.w - 89, p.y + p.h - 79, 58, 38}, "↓", ACT_JOURNAL, 1, true, false);
}
static void draw_overlay(App *a) {
    Gfx *f = &a->gfx;
    Game *g = &a->game;
    float w = a->width, h = a->height;
    char s[160];
    if (g->phase == PH_TITLE) {
        gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){6, 21, 27, 178});
        gfx_center(f, "증강", w * .29f, h * .26f, 20, GOLD);
        gfx_center(f, "팔도야담", w * .29f, h * .35f, fminf(57, h * .16f), PAPER);
        gfx_center(f, "사람이 짐승이 되는 밤.", w * .29f, h * .59f, 17, PAPER);
        gfx_center(f, "팔도에 남겨진 이야기를 따라.", w * .29f, h * .68f, 15, MUTED);
        float x = w * .58f, bw = fminf(320, w * .36f);
        button(a, (SDL_FRect){x, h * .35f, bw, 52}, "이야기 시작", ACT_START, 0, true, true);
        button(a, (SDL_FRect){x, h * .35f + 65, bw, 45}, "바로 마을로", ACT_SKIP, 0, true, false);
        gfx_center(f, "제1장 · 경상도 남부", x + bw / 2, h * .35f + 128, 13, MUTED);
        return;
    }
    if (g->phase == PH_PROLOGUE) {
        if (a->story_index == 4) {
            gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){5, 14, 20, 225});
            gfx_center(f, "10년 뒤", w / 2, h * .24f, 42, PAPER);
            gfx_center(f, "경상도 남부 · 사라진 사람들", w / 2, h * .24f + 64, 17, GOLD);
        }
        float y = h - 120;
        SDL_FRect panel = {16, y, w - 32, 108};
        gfx_rect(f, panel, INK);
        gfx_border(f, panel, (SDL_Color){143, 132, 94, 220});
        snprintf(s, sizeof s, "서장 %02d / 05", a->story_index + 1);
        gfx_text(f, s, 32, y + 15, 13, 0, MUTED);
        gfx_text(f, STORY_CAPS[a->story_index], 138, y + 12, 17, w - 345, GOLD);
        gfx_text(f, STORY_TEXT[a->story_index], 32, y + 49, 17, w - 64, PAPER);
        button(a, (SDL_FRect){w - 170, y + 5, 144, 40}, a->story_index == 4 ? "마을에 들어서기" : "계속",
               ACT_NEXT, 0, true, true);
        button(a, (SDL_FRect){w - 145, 12, 129, 40}, "건너뛰기", ACT_SKIP, 0, true, false);
        return;
    }
    if (g->phase == PH_DIALOGUE) {
        float pw = fminf(960, w - 40), ph = fminf(180, h * .55f);
        SDL_FRect p = {(w - pw) / 2, h - ph - 17, pw, ph};
        gfx_rect(f, p, INK);
        gfx_border(f, p, GOLD);
        Line line = a->dialogue[a->dialogue_index];
        gfx_text(f, line.speaker, p.x + 22, p.y + 15, 16, p.w - 44, GOLD);
        gfx_text(f, line.text, p.x + 22, p.y + 48, 19, p.w - 178, PAPER);
        button(a, (SDL_FRect){p.x + p.w - 130, p.y + p.h - 64, 108, 44},
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
    if (g->phase == PH_JOURNAL) {
        draw_journal(a);
        return;
    }
    if (g->phase == PH_AUGMENT) {
        gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){7, 22, 26, 232});
        gfx_center(f, "마을을 구하고 얻은 깨달음", w / 2, 24, 14, GOLD);
        gfx_center(f, "무엇을 새길 것인가", w / 2, 52, 30, PAPER);
        float cw = (fminf(w - 52, 1020) - 24) / 3, x = (w - (cw * 3 + 24)) / 2, top = 90, ch = h - top - 18;
        for (int i = 0; i < 3; i++) {
            SDL_FRect r = {x + (cw + 12) * (float)i, top, cw, ch};
            gfx_rect(f, r, (SDL_Color){25, 51, 47, 250});
            gfx_border(f, r, (SDL_Color){117, 137, 102, 255});
            gfx_text(f, AUGMENTS[i].kind, r.x + 15, r.y + 16, 14, cw - 30, GOLD);
            gfx_text(f, AUGMENTS[i].name, r.x + 15, r.y + 46, 22, cw - 30, PAPER);
            gfx_text(f, AUGMENTS[i].description, r.x + 15, r.y + 75, w < 740 ? 12 : 15, cw - 30, MUTED);
            button(a, (SDL_FRect){r.x + 15, r.y + ch - 58, cw - 30, 42}, "새기기", ACT_CHOOSE, i + 1, true,
                   true);
        }
        return;
    }
    if (g->phase == PH_PAUSED || g->phase == PH_DEAD || g->phase == PH_COMPLETE) {
        gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){7, 22, 26, 237});
        const char *title = g->phase == PH_PAUSED ? "잠시 숨 고르기"
                            : g->phase == PH_DEAD ? "다시 칼을 쥐다"
                                                  : "길은 계속된다";
        gfx_center(f, title, w / 2, 30, 31, PAPER);
        if (g->phase == PH_PAUSED) {
            gfx_center(f, "화면을 끌거나 목적지를 탭해 이동 · 가까운 적 자동 공격", w / 2, 84, 16, MUTED);
            gfx_center(f, "WASD / 방향키 이동 · Space 스킬 · E 상호작용", w / 2, 115, 15, MUTED);
            gfx_center(f, "I 가방 · J 여정 기록 · 1 탕약 · 2 지혈산 · F11 전체화면", w / 2, 146, 14, MUTED);
        } else if (g->phase == PH_DEAD) {
            gfx_center(f, "마을에서 회복하고 다시 떠납니다.", w / 2, 88, 18, MUTED);
            gfx_center(f, "조사한 흔적, 장비와 회수한 물품은 유지됩니다.", w / 2, 122, 16, MUTED);
        } else {
            gfx_center(f, "마을의 불빛을 뒤로하고, 장군은 전라도로 향한다.", w / 2, 86, 18, MUTED);
            gfx_center(f, g->augment > AUG_NONE ? AUGMENTS[g->augment - 1].name : "첫 여정 완료", w / 2, 124,
                       20, GOLD);
        }
        float top = fmaxf(186, h * .57f), bw = fminf(380, w - 56);
        button(a, (SDL_FRect){(w - bw) / 2, top, bw, 46},
               g->phase == PH_PAUSED ? "계속하기"
               : g->phase == PH_DEAD ? (g->replay ? "백정전 다시 도전" : "마을에서 다시 일어나기")
                                     : "선택한 증강으로 백정전 다시 하기",
               g->phase == PH_PAUSED ? ACT_CLOSE
               : g->phase == PH_DEAD ? ACT_RECOVER
                                     : ACT_REPLAY,
               0, true, true);
        button(a, (SDL_FRect){(w - bw) / 2, top + 57, bw, 42}, "처음으로", ACT_TITLE, 0, true, false);
        return;
    }
}
static void resize(App *a) {
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
    a->width = (float)ww / a->density;
    a->height = (float)wh / a->density;
    SDL_RenderSetScale(a->renderer, (float)ow / a->width, (float)oh / a->height);
}
static void draw(App *a) {
    resize(a);
    a->button_count = 0;
    Vec2 focus = a->game.phase == PH_TITLE || a->game.phase == PH_PROLOGUE ? HOME : a->game.hero.pos;
    a->camera = world_camera(a->width, a->height, focus);
    gfx_world(&a->gfx, &a->game, a->width, a->height, a->camera, a->pointer_down || a->target_set,
              a->target_set, a->target);
    if (a->debug_collision) {
        gfx_collision(&a->gfx, a->camera);
        gfx_circle(&a->gfx, (Vec2){a->game.hero.pos.x - a->camera.x, a->game.hero.pos.y - a->camera.y},
                   HERO_RADIUS, GOLD);
    }
    if (a->game.phase == PH_PROLOGUE && a->story_index < 4) {
        const int pictures[4] = {0, 1, 1, 2};
        gfx_story(&a->gfx, pictures[a->story_index], a->width, a->height, a->story_elapsed);
    }
    if (a->game.phase != PH_TITLE && a->game.phase != PH_PROLOGUE)
        draw_hud(a);
    draw_overlay(a);
    if (a->game.phase == PH_PLAY && a->game.toast_time > 0) {
        float width = fminf(550, a->width * .76f), x = (a->width - width) / 2, y = a->height * .35f;
        gfx_rect(&a->gfx, (SDL_FRect){x, y, width, 59}, INK);
        gfx_border(&a->gfx, (SDL_FRect){x, y, width, 59}, (SDL_Color){143, 132, 94, 220});
        gfx_text(&a->gfx, a->game.toast, x + 13, y + 10, 14, width - 26, PAPER);
    }
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
static void pointer_down(App *a, Sint64 id, Vec2 p) {
    for (int i = a->button_count - 1; i >= 0; i--) {
        Button b = a->buttons[i];
        if (inside(b.rect, p)) {
            if (b.enabled) {
                if (b.action == ACT_JOURNAL && a->game.phase == PH_JOURNAL && b.value != 0)
                    a->journal_scroll += (float)b.value;
                else
                    dispatch(a, b.action, b.value);
            }
            return;
        }
    }
    if (a->game.phase != PH_PLAY || a->width <= a->height || a->pointer_down)
        return;
    a->pointer_down = true;
    a->pointer_id = id;
    a->pointer_start = p;
    a->pointer_delta = (Vec2){0, 0};
    a->pointer_moved = false;
    a->target_set = false;
}
static void pointer_move(App *a, Sint64 id, Vec2 p) {
    if (!a->pointer_down || a->pointer_id != id)
        return;
    float dx = p.x - a->pointer_start.x, dy = p.y - a->pointer_start.y, d = hypotf(dx, dy), m = fmaxf(34, d);
    a->pointer_delta = (Vec2){dx / m, dy / m};
    if (d > 8)
        a->pointer_moved = true;
}
static void pointer_up(App *a, Sint64 id, Vec2 p) {
    if (!a->pointer_down || a->pointer_id != id)
        return;
    if (!a->pointer_moved && a->game.phase == PH_PLAY) {
        a->target =
            (Vec2){clampf(p.x + a->camera.x, 28, WORLD_W - 28), clampf(p.y + a->camera.y, 28, WORLD_H - 28)};
        a->target_set = world_route(a->game.hero.pos, a->target, HERO_RADIUS, &a->route);
        if (a->target_set)
            a->target = a->route.points[a->route.count - 1];
        else
            game_toast(&a->game, "그곳으로는 갈 수 없습니다. 열린 길을 선택해 주세요.");
    }
    a->pointer_down = false;
    a->pointer_delta = (Vec2){0, 0};
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
            SDL_Keycode key = e.key.keysym.sym;
            if (key == SDLK_ESCAPE || key == SDLK_AC_BACK)
                dispatch(a, ACT_PAUSE, 0);
            else if (key == SDLK_F11)
                dispatch(a, ACT_FULLSCREEN, 0);
            else if (key == SDLK_F3)
                a->debug_collision = !a->debug_collision;
            else if (key == SDLK_SPACE)
                dispatch(a, ACT_SKILL, 0);
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
            else if (key == SDLK_RETURN) {
                if (a->game.phase == PH_TITLE)
                    dispatch(a, ACT_START, 0);
                else
                    dispatch(a, ACT_NEXT, 0);
            }
            if (a->game.phase == PH_JOURNAL) {
                if (key == SDLK_DOWN)
                    a->journal_scroll++;
                else if (key == SDLK_UP)
                    a->journal_scroll--;
            }
        }
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
    } else if (!strcmp(scene, "bag")) {
        a->game.hero.hp = 45;
        a->game.phase = PH_BAG;
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
            game_toast(&a->game, "백정을 제압했다 · 70문. 마을 이장에게 돌아가세요.");
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
    double w = 0, h = 0;
    emscripten_get_element_css_size("#canvas", &w, &h);
    float density = clampf((float)emscripten_get_device_pixel_ratio(), 1, 3);
    if (w > 0 && h > 0)
        SDL_SetWindowSize(a->window, (int)lround(w * density), (int)lround(h * density));
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
                 "collision] "
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
                   a->gfx.glyph_count, status ? "FAILED" : "OK");
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
