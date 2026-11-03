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
        dispatch(a, ACT_CONTINUE, 0);
        CHECK(a->game.phase == PH_PLAY && a->game.quest == Q_TRACES && a->game.coins == 41);
        const Vec2 screens[] = {{432,270}, {568,320}, {844,390}, {1280,720}};
        for (int i = 0; i < 4; i++) for (int touch = 0; touch < 2; touch++) {
            a->width = screens[i].x; a->height = screens[i].y; a->touch_input = touch != 0;
            a->button_count = 0; draw_hud(a);
            SDL_FRect r = save_status_rect(a);
            for (int j = 0; j < a->button_count; j++) {
                SDL_FRect b = a->buttons[j].rect;
                CHECK(r.x >= b.x + b.w || b.x >= r.x + r.w || r.y >= b.y + b.h || b.y >= r.y + r.h);
            }
        }
        SaveStore real_store = a->save_store;
        save_store_init(&a->save_store, NULL); a->game.coins = 40; autosave(a, true);
        CHECK(a->save_result == SAVE_UNAVAILABLE);
        a->save_store = real_store; a->game.coins = 41; autosave(a, true);
        CHECK(a->save_result == SAVE_OK);
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
        CHECK(save_store_load(&a->save_store, &loaded, &recovered) == SAVE_OK && loaded.phase == PH_PROLOGUE);
    }
    gfx_free(&a->gfx); SDL_DestroyRenderer(a->renderer); SDL_FreeSurface(surface); free(a); SDL_Quit();
    printf("PASS: %d native session/title/confirmation/autosave checks (%s)\n", checks, argv[3]);
    return 0;
}
