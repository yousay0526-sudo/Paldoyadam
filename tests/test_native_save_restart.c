/* Run the production main loop in two OS processes. Only the storage path,
 * allocator observation and render-present callback are test adapters.
 * Gameplay is driven by SDL mouse/key/quit events, never Game writes. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p, 0700)
#endif
static void *observed_app;
static const char *profile;
static int stage, checks;
static bool writing, finished;
static Uint32 started;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL native main:%d stage=%d: %s\n", __LINE__, stage, #c); exit(1); } } while (0)
static void *observe_app(size_t n, size_t size) {
    void *p = calloc(n, size); if (!observed_app) observed_app = p; return p;
}
static char *test_pref(const char *org, const char *app) {
    (void)org; (void)app;
    char *p = SDL_malloc(strlen(profile) + 2);
    if (p) sprintf(p, "%s/", profile);
    return p;
}
static void observed_present(SDL_Renderer *renderer);
#define calloc observe_app
#define SDL_GetPrefPath test_pref
#define SDL_RenderPresent observed_present
#define main production_main
#include "../src/main.c"
#undef main
#undef SDL_RenderPresent
#undef SDL_GetPrefPath
#undef calloc
static void key_event(SDL_Keycode key) {
    SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = key; CHECK(SDL_PushEvent(&e) == 1);
    e.type = SDL_KEYUP; CHECK(SDL_PushEvent(&e) == 1);
}
static void tap(float x, float y) {
    SDL_Event e = {0}; e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
    e.button.x = (int)x; e.button.y = (int)y; CHECK(SDL_PushEvent(&e) == 1);
    e.type = SDL_MOUSEBUTTONUP; CHECK(SDL_PushEvent(&e) == 1);
}
static void control(App *a, Action action, int value) {
    for (int i = 0; i < a->button_count; i++) {
        Button b = a->buttons[i];
        if (b.enabled && b.action == action && b.value == value) {
            tap(b.rect.x + b.rect.w / 2, b.rect.y + b.rect.h / 2); return;
        }
    }
    CHECK(false);
}
static void quit_event(void) {
    SDL_Event e = {0}; e.type = SDL_QUIT; CHECK(SDL_PushEvent(&e) == 1); finished = true;
}
static void expected(App *a, bool write) {
    char p[2400]; snprintf(p, sizeof p, "%s/expected.txt", profile);
    FILE *f = fopen(p, write ? "w" : "r"); CHECK(f != NULL);
    if (write) fprintf(f, "%d %.9g %.9g %d %d %d\n", a->game.quest,
                       a->game.hero.pos.x, a->game.hero.pos.y, a->game.hero.hp, a->game.coins, key_for(a, KEY_ATTACK));
    else {
        int quest, hp, coins, attack; float x, y;
        CHECK(fscanf(f, "%d %f %f %d %d %d", &quest, &x, &y, &hp, &coins, &attack) == 6);
        CHECK(a->game.quest == quest && a->game.hero.hp == hp && a->game.coins == coins);
        CHECK(fabsf(a->game.hero.pos.x - x) < .01f && fabsf(a->game.hero.pos.y - y) < .01f);
        CHECK(key_for(a, KEY_ATTACK) == attack && attack == SDLK_b);
    }
    CHECK(fclose(f) == 0);
}
static void observed_present(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);
    App *a = observed_app;
    if (!started) started = SDL_GetTicks();
    CHECK(SDL_GetTicks() - started < 15000);
    if (finished) { CHECK(a->save_result == SAVE_OK); return; }
    if (!writing) {
        if (stage == 0 && a->game.phase == PH_TITLE) {
            CHECK(a->save_available); key_event(SDLK_RETURN); stage = 1;
        } else if (stage == 1 && a->game.phase == PH_PLAY) {
            CHECK(a->game.quest == Q_TRACES); expected(a, false); quit_event();
        }
        return;
    }
    if (stage == 0 && a->game.phase == PH_TITLE) {
        if (a->save_available) fprintf(stderr, "unexpected initial save: profile=%s store=%s result=%d recovered=%d argc-write=%d\n", profile, a->save_store.directory, a->save_result, a->save_recovered, writing);
        CHECK(!a->save_available); control(a, ACT_SKIP, 0); stage = 1;
    } else if (stage == 1 && a->game.phase == PH_PLAY) {
        float zoom = world_zoom(a->height);
        tap((NPCS[ELDER].pos.x - a->camera.x) * zoom, (NPCS[ELDER].pos.y - a->camera.y) * zoom); stage = 2;
    } else if (stage == 2 && game_nearby(&a->game).kind == NEAR_NPC) {
        key_event(SDLK_SPACE); stage = 3;
    } else if (stage == 3 && a->game.phase == PH_DIALOGUE) {
        key_event(SDLK_SPACE);
    } else if (stage == 3 && a->game.quest == Q_TRACES && a->game.phase == PH_PLAY) {
        key_event(SDLK_ESCAPE); stage = 4;
    } else if (stage == 4 && a->game.phase == PH_PAUSED) {
        control(a, ACT_KEY_MENU, 0); stage = 5;
    } else if (stage == 5 && a->key_menu) {
        control(a, ACT_KEY_SELECT, KEY_ATTACK); stage = 6;
    } else if (stage == 6 && a->key_waiting == KEY_ATTACK) {
        key_event(SDLK_b); stage = 7;
    } else if (stage == 7 && key_for(a, KEY_ATTACK) == SDLK_b) {
        control(a, ACT_CLOSE, 0); stage = 8;
    } else if (stage == 8 && !a->key_menu && a->game.phase == PH_PAUSED) {
        expected(a, true); quit_event();
    }
}
int main(int argc, char **argv) {
    CHECK(argc == 4); writing = !strcmp(argv[1], "write"); profile = argv[2]; MAKE_DIR(profile);
    if (writing) {
        const char *files[] = {"journey-autosave.sav", "journey-autosave.bak", "keys.txt", "expected.txt"};
        for (int i = 0; i < 4; i++) {
            char p[2400]; snprintf(p, sizeof p, "%s/%s", profile, files[i]);
            int result = remove(p);
            if (result && errno != ENOENT) fprintf(stderr, "fixture cleanup: %s: %s\n", p, strerror(errno));
            CHECK(result == 0 || errno == ENOENT);
        }
        SaveStore empty; Game probe; bool recovered;
        save_store_init(&empty, profile);
        CHECK(save_store_load(&empty, &probe, &recovered) == SAVE_MISSING);
    }
    SDL_SetMainReady(); SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    char *args[] = {argv[0], "--assets", argv[3], "--size", "1100x650"};
    int result = production_main(5, args);
    CHECK(result == 0 && finished);
    printf("PASS production main %s: quest, position, HP, coins, key settings; %d checks\n", argv[1], checks);
    return result;
}
