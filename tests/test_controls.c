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
    /* Auto-loot uses no button and leaves ongoing movement intact. */
    a->target_set = true;
    for (int i = 0; i < 24; i++) game_tick(&a->game, .05f, (Vec2){0, 0});
    CHECK(a->game.herbs == 2 && a->target_set);
    CHECK(a->game.phase == PH_PLAY && a->game.cooldown == 0);
}
static void zoomed_destination(App *a) {
    game_reset(&a->game); game_start(&a->game);
    a->button_count = 0;
    a->camera = world_camera(a->width / WORLD_ZOOM, a->height / WORLD_ZOOM, HOME);
    Vec2 tap = {a->width / 2 + 72, a->height / 2 + 12};
    Vec2 expected = {HOME.x + 60, HOME.y + 10};
    pointer_down(a, -1, tap);
    pointer_up(a, -1, tap);
    CHECK(a->target_set && distance(a->target, expected) < .01f);
    Vec2 origin = world_camera(a->width / WORLD_ZOOM, a->height / WORLD_ZOOM, (Vec2){0, 0});
    CHECK(origin.x == 0 && origin.y == 0);
    Vec2 far = world_camera(a->width / WORLD_ZOOM, a->height / WORLD_ZOOM,
                            (Vec2){WORLD_W, WORLD_H});
    CHECK(fabsf(far.x + a->width / WORLD_ZOOM - WORLD_W) < .01f);
    CHECK(fabsf(far.y + a->height / WORLD_ZOOM - WORLD_H) < .01f);
}
int main(void) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_EVENTS) != 0) return 1;
    App *a = calloc(1, sizeof *a);
    if (!a) return 1;
    a->width = 844; a->height = 390; a->density = 1;
    conversation(a);
    pickup(a);
    zoomed_destination(a);
    free(a);
    SDL_Quit();
    printf("PASS: %d controls checks: Space dialogue/repeat, Q skill, F/automatic pickup, panel isolation.\n", checks);
    return 0;
}
