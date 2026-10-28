/* Regression for the short landscape overflow reported in the prologue. */
#define SDL_MAIN_HANDLED
#define main paldo_app_main
#include "../src/main.c"
#undef main
#include "test_legacy_world.h"

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static void narrative_fits(App *a, const char *text) {
    SDL_FRect p = narrative_rect(a, text);
    float size = a->width < 600 ? 16 : 17;
    CHECK(p.x >= 0 && p.y >= 0 && p.x + p.w <= a->width && p.y + p.h <= a->height);
    CHECK(gfx_text_height(&a->gfx, text, size, p.w - 30) <= p.h - 47);
}
static void all_narratives(App *a) {
    const Vec2 sizes[] = {{432,270},{480,306},{568,320},{599,382},{844,390},{1100,650}};
    const Line *groups[] = {MEET_LINES,DELIVER_LINES,REPORT_LINES,RETURN_LINES,BOSS_LINES,MESSENGER_LINES};
    const int lengths[] = {3,2,3,3,2,3};
    for (int view = 0; view < 6; view++) {
        a->width = sizes[view].x; a->height = sizes[view].y;
        for (int i = 0; i < 5; i++) narrative_fits(a, STORY_TEXT[i]);
        for (int group = 0; group < 6; group++)
            for (int i = 0; i < lengths[group]; i++) narrative_fits(a, groups[group][i].text);
        for (int i = 0; i < TRACE_COUNT; i++) narrative_fits(a, TRACES[i].text);
    }
}
static void text_stays_inside(App *a, SDL_Surface *surface) {
    const float scales[] = {1,1.25f,2};
    for (int i = 0; i < 3; i++) {
        float scale = scales[i];
        SDL_RenderSetScale(a->renderer, scale, scale);
        SDL_SetRenderDrawColor(a->renderer, 0,0,0,255);
        SDL_RenderClear(a->renderer);
        SDL_FRect box = {20,20,170,38};
        float size = gfx_text_box(&a->gfx,
            "왕명을 받은 젊은 장군이 경상도 남부의 작은 마을에 도착했다.\n추가로 긴 문장도 상자 밖으로 나갈 수 없다.",
            box, 17, 14, (SDL_Color){255,255,255,255});
        CHECK(size >= 14 && !SDL_RenderIsClipEnabled(a->renderer));
        SDL_RenderPresent(a->renderer);
        SDL_LockSurface(surface);
        int inside = 0, outside = 0;
        for (int y = 0; y < surface->h; y++)
            for (int x = 0; x < surface->w; x++) {
                Uint32 pixel = ((Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch))[x];
                Uint8 r,g,b,alpha;
                SDL_GetRGBA(pixel,surface->format,&r,&g,&b,&alpha);
                if (!(r || g || b)) continue;
                if (x >= floorf(box.x*scale) && x < ceilf((box.x+box.w)*scale) &&
                    y >= floorf(box.y*scale) && y < ceilf((box.y+box.h)*scale)) inside++;
                else outside++;
            }
        SDL_UnlockSurface(surface);
        CHECK(inside > 100 && outside == 0);
    }
    SDL_RenderSetScale(a->renderer,1,1);
    SDL_Rect outer = {10,10,100,80}, after;
    SDL_RenderSetClipRect(a->renderer,&outer);
    gfx_label(&a->gfx,"긴 제목도 안으로",(SDL_FRect){0,0,190,24},38,PAPER);
    SDL_RenderGetClipRect(a->renderer,&after);
    CHECK(SDL_RenderIsClipEnabled(a->renderer) && after.x == outer.x && after.y == outer.y &&
          after.w == outer.w && after.h == outer.h);
    SDL_RenderSetClipRect(a->renderer,NULL);
}
static void panel_controls_fit(App *a) {
    const Vec2 sizes[]={{432,270},{480,306},{568,320},{844,390},{1100,650}};
    legacy_reset(&a->game); game_start(&a->game);
    a->game.owned_sword=a->game.sword=a->game.owned_armor=a->game.armor=true;
    a->game.herbs=2;
    for(int view=0;view<5;view++) {
        a->width=sizes[view].x; a->height=sizes[view].y;
        for(int tab=0;tab<4;tab++) {
            a->game.phase=PH_BAG; a->bag_tab=tab; a->bag_selected=-999;
            a->button_count=0; draw_bag(a);
            CHECK(a->button_count<BUTTON_MAX);
            int tabs=0, gear=0;
            for(int i=0;i<a->button_count;i++) {
                Button b=a->buttons[i];
                CHECK(b.rect.w>0 && b.rect.h>0 && b.rect.x>=0 && b.rect.y>=0);
                CHECK(b.rect.x+b.rect.w<=a->width && b.rect.y+b.rect.h<=a->height);
                tabs+=b.action==ACT_BAG_TAB; gear+=b.action==ACT_GEAR_SELECT;
            }
            CHECK(tabs==4 && gear==6);
        }
        a->game.phase=PH_TECHNIQUES; a->button_count=0; draw_techniques(a);
        int binds=0, directions=0;
        for(int i=0;i<a->button_count;i++) {
            Button b=a->buttons[i];
            CHECK(b.rect.x>=0 && b.rect.y>=0 && b.rect.w>0 && b.rect.h>0);
            CHECK(b.rect.x+b.rect.w<=a->width && b.rect.y+b.rect.h<=a->height);
            binds+=b.action==ACT_TECH_BIND; directions+=b.action==ACT_SWIPE_SELECT;
        }
        CHECK(binds==4 && directions==4);
        for(int touch=0;touch<2;touch++) {
            a->touch_input=touch!=0;
            a->game.phase=PH_PLAY;
            /* Force a nearby interaction button to catch narrow HUD overlaps. */
            a->game.hero.pos=NPCS[ELDER].pos;
            a->button_count=0; draw_hud(a);
            int attack_count=0, interact_count=0;
            for(int i=0;i<a->button_count;i++) {
                SDL_FRect x=a->buttons[i].rect;
                CHECK(x.x>=0 && x.y>=0 && x.w>0 && x.h>0);
                CHECK(x.x+x.w<=a->width && x.y+x.h<=a->height);
                if(a->buttons[i].action==ACT_INTERACT) interact_count++;
                if(a->buttons[i].action==ACT_ATTACK) {
                    attack_count++;
                    CHECK(x.w==x.h && x.w==88);
                    if(touch) {
                        CHECK(x.x==a->width-160 && x.y==a->height-160);
                        CHECK(a->width-(x.x+x.w)>=72 && a->height-(x.y+x.h)>=72);
                    }
                }
                for(int j=i+1;j<a->button_count;j++) {
                    SDL_FRect y=a->buttons[j].rect;
                    CHECK(x.x>=y.x+y.w || y.x>=x.x+x.w ||
                          x.y>=y.y+y.h || y.y>=x.y+x.h);
                }
            }
            CHECK(attack_count==1 && interact_count==1);
        }
        a->touch_input=false;
    }
}
/* Pause, fullscreen and bag are 44px icon buttons in one top row; the coin
 * amount sits left of the bag and never reaches the location card. */
static void top_hud_icons_fit(App *app) {
    const Vec2 sizes[] = {{432,270},{480,306},{568,320},{844,390},{1100,650},{1920,1080}};
    const int amounts[] = {0, 55, 999, 9999, 999999};
    legacy_reset(&app->game); game_start(&app->game);
    for (int view = 0; view < 6; view++) {
        app->width = sizes[view].x; app->height = sizes[view].y;
        for (int amount = 0; amount < 5; amount++) {
            app->game.coins = amounts[amount];
            app->button_count = 0; draw_hud(app);
            SDL_FRect bag = {0}, pause = {0}, screen = {0};
            for (int i = 0; i < app->button_count; i++) {
                Button control = app->buttons[i];
                if (control.action == ACT_BAG) bag = control.rect;
                if (control.action == ACT_PAUSE) pause = control.rect;
                if (control.action == ACT_FULLSCREEN) screen = control.rect;
            }
            CHECK(bag.w == 44 && bag.h == 44 && pause.w == 44 && screen.w == 44);
            CHECK(bag.y == pause.y && screen.y == pause.y);
            CHECK(bag.x + bag.w <= screen.x && screen.x + screen.w <= pause.x);
            CHECK(pause.x + pause.w <= app->width);
            char text[32]; float size;
            snprintf(text, sizeof text, "%d", amounts[amount]);
            SDL_FRect chip = coin_chip_rect(&app->gfx, bag.x - 2, text, &size);
            CHECK(chip.x + chip.w <= bag.x && chip.y >= bag.y && chip.y + chip.h <= bag.y + bag.h);
            CHECK(size >= 11 && size <= 15);
            if (amounts[amount] <= 9999) CHECK(chip.x >= COIN_CHIP_LEFT);
            if (amounts[amount] <= 999) CHECK(size == 15);
        }
    }
}
/* Bag/tonic/bandage art is optional: absent files leave the slot empty;
 * a supplied picture is drawn inside its own button only. */
static void hud_art_slots(App *app, SDL_Surface *surface) {
    for (int i = 0; i < UI_ICON_COUNT; i++)
        CHECK(app->gfx.ui_icons[i].texture == NULL || app->gfx.ui_icons[i].w > 0);
    Picture saved[UI_ICON_COUNT];
    memcpy(saved, app->gfx.ui_icons, sizeof saved);
    SDL_Texture *art = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 64, 64);
    CHECK(art != NULL);
    SDL_SetRenderTarget(app->renderer, art);
    SDL_SetRenderDrawColor(app->renderer, 255, 0, 255, 255); SDL_RenderClear(app->renderer);
    SDL_SetRenderTarget(app->renderer, NULL);
    legacy_reset(&app->game); game_start(&app->game);
    app->width = 480; app->height = 306; app->touch_input = false;
    for (int slot = 0; slot < UI_ICON_COUNT; slot++) {
        for (int i = 0; i < UI_ICON_COUNT; i++) app->gfx.ui_icons[i] = (Picture){0};
        app->gfx.ui_icons[slot] = (Picture){art, 64, 64};
        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255); SDL_RenderClear(app->renderer);
        app->button_count = 0; draw_hud(app);
        SDL_RenderPresent(app->renderer);
        Action want = slot == UI_ICON_BAG ? ACT_BAG : ACT_USE;
        int value = slot == UI_ICON_TONIC ? ITEM_TONIC : slot == UI_ICON_BANDAGE ? ITEM_BANDAGE : 0;
        SDL_FRect home = {0};
        for (int i = 0; i < app->button_count; i++)
            if (app->buttons[i].action == want && app->buttons[i].value == value) home = app->buttons[i].rect;
        CHECK(home.w > 0);
        int inside = 0, outside = 0;
        SDL_LockSurface(surface);
        for (int y = 0; y < surface->h; y++)
            for (int x = 0; x < surface->w; x++) {
                Uint8 r, g, b, a2;
                SDL_GetRGBA(((Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch))[x], surface->format, &r, &g, &b, &a2);
                if (!(r > 120 && g < 90 && b > 120 && r - g > 80)) continue;
                if (x >= home.x && x < home.x + home.w && y >= home.y && y < home.y + home.h) inside++;
                else outside++;
            }
        SDL_UnlockSurface(surface);
        CHECK(inside > 200 && outside == 0);
    }
    SDL_DestroyTexture(art);
    memcpy(app->gfx.ui_icons, saved, sizeof saved);
}
static void training_controls_fit(App *app) {
    const Vec2 sizes[] = {{432,270},{480,306},{568,320},{844,390},{1100,650}};
    Game saved;
    legacy_reset(&app->game); game_start(&app->game);
    CHECK(game_enter_training(&app->game, &saved));
    CHECK(gfx_measure(&app->gfx, "갈라베기 · 재사용 중", 12) <= 134);
    CHECK(gfx_measure(&app->gfx, "피해 38 · 입력 3/3", 12) <= 134);
    CHECK(gfx_measure(&app->gfx, "명중 3/3 · 1.0초", 12) <= 134);
    CHECK(gfx_measure(&app->gfx, "없음 · 방향 불명", 12) <= 134);
    CHECK(gfx_measure(&app->gfx, "정지 55ms · 피격 120ms", 12) <= 134);
    for (int view = 0; view < 5; view++) {
        app->width = sizes[view].x; app->height = sizes[view].y;
        for (int touch = 0; touch < 2; touch++) {
            app->touch_input = touch != 0;
            app->button_count = 0; draw_hud(app);
            int exits = 0, attacks = 0;
            SDL_FRect status = {12,58,148,146};
            for (int button_index = 0; button_index < app->button_count; button_index++) {
                Button control = app->buttons[button_index];
                SDL_FRect rect = control.rect;
                CHECK(rect.x >= 0 && rect.y >= 0 && rect.w > 0 && rect.h > 0);
                CHECK(rect.x + rect.w <= app->width && rect.y + rect.h <= app->height);
                CHECK(rect.x >= status.x + status.w || status.x >= rect.x + rect.w ||
                      rect.y >= status.y + status.h || status.y >= rect.y + rect.h);
                CHECK(control.action != ACT_INTERACT && control.action != ACT_JOURNAL && control.action != ACT_BAG);
                exits += control.action == ACT_TRAINING_EXIT;
                attacks += control.action == ACT_ATTACK;
                for (int other = button_index + 1; other < app->button_count; other++) {
                    SDL_FRect next = app->buttons[other].rect;
                    CHECK(rect.x >= next.x + next.w || next.x >= rect.x + rect.w ||
                          rect.y >= next.y + next.h || next.y >= rect.y + rect.h);
                }
            }
            CHECK(exits == 1 && attacks == 1);
        }
    }
    app->game.phase = PH_PAUSED; app->button_count = 0; draw_overlay(app);
    int returns = 0;
    for (int index = 0; index < app->button_count; index++) returns += app->buttons[index].action == ACT_TITLE;
    CHECK(returns == 1);
}
/* Lost health lingers as a pale trail, then drains to the real value;
 * healing and a fresh fight snap the trail up immediately. */
static void health_trails(void) {
    float ghost = 0, hold = 0;
    int seen = 0;
    trail_health(&ghost, &hold, &seen, 100, 100, .016f);
    CHECK(ghost == 100 && hold == 0);
    trail_health(&ghost, &hold, &seen, 82, 100, .016f);
    CHECK(ghost == 100 && hold > .3f);
    for (int i = 0; i < 20; i++) trail_health(&ghost, &hold, &seen, 82, 100, .016f);
    CHECK(ghost == 100);
    for (int i = 0; i < 5; i++) trail_health(&ghost, &hold, &seen, 82, 100, .016f);
    CHECK(ghost > 82 && ghost < 100);
    for (int i = 0; i < 60; i++) trail_health(&ghost, &hold, &seen, 82, 100, .016f);
    CHECK(ghost == 82 && hold == 0);
    trail_health(&ghost, &hold, &seen, 127, 100, .016f);
    CHECK(ghost == 127);
    /* A second hit while the trail lingers restarts the pause, keeping the old top. */
    trail_health(&ghost, &hold, &seen, 100, 100, .016f);
    for (int i = 0; i < 10; i++) trail_health(&ghost, &hold, &seen, 100, 100, .016f);
    trail_health(&ghost, &hold, &seen, 90, 100, .016f);
    CHECK(ghost == 127 && hold > .3f);
}
int main(int argc,char **argv) {
    SDL_SetMainReady();
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,480,306,32,SDL_PIXELFORMAT_RGBA32);
    App *a = calloc(1,sizeof *a);
    if (!surface || !a) return 1;
    a->renderer = SDL_CreateSoftwareRenderer(surface);
    if (!a->renderer || !gfx_load(&a->gfx,a->renderer,argc > 1 ? argv[1] : "assets")) return 1;
    health_trails();
    all_narratives(a);
    panel_controls_fit(a);
    training_controls_fit(a);
    top_hud_icons_fit(a);
    hud_art_slots(a,surface);
    text_stays_inside(a,surface);
    gfx_free(&a->gfx);
    SDL_DestroyRenderer(a->renderer);
    SDL_FreeSurface(surface);
    free(a);
    SDL_Quit();
    printf("PASS: %d UI checks: all narrative lines, compact landscapes, clipping at 1/1.25/2x.\n",checks);
    return 0;
}
