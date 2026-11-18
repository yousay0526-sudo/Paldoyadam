#include "game.h"
#include "save.h"
#include "world.h"
#include "raid.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while(0)
static void begin(Game *g) {
    game_reset(g); game_start(g); g->coins = 1000;
    CHECK(game_open_cultivation(g));
}
static void resign(unsigned char *b, size_t size, unsigned version) {
    b[4] = (unsigned char)version;
    for (int i=0;i<4;i++) b[8+i]=(unsigned char)(size>>(8*i));
    uint32_t sum=2166136261u;
    for (size_t i=0;i<size-4;i++) sum=(sum^b[i])*16777619u;
    for (int i=0;i<4;i++) b[size-4+i]=(unsigned char)(sum>>(8*i));
}
static void transactions(void) {
    Game g; begin(&g); g.hero.hp = 47;
    CHECK(!game_learn_cultivation(&g,CUL_ENDURANCE));
    CHECK(g.coins==1000 && !g.cultivation_nodes);
    CHECK(game_learn_cultivation(&g,CUL_FOUNDATION));
    CHECK(g.hero.hp==47 && g.hero.max_hp==105 && g.coins==970);
    Game before=g;
    CHECK(!game_learn_cultivation(&g,CUL_FOUNDATION) && !memcmp(&g,&before,sizeof g));
    CHECK(!game_learn_cultivation(&g,(CultivationNode)-1));
    CHECK(!game_learn_cultivation(&g,CUL_COUNT));
    g.coins=59; before=g;
    CHECK(!game_learn_cultivation(&g,CUL_VITALITY) && !memcmp(&g,&before,sizeof g));
    g.coins=1000;
    for(int i=1;i<CUL_COUNT;i++) CHECK(game_learn_cultivation(&g,(CultivationNode)i));
    CHECK(g.hero.max_hp==120 && g.hero.hp==47 && game_cultivation_damage(&g)==2);
    CHECK(fabsf(game_dodge_recharge_seconds(&g)-1.1f)<.0001f);
    CHECK(g.coins==460); /* 3 * (60 + 120) */
    game_close_panel(&g); CHECK(g.phase==PH_PLAY);
    Game saved;
    CHECK(game_enter_training(&g,&saved));
    CHECK(g.cultivation_nodes==127 && g.hero.max_hp==120 && g.hero.hp==120);
    CHECK(!game_open_cultivation(&g));
    CHECK(game_leave_training(&g,&saved));
    CHECK(g.hero.hp==47 && g.cultivation_nodes==127);
    game_hurt(&g,999,false); CHECK(g.hero.hp==0);
    game_recover(&g); CHECK(g.hero.hp==120 && g.cultivation_nodes==127);
    game_reset(&g); CHECK(g.hero.max_hp==100 && !g.cultivation_nodes);
    begin(&g); g.map=MAP_PASS;
    before=g; CHECK(!game_learn_cultivation(&g,CUL_FOUNDATION) && !memcmp(&g,&before,sizeof g));
    g.map=MAP_VILLAGE; g.training.active=true;
    CHECK(!game_learn_cultivation(&g,CUL_FOUNDATION));
    g.training.active=false; g.phase=PH_PLAY;
    CHECK(!game_learn_cultivation(&g,CUL_FOUNDATION));
    CHECK(!game_open_cultivation(NULL));
}
static void persistence(void) {
    unsigned char bytes[SAVE_CAPACITY], original[SAVE_CAPACITY]; size_t size;
    for(uint32_t mask=0;mask<128;mask++) {
        Game g,out; begin(&g); g.cultivation_nodes=mask;
        g.hero.max_hp=game_cultivation_health(&g);
        SaveResult expected=cultivation_valid(mask)?SAVE_OK:SAVE_INVALID;
        CHECK(save_encode(&g,bytes,&size)==expected);
        if(expected!=SAVE_OK) continue;
        CHECK(size==SAVE_V6_SIZE && save_check(bytes,size)==SAVE_OK);
        CHECK(save_decode(&out,bytes,size)==SAVE_OK);
        CHECK(out.cultivation_nodes==mask && out.hero.max_hp==g.hero.max_hp && out.hero.hp==g.hero.hp);
        CHECK(out.coins==g.coins && out.phase==PH_PLAY);
    }
    Game g,out; begin(&g);
    g.spirit_shards=17; g.spirit_orbs=8;
    CHECK(save_encode(&g,original,&size)==SAVE_OK);
    /* Real old layouts are prefixes before the checksum; each migrates to zero nodes. */
    const size_t legacy[]={0,216,224,SAVE_V3_SIZE,SAVE_V4_SIZE,SAVE_V5_SIZE};
    for(unsigned version=1;version<=5;version++) {
        memcpy(bytes,original,sizeof bytes); resign(bytes,legacy[version],version);
        CHECK(save_decode(&out,bytes,legacy[version])==SAVE_OK && !out.cultivation_nodes && out.hero.max_hp==100);
        CHECK(out.spirit_shards==(version>=5?17:0) && out.spirit_orbs==(version>=5?8:0));
    }
    const uint32_t invalid[]={1u<<CUL_COUNT,1u<<CUL_EDGE,UINT32_MAX};
    for(unsigned k=0;k<sizeof invalid/sizeof invalid[0];k++) {
        memcpy(bytes,original,sizeof bytes);
        for(int i=0;i<4;i++) bytes[size-8+i]=(unsigned char)(invalid[k]>>(8*i));
        resign(bytes,size,SAVE_VERSION); memset(&out,0xa5,sizeof out); Game before=out;
        CHECK(save_check(bytes,size)==SAVE_CORRUPT);
        CHECK(save_decode(&out,bytes,size)==SAVE_CORRUPT && !memcmp(&out,&before,sizeof out));
    }
    CHECK(game_learn_cultivation(&g,CUL_FOUNDATION));
    g.hero.max_hp=100; CHECK(save_encode(&g,bytes,&size)==SAVE_INVALID);
}
static int impact(bool trained, bool skill) {
    Game g,saved; game_reset(&g); game_start(&g);
    if(trained) {
        g.coins=1000; CHECK(game_open_cultivation(&g));
        CHECK(game_learn_cultivation(&g,CUL_FOUNDATION));
        CHECK(game_learn_cultivation(&g,CUL_STRENGTH));
        CHECK(game_learn_cultivation(&g,CUL_EDGE));
        game_close_panel(&g);
    }
    CHECK(game_enter_training(&g,&saved));
    CHECK(skill?game_technique(&g,TECH_CLEAVE):game_attack(&g));
    for(int i=0;i<24;i++) game_tick(&g,1.0f/60,(Vec2){0});
    CHECK(g.training.hits==1);
    return g.training.last_damage;
}
static void effects(void) {
    CHECK(impact(true,false)==impact(false,false)+2);
    CHECK(impact(true,true)==impact(false,true)+2);
    Game g; begin(&g);
    CHECK(game_learn_cultivation(&g,CUL_FOUNDATION));
    CHECK(game_learn_cultivation(&g,CUL_FOOTWORK));
    CHECK(game_learn_cultivation(&g,CUL_BREATH));
    float before=g.time; game_tick(&g,.05f,(Vec2){1,0}); CHECK(g.time==before);
    game_close_panel(&g);
    CHECK(game_dodge(&g,(Vec2){1,0}) && fabsf(g.dodge_recharge-1.1f)<.0001f);
    /* One extra fixed step covers float accumulation at the 1.10s boundary. */
    for(int i=0;i<67;i++) game_tick(&g,1.0f/60,(Vec2){0});
    CHECK(g.dodge_charges==DODGE_CHARGES && g.dodge_recharge==0);
}
static void raid_integration(void) {
    Game g,saved,out; begin(&g);
    for(int i=0;i<CUL_COUNT;i++) CHECK(game_learn_cultivation(&g,(CultivationNode)i));
    g.hero.hp=43; g.spirit_shards=17; g.spirit_orbs=8;
    g.traces[0]=g.traces[1]=true; g.quest=Q_RECOVER; game_close_panel(&g);
    int coins=g.coins;
    CHECK(raid_offer(&g) && raid_enter(&g,&saved));
    CHECK(g.cultivation_nodes==127 && g.hero.hp==120 && g.hero.max_hp==120);
    CHECK(!game_open_cultivation(&g));
    g.phase=PH_PAUSED; CHECK(!game_open_cultivation(&g)); g.phase=PH_PLAY;
    g.raid.stacks[RAUG_STEP]=3; g.raid.stacks[RAUG_STEEL]=2;
    CHECK(fabsf(game_dodge_recharge_seconds(&g)-.5f)<.0001f);
    CHECK(game_dodge(&g,(Vec2){1,0}) && fabsf(g.dodge_recharge-.5f)<.0001f);
    g.hero.dodging=false; g.hero.invul=0; g.hero.vel=(Vec2){0};
    g.hunt[0].pos=g.hunt[0].home=(Vec2){g.hero.pos.x+30,g.hero.pos.y};
    g.hunt[0].hp=g.hunt[0].max_hp=200; g.hunt[0].state=AI_RECOVER; g.hunt[0].timer=10;
    CHECK(game_attack(&g));
    for(int i=0;i<13;i++) game_tick(&g,1.0f/60,(Vec2){0});
    CHECK(g.hunt[0].hp==185); /* 9 base + 2 permanent + 4 temporary. */
    unsigned char bytes[SAVE_CAPACITY]; size_t size;
    CHECK(save_encode(&g,bytes,&size)==SAVE_INVALID);
    g.raid.coins=21; g.raid.success=true; g.raid.stage=RAID_RESULT; g.phase=PH_RAID;
    CHECK(raid_leave(&g,&saved));
    CHECK(!g.raid.active && g.cultivation_nodes==127 && g.hero.hp==43 && g.hero.max_hp==120);
    CHECK(g.spirit_shards==20 && g.spirit_orbs==8 && g.coins==coins+51);
    CHECK(fabsf(game_dodge_recharge_seconds(&g)-1.1f)<.0001f);
    CHECK(save_encode(&g,bytes,&size)==SAVE_OK && size==SAVE_V6_SIZE);
    CHECK(save_decode(&out,bytes,size)==SAVE_OK);
    CHECK(out.cultivation_nodes==127 && out.spirit_shards==20 && out.spirit_orbs==8 && out.coins==g.coins);
    CHECK(out.hero.hp==43 && out.hero.max_hp==120 && !out.raid.active);
}
int main(void) {
    transactions(); persistence(); effects(); raid_integration();
    printf("PASS: %d cultivation checks\n",checks); return 0;
}
