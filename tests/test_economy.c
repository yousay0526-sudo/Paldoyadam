#include "save_store.h"
#include "world.h"
#include "test_simulation.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr,"FAIL economy:%d: %s\n",__LINE__,#c); exit(1); } } while(0)
static unsigned char bytes[SAVE_CAPACITY];
static size_t size;
static uint32_t next_rng(uint32_t x) { x^=x<<13; x^=x>>17; x^=x<<5; return x; }
static uint32_t seed_for(unsigned roll) {
    for (uint32_t x=1; x<1000000; x++)
        if (((uint64_t)next_rng(x)*100>>32)==roll) return x;
    CHECK(false); return 0;
}
static void ready(Game *g) {
    game_reset(g); game_start(g); g->map=MAP_PASS;
    g->quest=Q_RECOVER; g->traces[0]=g->traces[1]=true;
    g->hero.pos=HUNT_SPAWNS[0]; world_select(g->map);
}
static Game rt(const Game *g) {
    Game out; MapId selected=world_selected();
    CHECK(save_encode(g,bytes,&size)==SAVE_OK && size==SAVE_V4_SIZE);
    CHECK(save_decode(&out,bytes,size)==SAVE_OK && world_selected()==selected);
    return out;
}
static int piles(const Game *g) { int n=0; for(int i=0;i<MATERIAL_DROP_MAX;i++) n+=g->material_drops[i].quantity>0; return n; }
static void kill(Game *g, unsigned roll) {
    g->loot_rng=seed_for(roll);
    CHECK(game_damage(g,&g->hunt[0],100,FX_HIT));
}
static void visit(Game *g) {
    g->hero.pos=MAP_EXITS[2].pos; CHECK(game_travel(g,2));
    g->hero.pos=MAP_EXITS[0].pos; CHECK(game_travel(g,0));
}
static void tables_and_once(void) {
    int counts[3]={0};
    for(unsigned r=0;r<100;r++) { int k=game_material_roll(r); CHECK(k>=-1 && k<MATERIAL_COUNT); counts[k+1]++; }
    CHECK(counts[0]==45 && counts[1]==35 && counts[2]==20);
    CHECK(game_material_roll(100)==-1 && game_material_roll(UINT_MAX)==-1);
    CHECK(35*MATERIALS[0].price*MATERIALS[0].quantity+20*MATERIALS[1].price*MATERIALS[1].quantity==145);
    const unsigned boundaries[]={0,34,35,54,55,99};
    for(int b=0;b<6;b++) {
        Game g; ready(&g); int coins=g.coins; Drop quest[BOAR_COUNT]; memcpy(quest,g.drops,sizeof quest);
        g.loot_rng=seed_for(boundaries[b]); uint32_t seed=g.loot_rng;
        CHECK(game_damage(&g,&g.hunt[0],1,FX_HIT)); CHECK(g.loot_rng==seed && !piles(&g));
        g=rt(&g); kill(&g,boundaries[b]);
        CHECK(g.loot_rng==next_rng(seed));
        int k=game_material_roll(boundaries[b]);
        CHECK(piles(&g)==(k>=0));
        CHECK(g.quest==Q_RECOVER && g.herbs==0);
        for(int i=0;i<BOAR_COUNT;i++) CHECK(g.drops[i].exists==quest[i].exists && !g.drops[i].collected && g.boars[i].hp==BOAR_HP);
        CHECK(g.coins==coins+HUNT_REWARD);
        for(int n=0;n<4;n++) {
            g=rt(&g); CHECK(!game_damage(&g,&g.hunt[0],1,FX_HIT));
            CHECK(g.loot_rng==next_rng(seed) && piles(&g)==(k>=0) && g.coins==coins+HUNT_REWARD);
        }
        CHECK(g.materials[0]==0 && g.materials[1]==0);
        if(k>=0) {
            int i=k*MATERIAL_PILES_PER_KIND; g.hero.pos=g.material_drops[i].drop.pos;
            CHECK(game_collect_material(&g,i)); CHECK(g.materials[k]==1 && piles(&g)==0);
            CHECK(!game_collect_material(&g,i));
            g=rt(&g); CHECK(g.materials[k]==1 && piles(&g)==0);
            CHECK(g.quest==Q_RECOVER && g.herbs==0);
        }
    }
    Game g; ready(&g); uint32_t seed=g.loot_rng;
    CHECK(game_damage(&g,&g.boars[0],100,FX_HIT));
    CHECK(g.loot_rng==seed && piles(&g)==0 && g.drops[0].exists);
    g.hero.pos=g.drops[0].pos; CHECK(game_collect(&g,0));
    CHECK(g.herbs==1 && game_material_total(&g,MAT_LEATHER)==0);
}
static void persistence_and_capacity(void) {
    Game g; ready(&g); kill(&g,0); Vec2 old=g.material_drops[0].drop.pos;
    g.hero.pos=MAP_EXITS[0].arrival;
    for(int n=0;n<620;n++) simulation_tick(&g,.05f,(Vec2){0});
    CHECK(g.hunt[0].hp==BOAR_HP && g.material_drops[0].quantity==1);
    CHECK(distance(g.material_drops[0].drop.pos,old)==0);
    kill(&g,35); CHECK(piles(&g)==2 && g.material_drops[0].quantity==1);
    for(int n=0;n<MATERIAL_PILES_PER_KIND+3;n++) { visit(&g); kill(&g,0); }
    CHECK(piles(&g)==MATERIAL_PILES_PER_KIND+1);
    CHECK(game_material_total(&g,MAT_LEATHER)==MATERIAL_PILES_PER_KIND+4);
    CHECK(g.material_drops[0].quantity==5 && distance(g.material_drops[0].drop.pos,old)==0);
    Game loaded=rt(&g); CHECK(game_material_total(&loaded,MAT_BONE)==1);
    CHECK(game_material_total(&loaded,MAT_LEATHER)==MATERIAL_PILES_PER_KIND+4);
    /* Full per-kind quantity holds both acquired and uncollected resources. */
    ready(&g); g.materials[0]=MATERIAL_LIMIT-1; kill(&g,0);
    CHECK(game_material_total(&g,MAT_LEATHER)==MATERIAL_LIMIT && piles(&g)==1);
    visit(&g); kill(&g,0); CHECK(game_material_total(&g,MAT_LEATHER)==MATERIAL_LIMIT && piles(&g)==1);
    CHECK(strstr(g.toast,"한도"));
    g.hero.pos=g.material_drops[0].drop.pos; CHECK(game_collect_material(&g,0));
    CHECK(g.materials[0]==MATERIAL_LIMIT && piles(&g)==0); g=rt(&g);
    g.hero.pos=MAP_EXITS[2].pos; CHECK(game_travel(&g,2));
    g=rt(&g); CHECK(g.materials[0]==MATERIAL_LIMIT);
    /* Dead resume recovers to the village without destroying ground or bag. */
    ready(&g); kill(&g,0); g.materials[1]=4; g.hero.hp=0; g.phase=PH_DEAD;
    g=rt(&g); CHECK(g.map==MAP_VILLAGE && g.materials[1]==4 && piles(&g)==1);
}
static void collection(void) {
    Game g; ready(&g); kill(&g,0); g.hero.pos=(Vec2){HUNT_SPAWNS[0].x,HUNT_SPAWNS[0].y};
    for(int a=0;a<360;a+=10) {
        Vec2 p={HUNT_SPAWNS[0].x+100*cosf(a*.0174533f),HUNT_SPAWNS[0].y+100*sinf(a*.0174533f)};
        if(world_clear(p,HERO_RADIUS)&&world_segment(p,HUNT_SPAWNS[0],0)) { g.hero.pos=p; break; }
    }
    CHECK(distance(g.hero.pos,HUNT_SPAWNS[0])>99);
    g.hunt[0].dying=0; g.hitstop_time=0;
    Drop before=g.material_drops[0].drop;
    const Phase menus[]={PH_BAG,PH_SHOP,PH_DIALOGUE,PH_JOURNAL,PH_PAUSED,PH_TECHNIQUES};
    for(unsigned n=0;n<sizeof menus/sizeof *menus;n++) {
        g.phase=menus[n]; game_tick(&g,.05f,(Vec2){0});
        CHECK(!memcmp(&before,&g.material_drops[0].drop,sizeof before));
        CHECK(!game_collect_material(&g,0));
    }
    g.phase=PH_PLAY;
    for(int n=0;n<13;n++) game_tick(&g,.05f,(Vec2){0});
    CHECK(g.materials[0]==0 && distance(before.pos,g.material_drops[0].drop.pos)==0);
    for(int n=0;n<70;n++) game_tick(&g,.05f,(Vec2){0});
    CHECK(g.materials[0]==1 && piles(&g)==0 && g.herbs==0 && g.quest==Q_RECOVER);
    CHECK(!game_collect_material(&g,-1) && !game_collect_material(&g,MATERIAL_DROP_MAX));
    /* Existing wall test point: find a blocked segment within magnet range. */
    bool found=false; ready(&g);
    for(int y=120;y<1400 && !found;y+=20) for(int x=120;x<2100 && !found;x+=20) {
        Vec2 a={(float)x,(float)y},b={(float)x+120,(float)y};
        if(world_clear(a,HERO_RADIUS)&&world_clear(b,HERO_RADIUS)&&!world_segment(a,b,0)) {
            g.hero.pos=a; g.material_drops[0]=(MaterialDrop){.quantity=1,.drop={.pos=b,.exists=true,.age=DROP_REST_SECONDS}};
            CHECK(!game_collect_material(&g,0)); before=g.material_drops[0].drop;
            game_tick(&g,.05f,(Vec2){0}); CHECK(distance(before.pos,g.material_drops[0].drop.pos)==0);
            CHECK(!g.material_drops[0].drop.attracting && g.materials[0]==0); found=true;
        }
    }
    CHECK(found);
    /* Side notice is deferred and consolidated, never replaces combat toast. */
    ready(&g); kill(&g,0); g.hero.pos=g.material_drops[0].drop.pos;
    CHECK(game_collect_material(&g,0)); char toast[384]; strcpy(toast,g.toast);
    game_tick(&g,.05f,(Vec2){0}); CHECK(!strcmp(toast,g.toast));
    CHECK(g.material_notice[0]==1); g.toast_time=0; g.hitstop_time=0;
    game_tick(&g,.05f,(Vec2){0}); CHECK(strstr(g.toast,"재료 회수") && !g.material_notice[0]);
}
static void smith(Game *g) { g->phase=PH_PLAY; g->map=MAP_VILLAGE; g->hero.pos=NPCS[SMITH].pos; CHECK(game_open_shop(g,SMITH)); }
static void sales(void) {
    Game g; ready(&g); smith(&g); int coins=g.coins;
    CHECK(!game_sell_material(&g,MAT_LEATHER,1)); CHECK(g.coins==coins);
    g.materials[0]=5; g.materials[1]=3;
    Game before=g; CHECK(!game_sell_material(&g,(Material)-1,1)); CHECK(!game_sell_material(&g,(Material)2,1));
    CHECK(!game_sell_material(&g,MAT_LEATHER,0)); CHECK(!game_sell_material(&g,MAT_LEATHER,-1));
    CHECK(!game_sell_material(&g,MAT_LEATHER,INT_MAX)); CHECK(!game_sell_material(&g,MAT_LEATHER,6));
    CHECK(!memcmp(&before,&g,sizeof g));
    const int bad_coins[]={-1,INT_MAX,COIN_LIMIT+1};
    for(int n=0;n<3;n++) {
        g.coins=bad_coins[n]; Game invalid=g;
        CHECK(!game_sell_material(&g,MAT_LEATHER,1)); CHECK(!memcmp(&invalid,&g,sizeof g));
    }
    g=before;
    const int bad_stock[]={-1,INT_MAX,MATERIAL_LIMIT+1};
    for(int n=0;n<3;n++) {
        g.materials[0]=bad_stock[n]; Game invalid=g;
        CHECK(!game_sell_material(&g,MAT_LEATHER,1)); CHECK(!memcmp(&invalid,&g,sizeof g));
    }
    g=before;
    g=rt(&g); smith(&g); CHECK(game_sell_material(&g,MAT_LEATHER,1));
    CHECK(g.materials[0]==4 && g.materials[1]==3 && g.coins==coins+3); g=rt(&g); smith(&g);
    CHECK(game_sell_material(&g,MAT_LEATHER,4)); CHECK(!game_sell_material(&g,MAT_LEATHER,4));
    CHECK(!game_sell_material(&g,MAT_LEATHER,g.materials[0]));
    CHECK(g.coins==coins+15 && g.materials[0]==0); g=rt(&g); smith(&g);
    for(int n=0;n<100;n++) game_sell_material(&g,MAT_BONE,1);
    CHECK(g.coins==coins+21 && g.materials[1]==0 && g.herbs==0 && g.quest==Q_RECOVER);
    CHECK(g.tonics==2 && g.bandages==1 && !g.owned_sword && !g.owned_armor);
    g.materials[0]=MATERIAL_LIMIT; g.coins=COIN_LIMIT-2; before=g;
    CHECK(!game_sell_material(&g,MAT_LEATHER,1)); CHECK(!memcmp(&g,&before,sizeof g));
    g.coins=COIN_LIMIT-3; CHECK(game_sell_material(&g,MAT_LEATHER,1) && g.coins==COIN_LIMIT);
    g=rt(&g); smith(&g); g.coins=0; CHECK(game_sell_material(&g,MAT_LEATHER,MATERIAL_LIMIT-1));
    CHECK(g.coins==(MATERIAL_LIMIT-1)*3 && g.materials[0]==0);
    g.materials[1]=3; before=g; game_close_panel(&g); CHECK(g.materials[1]==3 && g.coins==before.coins);
    CHECK(!game_sell_material(&g,MAT_BONE,1));
    for(int shop=HERBALIST;shop<=INNKEEPER;shop++) {
        g.hero.pos=NPCS[shop].pos; CHECK(game_open_shop(&g,shop));
        CHECK(!game_sell_material(&g,MAT_BONE,1)); game_close_panel(&g);
    }
}
static void sign_record(size_t n, unsigned version) {
    bytes[4]=(unsigned char)version; bytes[8]=(unsigned char)n; bytes[9]=(unsigned char)(n>>8);
    uint32_t h=2166136261u; for(size_t i=0;i<n-4;i++) h=(h^bytes[i])*16777619u;
    for(int i=0;i<4;i++) bytes[n-4+i]=(unsigned char)(h>>(8*i));
}
static void migration_and_training(void) {
    Game g; ready(&g); kill(&g,0); g.materials[1]=7;
    const size_t old_sizes[]={216,224,SAVE_V3_SIZE};
    for(int version=1;version<=3;version++) {
        CHECK(save_encode(&g,bytes,&size)==SAVE_OK); size=old_sizes[version-1]; sign_record(size,version);
        Game out; CHECK(save_decode(&out,bytes,size)==SAVE_OK);
        CHECK(out.coins==g.coins && out.quest==g.quest && out.materials[0]==0 && out.materials[1]==0 && piles(&out)==0 && out.loot_rng);
        if(version==3) CHECK(out.hunt[0].hp==0 && out.hunt_respawn[0]==HUNT_RESPAWN_SECONDS);
        out=rt(&out); CHECK(piles(&out)==0); /* no retrofit lottery */
    }
    Game bad=g; bad.materials[0]=-1; CHECK(save_encode(&bad,bytes,&size)==SAVE_INVALID);
    bad=g; bad.materials[0]=MATERIAL_LIMIT; CHECK(save_encode(&bad,bytes,&size)==SAVE_INVALID);
    bad=g; bad.material_drops[0].quantity=INT_MAX; CHECK(save_encode(&bad,bytes,&size)==SAVE_INVALID);
    bad=g; bad.material_drops[0].drop.age=NAN; CHECK(save_encode(&bad,bytes,&size)==SAVE_INVALID);
    bad=g; bad.loot_rng=0; CHECK(save_encode(&bad,bytes,&size)==SAVE_INVALID);
    bad=g; bad.material_drops[0].drop.pos=(Vec2){NAN,-100}; bad=rt(&bad);
    CHECK(piles(&bad)==1 && distance(bad.material_drops[0].drop.pos,HUNT_SPAWNS[0])==0);
    CHECK(save_encode(&g,bytes,&size)==SAVE_OK); bytes[272]=255; bytes[273]=255; bytes[274]=255; bytes[275]=127;
    sign_record(size,4); Game sentinel=g,out=g;
    CHECK(save_decode(&out,bytes,size)==SAVE_CORRUPT && !memcmp(&out,&sentinel,sizeof out));
    Game saved; CHECK(game_enter_training(&g,&saved));
    int money=g.coins; uint32_t rng=g.loot_rng;
    for(int n=0;n<20;n++) CHECK(game_damage(&g,&g.boss,1000,FX_HIT));
    CHECK(g.coins==money && g.loot_rng==rng && piles(&g)==0 && !g.materials[0] && !g.materials[1]);
    CHECK(!game_collect_material(&g,0) && !game_sell_material(&g,MAT_LEATHER,1));
    CHECK(save_encode(&g,bytes,&size)==SAVE_INVALID);
    CHECK(game_leave_training(&g,&saved)); CHECK(g.materials[1]==7 && piles(&g)==1);
}
/* An isolated initial save used only by the SDL/browser execution tests. */
static void fixture(const char *path) {
    Game g; ready(&g); g.hero.pos.x+=45; g.loot_rng=seed_for(0);
    CHECK(save_encode(&g,bytes,&size)==SAVE_OK); FILE *f=fopen(path,"wb"); CHECK(f);
    CHECK(fwrite(bytes,1,size,f)==size); CHECK(!fclose(f));
}
int main(int argc,char **argv) {
    tables_and_once(); persistence_and_capacity(); collection(); sales(); migration_and_training();
    if(argc>1) fixture(argv[1]);
    printf("Economy: %d checks passed\n",checks); return 0;
}
