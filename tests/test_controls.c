/* Drive the real SDL event handler; verify observable game state, not key constants. */
#define SDL_MAIN_HANDLED
#define main paldo_app_main
#include "../src/main.c"
#undef main

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static void key(App *a, SDL_Keycode code, bool repeat) {
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = code;
    e.key.repeat = repeat;
    SDL_PushEvent(&e);
    handle_events(a);
}
static void conversation(App *a) {
    game_reset(&a->game);
    key(a, SDLK_SPACE, false);
    CHECK(a->game.phase == PH_PROLOGUE && a->story_index == 0);
    key(a, SDLK_SPACE, true);
    CHECK(a->story_index == 0);
    for (int i = 0; i < 5; i++) key(a, SDLK_SPACE, false);
    CHECK(a->game.phase == PH_PLAY);
    a->game.hero.pos = NPCS[ELDER].pos;
    a->target_set = true;
    key(a, SDLK_SPACE, false);
    CHECK(a->game.phase == PH_DIALOGUE && a->dialogue_index == 0);
    CHECK(!a->target_set && a->game.cooldown == 0);
    key(a, SDLK_SPACE, true);
    CHECK(a->dialogue_index == 0);
    key(a, SDLK_q, false);
    CHECK(a->game.cooldown == 0);
    for (int i = 0; i < 3; i++) key(a, SDLK_SPACE, false);
    CHECK(a->game.phase == PH_PLAY && a->game.quest == Q_TRACES);
    CHECK(a->game.cooldown == 0);
    key(a, SDLK_q, false);
    CHECK(a->game.cooldown > 0);
    key(a, SDLK_j, false);
    CHECK(a->game.phase == PH_JOURNAL);
    key(a, SDLK_SPACE, false);
    CHECK(a->game.phase == PH_JOURNAL);
    key(a, SDLK_ESCAPE, false);
    CHECK(a->game.phase == PH_PLAY);
}
static void pickup(App *a) {
    game_reset(&a->game);
    game_start(&a->game);
    a->game.quest = Q_RECOVER;
    a->game.hero.pos = NPCS[ELDER].pos;
    a->game.drops[0] = (Drop){.pos = NPCS[ELDER].pos, .exists = true};
    a->target_set = true;
    key(a, SDLK_f, false);
    CHECK(a->game.herbs == 1 && a->game.drops[0].collected);
    CHECK(a->game.phase == PH_PLAY && a->game.cooldown == 0);
    CHECK(a->target_set); /* Loot does not interrupt movement. */
    key(a, SDLK_f, false);
    CHECK(a->game.herbs == 1);
    key(a, SDLK_SPACE, false);
    CHECK(a->game.phase == PH_DIALOGUE);
    a->game.drops[1] = (Drop){.pos = NPCS[ELDER].pos, .exists = true};
    key(a, SDLK_f, false);
    CHECK(a->game.herbs == 1);
    key(a, SDLK_RETURN, false);
    CHECK(a->game.phase == PH_PLAY);
    /* Use the same touch hit testing as the on-screen pickup control. */
    a->button_count = 1;
    a->buttons[0] = (Button){{636, 246, 194, 44}, ACT_PICKUP, 0, true};
    pointer_down(a, 3, (Vec2){700, 270});
    CHECK(a->game.herbs == 2 && !a->pointer_down);
    CHECK(a->game.phase == PH_PLAY && a->game.cooldown == 0);
}
int main(void) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_EVENTS) != 0) return 1;
    App *a = calloc(1, sizeof *a);
    if (!a) return 1;
    a->width = 844; a->height = 390; a->density = 1;
    conversation(a);
    pickup(a);
    free(a);
    SDL_Quit();
    printf("PASS: %d controls checks: Space dialogue/repeat, Q skill, F/touch pickup, panel isolation.\n", checks);
    return 0;
}
