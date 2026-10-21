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
    /* A release can carry the only motion. Judge the final point with the
     * same 8-pixel rule: a coalesced drag never becomes a destination tap. */
    a->target_set = false;
    pointer_down(a, -1, tap);
    pointer_up(a, -1, (Vec2){tap.x + 80, tap.y});
    CHECK(!a->target_set && !a->pointer_down && a->pointer_delta.x == 0);
    pointer_down(a, 7, tap);
    pointer_up(a, 7, (Vec2){tap.x + 9, tap.y});
    CHECK(!a->target_set && !a->pointer_down);
    pointer_down(a, 7, tap);
    pointer_up(a, 7, (Vec2){tap.x + 6, tap.y});
    CHECK(a->target_set && !a->pointer_down); /* Jitter within 8 px is still a tap. */
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
        CHECK(a->attack_swiping && !a->game.hero.attacking && a->game.hero.skilling);
        CHECK(a->attack_pointer_down && a->game.hero.technique==a->game.swipe_bindings[d]);
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
static void combat_frames(App *a, int count);
/* Holding Z repeats the basic attack, but never replaces a technique the
 * player queued explicitly during the current move. */
static void held_attack_keeps_queued_technique(App *a) {
    attack_fixture(a); key(a,SDLK_z,false);
    CHECK(a->attack_key_down && a->game.hero.attacking);
    combat_frames(a,10);
    key(a,SDLK_r,false);
    CHECK(a->game.hero.skilling && a->game.hero.technique==TECH_SWEEP);
    key(a,SDLK_q,false);
    CHECK(a->game.queued_move==TECH_CLEAVE);
    for(int i=0;i<6;i++) {
        combat_frames(a,1);
        CHECK(a->game.queued_move==TECH_CLEAVE);
    }
    CHECK(a->game.attack_cooldown==0); /* The repeat was eligible but held back. */
    for(int i=0;i<20 && a->game.hero.technique!=TECH_CLEAVE;i++) combat_frames(a,1);
    CHECK(a->game.hero.skilling && a->game.hero.technique==TECH_CLEAVE);
    /* With nothing queued, the held key still queues and resumes basic attacks. */
    combat_frames(a,1); CHECK(a->game.queued_move==MOVE_ATTACK);
    for(int i=0;i<20 && !a->game.hero.attacking;i++) combat_frames(a,1);
    CHECK(a->game.hero.attacking && !a->game.hero.skilling);
    key_up(a,SDLK_z); CHECK(!a->attack_key_down);
}
/* The rotate notice hides the game: shortcuts must not change it unseen. */
static void portrait_blocks_shortcuts(App *a) {
    game_reset(&a->game); stop_input(a);
    a->width=390; a->height=844;
    key(a,SDLK_SPACE,false); CHECK(a->game.phase==PH_TITLE);
    a->game.phase=PH_PROLOGUE; a->story_index=0;
    key(a,SDLK_SPACE,false); key(a,SDLK_RETURN,false);
    CHECK(a->game.phase==PH_PROLOGUE && a->story_index==0);
    game_start(&a->game);
    a->game.hero.pos=NPCS[ELDER].pos; a->game.hero.hp=40;
    a->game.hero.bleed=3; a->game.quest=Q_RECOVER;
    a->game.drops[0]=(Drop){.pos=NPCS[ELDER].pos,.exists=true};
    const SDL_Keycode keys[]={SDLK_SPACE,SDLK_RETURN,SDLK_e,SDLK_f,SDLK_1,SDLK_2,SDLK_i,SDLK_j,
                              SDLK_k,SDLK_z,SDLK_q,SDLK_r,SDLK_g,SDLK_t,SDLK_ESCAPE,SDLK_F8};
    for(size_t i=0;i<sizeof keys/sizeof *keys;i++) {
        key(a,keys[i],false);
        CHECK(a->game.phase==PH_PLAY && a->game.hero.hp==40 && a->game.tonics==2);
        CHECK(a->game.bandages==1 && a->game.herbs==0 && !a->game.training.active);
        CHECK(!a->attack_key_down && !a->game.hero.attacking && !a->game.hero.skilling);
        CHECK(a->game.cooldown==0 && a->game.queued_move==MOVE_NONE);
    }
    key_up(a,SDLK_z);
    /* Display-only keys still work, and rotating back restores shortcuts. */
    key(a,SDLK_F3,false); CHECK(a->debug_collision); key(a,SDLK_F3,false);
    a->width=844; a->height=390;
    key(a,SDLK_1,false); CHECK(a->game.hero.hp==85 && a->game.tonics==1);
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
static void touch(App *a, Uint32 type, Sint64 id, Vec2 p) {
    SDL_Event e={0}; e.type=type;
    e.tfinger.touchId=1; e.tfinger.fingerId=id;
    e.tfinger.x=p.x/a->width; e.tfinger.y=p.y/a->height;
    CHECK(SDL_PushEvent(&e)==1);
    handle_events(a);
}
static void combat_frames(App *a, int count) {
    for(int i=0;i<count;i++) {
        combat_tick(a,.05f);
        game_tick(&a->game,.05f,input(a,.05f));
    }
}
static void continuous_swipes(App *a) {
    Vec2 p=attack_fixture(a), move={100,220}, up={p.x,p.y-100};
    touch(a,SDL_FINGERDOWN,20,move);
    touch(a,SDL_FINGERMOTION,20,(Vec2){135,220});
    touch(a,SDL_FINGERDOWN,21,p);
    touch(a,SDL_FINGERMOTION,21,up);
    CHECK(a->attack_pointer_down && a->game.hero.skilling && a->game.hero.technique==TECH_CLEAVE);
    CHECK(a->pointer_down && a->pointer_delta.x>0);
    Vec2 before=a->game.hero.pos;
    combat_frames(a,9);
    CHECK(a->game.hero.pos.x>before.x && world_clear(a->game.hero.pos,HERO_RADIUS));
    /* Continue upward before reversing: the down stroke stays above the button. */
    up.y-=40; touch(a,SDL_FINGERMOTION,21,up);
    touch(a,SDL_FINGERMOTION,99,(Vec2){p.x-90,p.y});
    touch(a,SDL_FINGERUP,99,p);
    CHECK(a->attack_pointer_down && a->game.queued_move==MOVE_NONE);
    Vec2 down={up.x,up.y+40};
    touch(a,SDL_FINGERMOTION,21,down);
    CHECK(a->game.queued_move==TECH_RETREAT && a->game.hero.technique==TECH_CLEAVE);
    combat_frames(a,4);
    CHECK(a->game.hero.skilling && a->game.hero.technique==TECH_RETREAT && a->game.input_count==2);
    combat_frames(a,9);
    Vec2 left={down.x-40,down.y};
    touch(a,SDL_FINGERMOTION,21,left);
    CHECK(a->game.queued_move==TECH_THRUST);
    combat_frames(a,4);
    CHECK(a->attack_pointer_down && a->game.hero.technique==TECH_THRUST && a->game.input_count==3);
    touch(a,SDL_FINGERUP,20,move);
    CHECK(!a->pointer_down && a->attack_pointer_down);
    combat_frames(a,70); /* Cooldowns elapse without held swipe repeating or becoming Z. */
    touch(a,SDL_FINGERMOTION,21,(Vec2){left.x-40,left.y});
    touch(a,SDL_FINGERUP,21,(Vec2){left.x-40,left.y});
    CHECK(!a->game.hero.skilling && !a->game.hero.attacking && a->game.queued_move==MOVE_NONE);
    CHECK(game_technique_cooldown(&a->game,TECH_THRUST)==0);

    /* Last valid input wins, rejected directions cannot erase it or retry later. */
    p=attack_fixture(a); up=(Vec2){p.x,p.y-60};
    touch(a,SDL_FINGERDOWN,21,p); touch(a,SDL_FINGERMOTION,21,up);
    touch(a,SDL_FINGERMOTION,21,p); /* Early down now waits for the next action. */
    CHECK(a->game.queued_move==TECH_RETREAT);
    combat_frames(a,9);
    touch(a,SDL_FINGERMOTION,21,(Vec2){p.x,p.y+35}); /* Same direction is not re-requested. */
    CHECK(a->game.queued_move==TECH_RETREAT);
    Vec2 right={p.x+40,p.y+35}, back={p.x,p.y+35};
    touch(a,SDL_FINGERMOTION,21,right);
    CHECK(a->game.queued_move==TECH_SWEEP);
    touch(a,SDL_FINGERMOTION,21,back);
    CHECK(a->game.queued_move==TECH_THRUST);
    touch(a,SDL_FINGERMOTION,21,(Vec2){back.x,back.y-40}); /* Cleave still cooling down. */
    CHECK(a->game.queued_move==TECH_THRUST);
    touch(a,SDL_FINGERUP,21,(Vec2){back.x,back.y-40});
    combat_frames(a,4);
    CHECK(a->game.hero.technique==TECH_THRUST && a->game.input_count==2);
    combat_frames(a,120);
    CHECK(!a->game.hero.skilling && a->game.queued_move==MOVE_NONE);
}
/* Touch direction transitions are not restricted to up/down or to
 * opposites. Each ordered pair gets its own real held-pointer stroke. */
static void all_direction_pairs(App *a) {
    const Vec2 strokes[]={{0,-60},{60,0},{0,60},{-60,0}};
    for(int first=0;first<SWIPE_COUNT;first++) {
        for(int next=0;next<SWIPE_COUNT;next++) {
            if(first==next) continue;
            Vec2 p=attack_fixture(a);
            Vec2 first_point={p.x+strokes[first].x,p.y+strokes[first].y};
            Vec2 second_point={first_point.x+strokes[next].x,
                               first_point.y+strokes[next].y};
            pointer_down(a,1,p);
            pointer_move(a,1,first_point);
            CHECK(a->attack_pointer_down && a->game.hero.skilling);
            CHECK(a->game.hero.technique==a->game.swipe_bindings[first]);
            pointer_move(a,1,second_point); /* Intentionally early (<0.22s elapsed). */
            CHECK(a->game.queued_move==a->game.swipe_bindings[next]);
            combat_frames(a,13);
            CHECK(a->game.hero.skilling && a->game.hero.technique==a->game.swipe_bindings[next]);
            CHECK(a->game.input_count==2 && a->game.queued_move==MOVE_NONE);
            pointer_up(a,1,second_point);
            CHECK(!a->attack_pointer_down && a->game.queued_move==MOVE_NONE);
        }
    }
}
static void swipe_thresholds(App *a) {
    Vec2 p=attack_fixture(a);
    pointer_down(a,1,p); pointer_move(a,1,(Vec2){p.x,p.y-31});
    CHECK(!a->game.hero.skilling);
    pointer_move(a,1,(Vec2){p.x,p.y-32});
    CHECK(a->game.hero.skilling && a->game.hero.technique==TECH_CLEAVE);
    combat_frames(a,9);
    for(int i=0;i<12;i++) {
        pointer_move(a,1,(Vec2){p.x+(i%2 ? -8 : 8),p.y-32+(i%3)*8});
    }
    CHECK(a->game.queued_move==MOVE_NONE);
    pointer_move(a,1,(Vec2){p.x+40,p.y+8}); /* 40/40 diagonal from tip. */
    CHECK(a->game.queued_move==MOVE_NONE);
    CHECK(a->game.combat_request==TRAIN_INVALID);
    pointer_move(a,1,(Vec2){p.x+44,p.y+8}); /* Still ambiguous (1.10 ratio). */
    CHECK(a->game.queued_move==MOVE_NONE);
    pointer_move(a,1,(Vec2){p.x,p.y-32});
    /* Small forward drift cannot continually reset a predominantly lateral turn. */
    for(int i=1;i<=9;i++) pointer_move(a,1,(Vec2){p.x-i*4,p.y-32-i*.25f});
    CHECK(a->game.queued_move==TECH_THRUST);
    pointer_up(a,1,(Vec2){p.x-36,p.y-34.25f});
    CHECK(a->game.queued_move==TECH_THRUST && !a->game.hero.attacking);

    p=attack_fixture(a); pointer_down(a,1,p);
    pointer_move(a,1,(Vec2){p.x+50,p.y+50});
    combat_frames(a,8); pointer_move(a,1,p); pointer_up(a,1,p);
    CHECK(!a->game.hero.skilling && !a->game.hero.attacking); /* No diagonal-to-tap fallback. */
    p=attack_fixture(a); pointer_down(a,1,p);
    pointer_up(a,1,(Vec2){p.x,p.y-60}); /* Coalesced final motion still works. */
    CHECK(a->game.hero.skilling && a->game.hero.technique==TECH_CLEAVE);

    p=attack_fixture(a); pointer_down(a,1,p); combat_frames(a,10);
    CHECK(a->game.hero.attacking && a->attack_repeated);
    pointer_move(a,1,(Vec2){p.x+60,p.y});
    CHECK(a->game.queued_move==TECH_SWEEP);
    combat_frames(a,4);
    CHECK(a->game.hero.skilling && a->game.hero.technique==TECH_SWEEP);
    combat_frames(a,40);
    CHECK(!a->game.hero.attacking && !a->game.hero.skilling);
    pointer_up(a,1,(Vec2){p.x+60,p.y});
    CHECK(!a->game.hero.attacking);

    p=attack_fixture(a); key(a,SDLK_k,false);
    dispatch(a,ACT_SWIPE_SELECT,SWIPE_UP); dispatch(a,ACT_TECH_BIND,TECH_THRUST);
    key(a,SDLK_ESCAPE,false);
    touch(a,SDL_FINGERDOWN,1,p); touch(a,SDL_FINGERMOTION,1,(Vec2){p.x,p.y-60});
    CHECK(a->game.hero.technique==TECH_THRUST);
    touch(a,SDL_FINGERUP,1,(Vec2){p.x,p.y-60});
}
static void swipe_cancellation(App *a) {
    /* Exercise real menu/dialogue, focus/background and resize cancellation paths. */
    for(int reason=0;reason<8;reason++) {
        Vec2 p=attack_fixture(a), up={p.x,p.y-60};
        touch(a,SDL_FINGERDOWN,20,(Vec2){100,220});
        touch(a,SDL_FINGERMOTION,20,(Vec2){135,220});
        touch(a,SDL_FINGERDOWN,21,p); touch(a,SDL_FINGERMOTION,21,up);
        combat_frames(a,9); touch(a,SDL_FINGERMOTION,21,p);
        CHECK(a->game.queued_move==TECH_RETREAT);
        if(reason==0) key(a,SDLK_i,false);
        else if(reason==1) { a->game.hero.pos=NPCS[ELDER].pos; key(a,SDLK_SPACE,false); }
        else if(reason==2) key(a,SDLK_ESCAPE,false);
        else if(reason==3) key(a,SDLK_k,false);
        else if(reason==4) key(a,SDLK_j,false);
        else {
            SDL_Event e={0};
            e.type=reason==6 ? SDL_APP_WILLENTERBACKGROUND : SDL_WINDOWEVENT;
            e.window.event=reason==7 ? SDL_WINDOWEVENT_SIZE_CHANGED : SDL_WINDOWEVENT_FOCUS_LOST;
            if(reason==7) SDL_SetWindowSize(a->window,390,844);
            SDL_PushEvent(&e); handle_events(a);
        }
        CHECK(!a->attack_pointer_down && !a->pointer_down && !a->attack_key_down);
        CHECK(a->attack_direction==-1 && !a->attack_swiping && a->attack_hold==0);
        CHECK(a->game.queued_move==MOVE_NONE && a->game.buffer_time==0);
        if(reason==7) {
            CHECK(a->width<a->height);
            SDL_SetWindowSize(a->window,844,390); resize(a);
        }
        a->game.phase=PH_PLAY;
        touch(a,SDL_FINGERMOTION,21,(Vec2){p.x-60,p.y});
        touch(a,SDL_FINGERUP,21,p); touch(a,SDL_FINGERUP,20,(Vec2){135,220});
        combat_frames(a,20);
        CHECK(!a->game.hero.skilling && !a->game.hero.attacking && !a->target_set);
        CHECK(game_technique_cooldown(&a->game,TECH_RETREAT)==0);
    }
}
static void training_entry_and_inputs(App *app) {
    game_reset(&app->game); stop_input(app);
    key(app, SDLK_F8, false);
    CHECK(!app->game.training.active);
    game_start(&app->game);
    key(app, SDLK_F8, true);
    CHECK(!app->game.training.active);
    Vec2 title = {60, 18};
    for (int tap = 0; tap < 6; tap++) {
        touch(app, SDL_FINGERDOWN, 71, title);
        touch(app, SDL_FINGERUP, 71, title);
    }
    CHECK(!app->game.training.active && !app->target_set);
    app->secret_last = SDL_GetTicks64() - 401;
    touch(app, SDL_FINGERDOWN, 71, title); touch(app, SDL_FINGERUP, 71, title);
    CHECK(app->secret_taps == 1 && !app->game.training.active);
    touch(app, SDL_FINGERDOWN, 71, title);
    touch(app, SDL_FINGERMOTION, 71, (Vec2){80, 18}); touch(app, SDL_FINGERUP, 71, title);
    CHECK(app->secret_taps == 0);
    for (int tap = 0; tap < 7; tap++) {
        touch(app, SDL_FINGERDOWN, 71, title); touch(app, SDL_FINGERUP, 71, title);
    }
    CHECK(app->game.training.active && !app->pointer_down && !app->attack_pointer_down);
    touch(app, SDL_FINGERDOWN, 72, (Vec2){60, 90}); touch(app, SDL_FINGERUP, 72, (Vec2){60, 90});
    CHECK(!app->target_set && !app->pointer_down);
    key(app, SDLK_i, false); key(app, SDLK_j, false); key(app, SDLK_SPACE, false); key(app, SDLK_f, false);
    CHECK(app->game.phase == PH_PLAY && app->game.quest == Q_MEET && app->game.herbs == 0);
    key(app, SDLK_z, false); key_up(app, SDLK_z);
    combat_frames(app, 4);
    CHECK(app->game.training.hits == 1 && app->game.training.last_damage == 9);
    key(app, SDLK_ESCAPE, false);
    CHECK(app->game.phase == PH_PAUSED);
    key(app, SDLK_F8, false);
    CHECK(!app->game.training.active && app->game.phase == PH_PLAY);
    CHECK(!app->attack_key_down && !app->pointer_down && app->game.queued_move == MOVE_NONE);

    const Vec2 strokes[] = {{0, -40}, {40, 0}, {0, 40}, {-40, 0}};
    for (int first = 0; first < SWIPE_COUNT; first++) {
        for (int second = 0; second < SWIPE_COUNT; second++) {
            if (first == second) continue;
            for (int third = 0; third < SWIPE_COUNT; third++) {
                if (third == first || third == second) continue;
                key(app, SDLK_F8, false);
                CHECK(app->game.training.active);
                app->button_count = 1;
                app->buttons[0] = (Button){{684, 230, 88, 88}, ACT_ATTACK, 0, true};
                Vec2 point = {728, 274};
                touch(app, SDL_FINGERDOWN, 81, point);
                point.x += strokes[first].x; point.y += strokes[first].y;
                touch(app, SDL_FINGERMOTION, 81, point);
                combat_frames(app, 2);
                point.x += strokes[second].x; point.y += strokes[second].y;
                touch(app, SDL_FINGERMOTION, 81, point);
                CHECK(app->game.queued_move == (int)app->game.swipe_bindings[second]);
                combat_frames(app, 14);
                point.x += strokes[third].x; point.y += strokes[third].y;
                touch(app, SDL_FINGERMOTION, 81, point);
                combat_frames(app, 28);
                touch(app, SDL_FINGERUP, 81, point);
                CHECK(app->game.training.hits == 3 && app->game.training.misses == 0);
                CHECK(app->game.combo_count == 3 && !app->game.hero.attacking);
                key(app, SDLK_F8, false);
                CHECK(!app->game.training.active && !app->attack_pointer_down);
            }
        }
    }
    for (int reason = 0; reason < 5; reason++) {
        key(app, SDLK_F8, false);
        key(app, SDLK_q, false); key(app, SDLK_r, false);
        CHECK(app->game.queued_move == TECH_SWEEP);
        app->game.hitstop_time = HITSTOP_LIGHT_SECONDS;
        app->game.hero.hurt_time = app->game.boss.hit_time = HIT_FLASH_SECONDS;
        if (reason == 0) key(app, SDLK_k, false);
        else if (reason == 1) key(app, SDLK_ESCAPE, false);
        else {
            SDL_Event event = {0};
            event.type = reason == 3 ? SDL_APP_WILLENTERBACKGROUND : SDL_WINDOWEVENT;
            event.window.event = reason == 4 ? SDL_WINDOWEVENT_SIZE_CHANGED : SDL_WINDOWEVENT_FOCUS_LOST;
            if (reason == 4) SDL_SetWindowSize(app->window, 390, 844);
            SDL_PushEvent(&event); handle_events(app);
        }
        CHECK(app->game.queued_move == MOVE_NONE && app->game.buffer_time == 0);
        CHECK(app->game.hitstop_time == 0 && app->game.hero.hurt_time == 0 && app->game.boss.hit_time == 0);
        CHECK(!app->attack_key_down && !app->attack_pointer_down && !app->pointer_down && app->secret_taps == 0);
        if (reason == 4) { SDL_SetWindowSize(app->window, 844, 390); resize(app); }
        dispatch(app, ACT_TITLE, 0);
        CHECK(!app->game.training.active && app->game.phase == PH_PLAY);
    }
}
static void hitstop_swipe_inputs(App *app) {
    attack_fixture(app); key(app, SDLK_F8, false);
    CHECK(app->game.training.active);
    app->button_count = 1;
    app->buttons[0] = (Button){{684,230,88,88}, ACT_ATTACK, 0, true};
    Vec2 point = {728,274};
    touch(app, SDL_FINGERDOWN, 91, point);
    point.y -= 40; touch(app, SDL_FINGERMOTION, 91, point);
    for (int frame = 0; frame < 20 && !app->game.hero.skill_hit; frame++) combat_frames(app, 1);
    CHECK(app->game.training.hits == 1 && app->game.hitstop_time > 0);
    float elapsed = app->game.hero.skill_elapsed;
    Vec2 position = app->game.hero.pos;
    point.x += 40; touch(app, SDL_FINGERMOTION, 91, point);
    CHECK(app->game.queued_move == TECH_SWEEP && app->attack_pointer_down);
    point.x -= 40; touch(app, SDL_FINGERMOTION, 91, point);
    CHECK(app->game.queued_move == TECH_THRUST);
    key(app, SDLK_q, false);
    CHECK(app->game.combat_request == TRAIN_COOLDOWN && app->game.queued_move == TECH_THRUST);
    game_tick(&app->game, .01f, (Vec2){1,0});
    CHECK(app->game.hero.skill_elapsed == elapsed && distance(position, app->game.hero.pos) == 0);
    touch(app, SDL_FINGERUP, 91, point);
    CHECK(!app->attack_pointer_down && app->game.queued_move == TECH_THRUST);
    combat_frames(app, 40);
    CHECK(app->game.training.hits == 2 && app->game.training.misses == 0);
    CHECK(app->game.hero.technique == TECH_THRUST && app->game.queued_move == MOVE_NONE);
    key(app, SDLK_F8, false);
    CHECK(!app->game.training.active && app->game.hitstop_time == 0);
}
static void eight_direction_movement(App *a) {
    const Vec2 dirs[]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    for(int mode=0;mode<2;mode++) for(int d=0;d<HERO_DIRECTIONS;d++) {
        game_reset(&a->game);game_start(&a->game);stop_input(a);
        a->width=844;a->height=390;a->density=1;a->button_count=0;
        a->camera=world_camera(a->width/WORLD_ZOOM,a->height/WORLD_ZOOM,HOME);
        if(mode==0) {
            Vec2 start={180,230},end={start.x+dirs[d].x*60,start.y+dirs[d].y*60};
            touch(a,SDL_FINGERDOWN,101,start);touch(a,SDL_FINGERMOTION,101,end);
            game_tick(&a->game,.05f,input(a,.05f));
            CHECK(a->game.hero.walking && gfx_hero_direction(a->game.hero.dir)==(HeroDirection)d);
            touch(a,SDL_FINGERUP,101,end);
            game_tick(&a->game,.05f,input(a,.05f));
        } else {
            Vec2 target={HOME.x+dirs[d].x*20,HOME.y+dirs[d].y*20};
            Vec2 tap={(target.x-a->camera.x)*WORLD_ZOOM,(target.y-a->camera.y)*WORLD_ZOOM};
            touch(a,SDL_FINGERDOWN,102,tap);touch(a,SDL_FINGERUP,102,tap);
            CHECK(a->target_set);
            game_tick(&a->game,.05f,input(a,.05f));
            CHECK(a->game.hero.walking && gfx_hero_direction(a->game.hero.dir)==(HeroDirection)d);
            for(int tick=0;tick<20;tick++) game_tick(&a->game,.05f,input(a,.05f));
            CHECK(!a->target_set && distance(a->game.hero.pos,target)<1);
        }
        CHECK(!a->game.hero.walking && gfx_hero_direction(a->game.hero.dir)==(HeroDirection)d);
        dispatch(a,ACT_PAUSE,0);
        game_tick(&a->game,.05f,input(a,.05f));
        CHECK(!a->game.hero.walking && gfx_hero_direction(a->game.hero.dir)==(HeroDirection)d);
    }
}
/* The view eases after the hero, snaps on teleports, shakes only on real
 * hits, stays on the map and lands on whole device pixels. */
static void camera_follow_and_shake(App *a) {
    game_reset(&a->game); game_start(&a->game);
    a->view_ready = false; a->trauma = 0; a->shake_clock = 0;
    float w = a->width / WORLD_ZOOM, h = a->height / WORLD_ZOOM, scale = 1.5f * WORLD_ZOOM;
    follow_view(a, .016f);
    CHECK(a->view_ready && distance(a->view_focus, a->game.hero.pos) < .001f);
    a->game.hero.pos.x += 40;
    follow_view(a, .016f);
    float gap = distance(a->view_focus, a->game.hero.pos);
    CHECK(gap > 20 && gap < 40);
    for (int i = 0; i < 60; i++) follow_view(a, .016f);
    CHECK(distance(a->view_focus, a->game.hero.pos) < .5f);
    a->game.hero.walking = true; a->game.hero.dir = (Vec2){1, 0};
    for (int i = 0; i < 90; i++) follow_view(a, .016f);
    CHECK(a->view_focus.x > a->game.hero.pos.x + 15);
    a->game.hero.walking = false;
    a->game.hero.pos = (Vec2){BOSS_HOME.x, BOSS_HOME.y + 45};
    follow_view(a, .016f);
    CHECK(distance(a->view_focus, a->game.hero.pos) < .001f);
    Vec2 still = view_camera(a, w, h, scale);
    CHECK(fabsf(still.x * scale - roundf(still.x * scale)) < .001f);
    CHECK(fabsf(still.y * scale - roundf(still.y * scale)) < .001f);
    float marks[2 + BOAR_COUNT];
    hit_marks(&a->game, marks);
    add_shake(a, marks, .016f);
    CHECK(a->trauma == 0);
    hit_marks(&a->game, marks);
    a->game.hero.hurt_time = HIT_FLASH_SECONDS;
    add_shake(a, marks, .016f);
    CHECK(a->trauma > .5f);
    float moved = 0;
    for (int i = 0; i < 8; i++) {
        a->shake_clock += .013f;
        Vec2 c = view_camera(a, w, h, scale);
        moved = fmaxf(moved, distance(c, still));
        CHECK(distance(c, still) < 12);
    }
    CHECK(moved > .5f);
    hit_marks(&a->game, marks);
    for (int i = 0; i < 60; i++) add_shake(a, marks, .016f);
    CHECK(a->trauma == 0);
    /* At the map corner the shake never shows the void past the painting. */
    a->game.hero.pos = (Vec2){30, 30}; a->view_ready = false; follow_view(a, .016f);
    a->trauma = 1;
    for (int i = 0; i < 40; i++) {
        a->shake_clock += .011f;
        Vec2 c = view_camera(a, w, h, scale);
        CHECK(c.x >= 0 && c.y >= 0 && c.x <= WORLD_W - w && c.y <= WORLD_H - h);
    }
    a->trauma = 0; a->view_ready = false;
}
int main(void) {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 1;
    App *a = calloc(1, sizeof *a);
    if (!a) return 1;
    a->window=SDL_CreateWindow("controls",0,0,844,390,SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if(!a->window) return 1;
    a->renderer=SDL_CreateRenderer(a->window,-1,SDL_RENDERER_SOFTWARE);
    if(!a->renderer) return 1;
    a->width = 844; a->height = 390; a->density = 1;
    conversation(a);
    pickup(a);
    zoomed_destination(a);
    combat_inputs(a);
    held_attack_keeps_queued_technique(a);
    portrait_blocks_shortcuts(a);
    continuous_swipes(a);
    all_direction_pairs(a);
    swipe_thresholds(a);
    swipe_cancellation(a);
    bag_actions(a);
    training_entry_and_inputs(a);
    hitstop_swipe_inputs(a);
    eight_direction_movement(a);
    camera_follow_and_shake(a);
    SDL_DestroyRenderer(a->renderer);
    SDL_DestroyWindow(a->window);
    free(a);
    SDL_Quit();
    printf("PASS: %d controls checks: dialogue, continuous swipes, timing, jitter, holds, multitouch, cancellation, remapping and inventory.\n", checks);
    return 0;
}
