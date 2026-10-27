#include "game.h"
#include "world.h"
#include "test_simulation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static int checks;
#define CHECK(c) do { checks++; if(!(c)) { fprintf(stderr,"FAIL maps:%d: %s\n",__LINE__,#c);exit(1); } } while(0)
static void walk(Game *g, Vec2 to) {
    world_select(g->map);Route r;
    CHECK(world_route(g->hero.pos,to,HERO_RADIUS,&r));
    CHECK(distance(r.points[r.count-1],to)<.2f);
    int steps=0;
    while(r.cursor<r.count && steps++<12000) {
        Vec2 v=world_follow(&r,g->hero.pos);
        float left=r.cursor<r.count?distance(g->hero.pos,r.points[r.cursor]):0;
        float strength=game_arrival_strength(g,v,left,.05f);
        simulation_tick(g,.05f,(Vec2){v.x*strength,v.y*strength});
        CHECK(g->phase==PH_PLAY);CHECK(world_clear(g->hero.pos,HERO_RADIUS));
    }
    CHECK(steps<12000 && distance(g->hero.pos,to)<2.1f);
    for(int i=0;i<10;i++) simulation_tick(g,.01f,(Vec2){0});
}
static void travel(Game *g,int i) {
    CHECK(MAP_EXITS[i].from==g->map);walk(g,MAP_EXITS[i].pos);
    CHECK(game_nearby(g).kind==NEAR_EXIT);CHECK(game_travel(g,i));
    CHECK(g->map==MAP_EXITS[i].to && distance(g->hero.pos,MAP_EXITS[i].arrival)==0);
    CHECK(game_exit_nearby(g)<0);
}
static void routes(void) {
    Game g;game_reset(&g);game_start(&g);CHECK(g.map==MAP_VILLAGE);CHECK(!world_clear((Vec2){825*1.5f,300*1.5f},HERO_RADIUS));
    for(int i=0;i<NPC_COUNT;i++) { walk(&g,NPCS[i].pos);walk(&g,HOME); }
    for(int c=0;c<2;c++) {
        travel(&g,0);
        for(int i=0;i<TRACE_COUNT;i++) walk(&g,TRACES[i].pos);
        for(int i=0;i<BOAR_COUNT;i++) walk(&g,g.boars[i].pos);
        travel(&g,2);travel(&g,1);walk(&g,CAVE_HOME);travel(&g,3);
    }
    Route r;
    const Vec2 blocked[]={{300,270},{1140,675},{150,675}};
    const Vec2 starts[]={HOME,{1152,1260},{1152,1275}};
    for(int c=0;c<3;c++) for(int m=0;m<3;m++) {
        world_select((MapId)m);world_prepare();CHECK(!world_clear(blocked[m],HERO_RADIUS));
        CHECK(world_route(starts[m],blocked[m],HERO_RADIUS,&r));
        CHECK(world_clear(r.points[r.count-1],HERO_RADIUS));
        CHECK(!world_clear((Vec2){NAN,0},HERO_RADIUS));
        for(int i=0;i<4;i++) if(MAP_EXITS[i].from==m) CHECK(world_clear(MAP_EXITS[i].pos,HERO_RADIUS));
    }
}
static void persistence(void) {
    Game g;game_reset(&g);game_start(&g);
    CHECK(!game_travel(&g,0) && !game_travel(&g,-1) && !game_travel(&g,4));
    g.hero.pos=MAP_EXITS[0].pos;CHECK(!game_travel(&g,1));
    const Phase blocked[]={PH_BAG,PH_DIALOGUE,PH_DEAD,PH_PAUSED,PH_TECHNIQUES};
    for(unsigned i=0;i<sizeof blocked/sizeof *blocked;i++) { g.phase=blocked[i];CHECK(!game_travel(&g,0)); }
    g.phase=PH_PLAY;g.quest=Q_COMBAT;CHECK(!game_travel(&g,0));g.quest=Q_RECOVER;
    g.traces[0]=g.traces[1]=true;g.hero.hp=37;g.hero.bleed=4;
    g.cooldown=2.5f;g.item_cooldown=.7f;g.attack_cooldown=.3f;g.technique_cooldowns[TECH_THRUST]=1.7f;
    g.coins=123;g.herbs=1;g.tonics=4;g.bandages=2;g.augment=AUG_WIND;
    g.owned_sword=g.sword=g.owned_armor=g.armor=true;
    g.boars[0].hp=0;g.boars[0].state=AI_DEAD;g.boars[1].hp=17;
    g.drops[0]=(Drop){.exists=true,.collected=true,.pos=g.boars[0].pos,.age=3};
    g.drops[1]=(Drop){.exists=true,.pos=g.boars[1].pos,.age=2,.attracting=true};
    g.hero.attacking=g.hero.skilling=true;g.queued_move=TECH_SWEEP;g.hero.vel=(Vec2){20,10};g.effect_count=1;
    CHECK(game_travel(&g,0));
    CHECK(g.hero.hp==37 && g.hero.bleed==4 && g.cooldown==2.5f && g.item_cooldown==.7f);
    CHECK(g.attack_cooldown==.3f && g.technique_cooldowns[TECH_THRUST]==1.7f);
    CHECK(g.coins==123 && g.herbs==1 && g.tonics==4 && g.bandages==2);
    CHECK(g.sword && g.armor && g.owned_sword && g.owned_armor && g.augment==AUG_WIND);
    CHECK(!g.hero.attacking && !g.hero.skilling && g.queued_move==MOVE_NONE && g.effect_count==0);
    CHECK(g.hero.vel.x==0 && g.hero.vel.y==0);
    CHECK(g.boars[0].hp==0 && g.boars[1].hp==17 && g.drops[0].collected && g.drops[1].exists);
    CHECK(!g.drops[1].attracting && g.drops[1].age==2);
    g.hero.pos=MAP_EXITS[2].pos;CHECK(game_travel(&g,2));CHECK(!game_boars_active(&g) && game_pickup_count(&g)==0);
    Vec2 drop_pos=g.drops[1].pos;float drop_age=g.drops[1].age;g.hero.pos=drop_pos;
    for(int i=0;i<10;i++) simulation_tick(&g,.05f,(Vec2){0});
    CHECK(g.drops[1].age==drop_age && distance(g.drops[1].pos,drop_pos)==0 && !g.drops[1].attracting);
    g.hero.pos=TRACES[0].pos;g.quest=Q_TRACES;g.traces[0]=false;CHECK(!game_inspect(&g,0));
    g.traces[0]=true;g.quest=Q_RECOVER;g.hero.pos=MAP_EXITS[0].pos;CHECK(game_travel(&g,0));
    CHECK(g.boars[0].hp==0 && g.boars[1].hp==17 && g.coins==123);
    g.hero.pos=NPCS[SMITH].pos;CHECK(!game_open_shop(&g,SMITH));g.quest=Q_HUNT;CHECK(!game_begin_boss(&g));
}
static void quest_and_recovery(void) {
    Game g;game_reset(&g);game_start(&g);g.hero.pos=NPCS[ELDER].pos;CHECK(game_accept(&g));travel(&g,0);
    for(int i=0;i<2;i++) { walk(&g,TRACES[i].pos);CHECK(game_inspect(&g,i)); }
    CHECK(g.quest==Q_RECOVER);g.hero.invul=1000;
    for(int i=0;i<3;i++) {
        g.hero.pos=g.boars[i].pos;CHECK(game_damage(&g,&g.boars[i],99,FX_HIT));
        CHECK(game_collect(&g,i));CHECK(!game_collect(&g,i));
    }
    CHECK(g.quest==Q_DELIVER && g.herbs==3 && g.coins==82);travel(&g,2);
    walk(&g,NPCS[HERBALIST].pos);CHECK(game_deliver(&g));walk(&g,NPCS[ELDER].pos);CHECK(game_report(&g));
    travel(&g,1);CHECK(!game_boars_active(&g));g.hero.pos=(Vec2){CAVE_HOME.x-100,CAVE_HOME.y};
    simulation_tick(&g,.05f,(Vec2){0});CHECK(g.phase==PH_DIALOGUE && (g.notices&NOTICE_BOSS));
    CHECK(game_begin_boss(&g));CHECK(game_damage(&g,&g.boss,240,FX_HIT));CHECK(g.quest==Q_RETURN && g.coins==207);
    g.notices=0;travel(&g,3);g.hero.pos=MAP_EXITS[1].pos;CHECK(game_travel(&g,1));
    CHECK(g.boss.hp==0 && g.boss_rewarded && g.coins==207);
    g.hero.pos=MAP_EXITS[3].pos;CHECK(game_travel(&g,3));g.phase=PH_AUGMENT;
    CHECK(game_choose(&g,AUG_STEEL) && g.quest==Q_MESSENGER);
    game_reset(&g);game_start(&g);g.hero.pos=MAP_EXITS[1].pos;CHECK(game_travel(&g,1));
    g.quest=Q_HUNT;CHECK(game_begin_boss(&g));g.boss.hp=19;g.hero.hp=0;g.phase=PH_DEAD;game_recover(&g);
    CHECK(g.map==MAP_VILLAGE && g.quest==Q_HUNT && g.hero.hp==100 && g.boss.hp==240 && distance(g.hero.pos,HOME)==0);
    g.hero.hp=0;g.phase=PH_DEAD;game_retry(&g);
    CHECK(g.map==MAP_CAVE && g.quest==Q_COMBAT && g.boss.hp==240 && g.hero.hp==100 && world_clear(g.hero.pos,HERO_RADIUS));
}
static void training(void) {
    for(int m=0;m<3;m++) {
        Game g,saved;game_reset(&g);game_start(&g);g.map=(MapId)m;g.hero.pos=m==0?HOME:MAP_EXITS[m==1?0:1].arrival;
        g.coins=321;g.hero.hp=49;g.cooldown=1.8f;MapId before=g.map;Vec2 p=g.hero.pos;
        CHECK(game_enter_training(&g,&saved));CHECK(g.map==MAP_TRAINING && g.training.active && !game_travel(&g,0));
        CHECK(game_leave_training(&g,&saved));CHECK(g.map==before && world_selected()==before && distance(p,g.hero.pos)==0);
        CHECK(g.coins==321 && g.hero.hp==49 && g.cooldown==1.8f);
    }
}
int main(void) { routes();persistence();quest_and_recovery();training();
    printf("PASS: %d map travel/navigation/quest/persistence checks.\n",checks);return 0; }
