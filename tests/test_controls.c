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
static void key_up(App *a, SDL_Keycode code) {
    SDL_Event e={0}; e.type=SDL_KEYUP; e.key.keysym.sym=code;
    SDL_PushEvent(&e); handle_events(a);
}
static Vec2 attack_fixture(App *a) {
    game_reset(&a->game); game_start(&a->game); stop_input(a);
    a->width=844; a->height=390; a->button_count=1;
    a->buttons[0]=(Button){{728,274,88,88},ACT_ATTACK,0,true};
    return (Vec2){772,318};
}
static void combat_inputs(App *a) {
    Vec2 p=attack_fixture(a);
    pointer_down(a,10,p); CHECK(a->attack_pointer_down && !a->game.hero.attacking);
    pointer_up(a,10,p); CHECK(a->game.hero.attacking && !a->attack_pointer_down);
    const Vec2 swipes[]={{0,-60},{60,0},{0,60},{-60,0}};
    for(int d=0;d<SWIPE_COUNT;d++) {
        p=attack_fixture(a); Vec2 end={p.x+swipes[d].x,p.y+swipes[d].y};
        pointer_down(a,12,p); pointer_move(a,12,end);
        CHECK(a->attack_swiping && !a->game.hero.attacking && !a->game.hero.skilling);
        pointer_up(a,12,end);
        CHECK(a->game.hero.skilling && a->game.hero.technique==a->game.swipe_bindings[d]);
        CHECK(!a->game.hero.attacking && a->game.attack_cooldown==0);
    }
    p=attack_fixture(a); pointer_down(a,1,p);
    pointer_move(a,1,(Vec2){p.x+50,p.y+50}); pointer_up(a,1,(Vec2){p.x+50,p.y+50});
    CHECK(!a->game.hero.attacking && !a->game.hero.skilling); /* Ambiguous diagonals cancel. */
    pointer_down(a,1,p); pointer_move(a,1,(Vec2){p.x+8,p.y+4}); pointer_up(a,1,p);
    CHECK(a->game.hero.attacking); /* Finger jitter is still a tap. */
    p=attack_fixture(a); pointer_down(a,1,p);
    for(int i=0;i<4;i++) combat_tick(a,.05f);
    CHECK(!a->game.hero.attacking);
    combat_tick(a,.05f); CHECK(a->game.hero.attacking && a->attack_repeated);
    for(int i=0;i<16;i++) { game_tick(&a->game,.05f,(Vec2){0}); combat_tick(a,.05f); }
    CHECK(a->game.attack_cooldown>.6f); /* Holding repeats only after the cooldown. */
    float elapsed=a->game.hero.attack_elapsed;
    pointer_up(a,1,p); CHECK(a->game.hero.attack_elapsed==elapsed && !a->attack_pointer_down);
    p=attack_fixture(a); pointer_down(a,20,(Vec2){100,220});
    pointer_move(a,20,(Vec2){135,220}); pointer_down(a,21,p);
    pointer_move(a,21,(Vec2){p.x,p.y-60}); pointer_up(a,21,(Vec2){p.x,p.y-60});
    CHECK(a->pointer_down && a->pointer_delta.x>0 && a->game.hero.skilling);
    pointer_up(a,20,(Vec2){135,220}); CHECK(!a->pointer_down);
    p=attack_fixture(a); pointer_down(a,1,p); dispatch(a,ACT_BAG,0);
    CHECK(!a->attack_pointer_down && a->game.phase==PH_BAG);
    pointer_up(a,1,p); dispatch(a,ACT_CLOSE,0); CHECK(!a->game.hero.attacking);
    key(a,SDLK_z,false); CHECK(a->attack_key_down && a->game.hero.attacking);
    key_up(a,SDLK_z); CHECK(!a->attack_key_down);
    const SDL_Keycode keys[]={SDLK_q,SDLK_r,SDLK_g,SDLK_t};
    for(int d=0;d<SWIPE_COUNT;d++) {
        attack_fixture(a); key(a,keys[d],false);
        CHECK(a->game.hero.skilling && a->game.hero.technique==a->game.swipe_bindings[d]);
    }
    attack_fixture(a); key(a,SDLK_k,false); CHECK(a->game.phase==PH_TECHNIQUES);
    dispatch(a,ACT_SWIPE_SELECT,SWIPE_UP); dispatch(a,ACT_TECH_BIND,TECH_THRUST);
    CHECK(a->game.swipe_bindings[SWIPE_UP]==TECH_THRUST);
    key(a,SDLK_ESCAPE,false); key(a,SDLK_q,false);
    CHECK(a->game.hero.technique==TECH_THRUST && a->game.hero.skilling);
    attack_fixture(a); key(a,SDLK_z,false);
    SDL_Event e={0}; e.type=SDL_WINDOWEVENT; e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    SDL_PushEvent(&e); handle_events(a);
    CHECK(a->game.phase==PH_PAUSED && !a->attack_key_down);
    attack_fixture(a); a->height=900; pointer_down(a,1,p);
    CHECK(!a->attack_pointer_down && !a->pointer_down);
    key(a,SDLK_z,false); key(a,SDLK_q,false);
    CHECK(!a->game.hero.attacking && !a->game.hero.skilling);
    key_up(a,SDLK_z);
    attack_fixture(a);
}
static void bag_actions(App *a) {
    attack_fixture(a); dispatch(a,ACT_BAG,0);
    CHECK(a->game.phase==PH_BAG && a->bag_tab==0);
    BagEntry entries[8]; CHECK(bag_entries(&a->game,0,entries)==2);
    a->game.owned_sword=true; CHECK(bag_entries(&a->game,0,entries)==3);
    dispatch(a,ACT_BAG_SELECT,ITEM_SWORD); dispatch(a,ACT_EQUIP,ITEM_SWORD);
    CHECK(a->game.sword && a->bag_selected==ITEM_SWORD);
    dispatch(a,ACT_EQUIP,ITEM_SWORD); CHECK(!a->game.sword && a->game.owned_sword);
    dispatch(a,ACT_GEAR_SELECT,1); CHECK(a->bag_tab==0 && a->bag_selected==201);
    dispatch(a,ACT_BAG_TAB,1); CHECK(a->bag_tab==1 && bag_entries(&a->game,1,entries)==2);
    a->game.hero.hp=40; dispatch(a,ACT_USE,ITEM_TONIC);
    CHECK(a->game.hero.hp==85 && a->game.phase==PH_BAG && a->game.tonics==1);
    CHECK(bag_entries(&a->game,2,entries)==0);
    a->game.herbs=2; CHECK(bag_entries(&a->game,3,entries)==1 && entries[0].count==2);
    dispatch(a,ACT_CLOSE,0);
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
    combat_inputs(a);
    bag_actions(a);
    free(a);
    SDL_Quit();
    printf("PASS: %d controls checks: dialogue, manual attack, four swipes, holds, multitouch, remapping and inventory.\n", checks);
    return 0;
}
