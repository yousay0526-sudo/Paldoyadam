/* Real App dispatch, title buttons and autosave lifecycle, in isolated storage. */
#define SDL_MAIN_HANDLED
#define main paldo_app_main
#include "../src/main.c"
#undef main
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#define MAKE_DIR(p) mkdir(p, 0700)
#endif
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL save UI:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
static bool has_button(App *a, Action action) {
    for (int i = 0; i < a->button_count; i++) if (a->buttons[i].action == action) return true;
    return false;
}
int main(int argc, char **argv) {
    CHECK(argc == 4);
    SDL_SetMainReady(); SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0);
    App *a = calloc(1, sizeof *a); CHECK(a != NULL);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 844, 390, 32, SDL_PIXELFORMAT_RGBA32);
    CHECK(surface != NULL); a->renderer = SDL_CreateSoftwareRenderer(surface); CHECK(a->renderer != NULL);
    CHECK(gfx_load(&a->gfx, a->renderer, argv[1]));
    MAKE_DIR(argv[2]); save_store_init(&a->save_store, argv[2]);
    a->save_enabled = true; a->width = 844; a->height = 390;
    if (!strcmp(argv[3], "write")) {
        const char *names[] = {"journey-autosave.sav", "journey-autosave.bak"};
        for (int i = 0; i < 2; i++) { char p[2400]; snprintf(p, sizeof p, "%s/%s", argv[2], names[i]); remove(p); }
    }
    game_reset(&a->game); refresh_save(a);
    if (!strcmp(argv[3], "write")) {
        CHECK(a->save_result == SAVE_MISSING);
        a->button_count = 0; draw_overlay(a); CHECK(!has_button(a, ACT_CONTINUE));
        dispatch(a, ACT_SKIP, 0); CHECK(a->game.phase == PH_PLAY && a->save_active);
        a->game.hero.pos = NPCS[ELDER].pos;
        dispatch(a, ACT_INTERACT, 0); CHECK(a->game.phase == PH_DIALOGUE);
        for (int i = 0; i < 3; i++) dispatch(a, ACT_NEXT, 0);
        CHECK(a->game.quest == Q_TRACES);
        a->game.coins = 41;
        a->save_elapsed = .99f; autosave(a, false);
        Game loaded; bool recovered;
        CHECK(save_store_load(&a->save_store, &loaded, &recovered) == SAVE_OK && loaded.coins == 55);
        a->save_elapsed = 1; autosave(a, false);
        CHECK(save_store_load(&a->save_store, &loaded, &recovered) == SAVE_OK && loaded.coins == 41);
        dispatch(a, ACT_TITLE, 0); CHECK(a->game.phase == PH_TITLE && a->save_available);
    } else {
        CHECK(a->save_available);
        for (int i = 0; i < 3; i++) {
            a->width = i == 0 ? 432 : i == 1 ? 844 : 1280;
            a->height = i == 0 ? 270 : i == 1 ? 390 : 720;
            a->button_count = 0; draw_overlay(a); CHECK(has_button(a, ACT_CONTINUE));
            for (int k = 0; k < a->button_count; k++) {
                SDL_FRect r = a->buttons[k].rect;
                CHECK(r.x >= 0 && r.y >= 0 && r.x + r.w <= a->width && r.y + r.h <= a->height - 24);
            }
        }
        dispatch(a, ACT_START, 0); CHECK(a->new_confirm && a->game.phase == PH_TITLE);
        dispatch(a, ACT_SKIP, 0); CHECK(a->game.phase == PH_TITLE);
        dispatch(a, ACT_NEW_CANCEL, 0); CHECK(!a->new_confirm);
        dispatch(a, ACT_SKIP, 0); CHECK(a->new_confirm == 2);
        dispatch(a, ACT_PAUSE, 0); CHECK(!a->new_confirm);
        key_assign(a, KEY_ATTACK, SDLK_b);
        dispatch(a, ACT_CONTINUE, 0);
        CHECK(a->game.phase == PH_PLAY && a->game.quest == Q_TRACES && a->game.coins == 41);
        CHECK(key_for(a, KEY_ATTACK) == SDLK_b);
        Game journey = a->game;
        const Vec2 screens[] = {{432,270}, {480,306}, {568,320}, {667,375}, {844,390}, {1100,650}, {1280,720}, {1920,1080}};
        for (int i = 0; i < 8; i++) for (int touch = 0; touch < 2; touch++) for (int scene = 0; scene < 3; scene++) for (int hurt = 0; hurt < 2; hurt++) {
            a->game = journey;
            if (scene == 1) { a->game.quest = Q_COMBAT; a->game.map = MAP_CAVE; }
            if (scene == 2) { Game ignored; CHECK(game_enter_training(&a->game, &ignored)); }
            a->game.chain_hits = 12; a->game.combo_count = 3; a->game.combo_time = COMBO_SECONDS;
            if (hurt) { a->game.hero.bleed = 2; a->game.perfect_flash = .7f; }
            a->width = screens[i].x; a->height = screens[i].y; a->touch_input = touch != 0;
            a->button_count = 0; draw_hud(a);
            SDL_FRect r = save_status_rect(a);
            if (r.w <= 0) fprintf(stderr, "No notice room: %.0fx%.0f touch=%d scene=%d hurt=%d\n", a->width, a->height, touch, scene, hurt);
            CHECK(r.w > 0 && r.x >= 0 && r.y >= 0 && r.x + r.w <= a->width && r.y + r.h <= a->height);
            CHECK(a->combo.w <= 0 || !rects_overlap(r, a->combo));
            CHECK(!rects_overlap(r, a->attack_hint));
            CHECK(a->minimap.w <= 0 || !rects_overlap(r, a->minimap));
            if (scene == 1) CHECK(!rects_overlap(r, boss_bar_rect(a)));
            if (hurt) {
                CHECK(!rects_overlap(r, (SDL_FRect){18, a->height - 85, 120, 20}));
                CHECK(!rects_overlap(r, (SDL_FRect){a->width / 2 - 100, a->height / 2 - fminf(130, a->height * .3f), 200, 66}));
            }
            for (int j = 0; j < a->button_count; j++) {
                SDL_FRect b = a->buttons[j].rect;
                CHECK(r.x >= b.x + b.w || b.x >= r.x + r.w || r.y >= b.y + b.h || b.y >= r.y + r.h);
            }
        }
        a->game = journey;
        SaveStore real_store = a->save_store;
        /* Routine successful saves stay quiet; failures and the first success
         * after a failure show the notice. */
        a->save_message_time = 0; a->game.coins = 42; autosave(a, true);
        CHECK(a->save_result == SAVE_OK && a->save_message_time == 0);
        save_store_init(&a->save_store, NULL); a->game.coins = 40; autosave(a, true);
        CHECK(a->save_result == SAVE_UNAVAILABLE && a->save_message_time > 0);
        a->save_message_time = 0; a->game.coins = 39; autosave(a, true);
        CHECK(a->save_result == SAVE_UNAVAILABLE && a->save_message_time > 0);
        a->save_message_time = 0;
        a->save_store = real_store; a->game.coins = 41; autosave(a, true);
        CHECK(a->save_result == SAVE_OK && a->save_message_time > 0);
        a->save_message_time = 0; a->game.coins = 43; autosave(a, true);
        CHECK(a->save_result == SAVE_OK && a->save_message_time == 0);
        a->game.coins = 41; autosave(a, true);
        Game before = a->game;
        toggle_training(a); CHECK(a->game.training.active);
        a->game.coins = 999; a->save_elapsed = 3; autosave(a, true);
        Game loaded; bool recovered;
        CHECK(save_store_load(&a->save_store, &loaded, &recovered) == SAVE_OK && loaded.coins == 41);
        toggle_training(a); CHECK(!a->game.training.active && a->game.coins == before.coins);
        a->game.coins = 37;
        SDL_Event e = {0}; e.type = SDL_APP_WILLENTERBACKGROUND; SDL_PushEvent(&e); handle_events(a);
        CHECK(save_store_load(&a->save_store, &loaded, &recovered) == SAVE_OK && loaded.coins == 37);
        dispatch(a, ACT_TITLE, 0); dispatch(a, ACT_START, 0); dispatch(a, ACT_NEW_CONFIRM, 0);
        CHECK(a->game.phase == PH_PROLOGUE && a->game.quest == Q_MEET && a->game.coins == 55);
        CHECK(key_for(a, KEY_ATTACK) == SDLK_b);
        CHECK(save_store_load(&a->save_store, &loaded, &recovered) == SAVE_OK && loaded.phase == PH_PROLOGUE);
    }
    gfx_free(&a->gfx); SDL_DestroyRenderer(a->renderer); SDL_FreeSurface(surface); free(a); SDL_Quit();
    printf("PASS: %d native session/title/confirmation/autosave checks (%s)\n", checks, argv[3]);
    return 0;
}
