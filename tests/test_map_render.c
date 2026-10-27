#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "draw.h"
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static int checks;
#define CHECK(c) do{checks++;if(!(c)){fprintf(stderr,"FAIL map render:%d: %s\n",__LINE__,#c);exit(1);}}while(0)
static uint64_t capture(Gfx *f,Game *g,SDL_Surface *s) {
    gfx_world(f,g,2304,1536,(Vec2){0},false,(Vec2){0});
    CHECK(SDL_RenderReadPixels(f->renderer,NULL,s->format->format,s->pixels,s->pitch)==0);
    uint64_t hash=1469598103934665603ULL;
    unsigned char *p=s->pixels;
    for(int i=0;i<s->pitch*s->h;i++) hash=(hash^p[i])*1099511628211ULL;
    return hash;
}
int main(int argc,char **argv) {
    CHECK(argc>=2);SDL_SetMainReady();SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,2304,1536,32,SDL_PIXELFORMAT_RGBA32);CHECK(s);
    SDL_Renderer *r=SDL_CreateSoftwareRenderer(s);CHECK(r);Gfx *f=calloc(1,sizeof *f);CHECK(f);
    CHECK(gfx_load(f,r,argv[1]));Game g;uint64_t hashes[3];
    for(int m=0;m<3;m++) {
        game_reset(&g);game_start(&g);g.map=(MapId)m;g.hero.pos=m==0?HOME:m==1?(Vec2){1152,795}:(Vec2){1152,1095};
        g.quest=m==1?Q_TRACES:m==2?Q_HUNT:Q_MEET;
        hashes[m]=capture(f,&g,s);CHECK(world_clear(g.hero.pos,HERO_RADIUS));CHECK(f->active_map==&f->locations[m]);
        if(argc>=3) { char p[2048];snprintf(p,sizeof p,"%s/map-%d.bmp",argv[2],m);CHECK(SDL_SaveBMP(s,p)==0); }
    }
    CHECK(hashes[0]!=hashes[1] && hashes[1]!=hashes[2] && hashes[0]!=hashes[2]);
    game_reset(&g);game_start(&g);g.map=MAP_VILLAGE;g.traces[0]=g.traces[1]=true;g.quest=Q_RECOVER;
    for(int i=0;i<BOAR_COUNT;i++) g.boars[i].pos=(Vec2){1152+i*80,800};
    uint64_t before=capture(f,&g,s);for(int i=0;i<BOAR_COUNT;i++) g.boars[i].hp=0;
    CHECK(before==capture(f,&g,s)); /* Hunting monsters never leak into town. */
    g.map=MAP_PASS;before=capture(f,&g,s);g.boars[0].hp=38;
    CHECK(before!=capture(f,&g,s));
    g.quest=Q_COMBAT;g.boss.pos=(Vec2){1152,800};before=capture(f,&g,s);g.boss.hp=0;
    CHECK(before==capture(f,&g,s)); /* The boss never leaks into the normal field. */
    g.map=MAP_CAVE;g.quest=Q_HUNT;before=capture(f,&g,s);g.boss.hp=240;
    CHECK(before!=capture(f,&g,s));
    gfx_free(f);free(f);SDL_DestroyRenderer(r);SDL_FreeSurface(s);SDL_Quit();
    printf("PASS: %d map resource, visibility and render checks.\n",checks);return 0;
}
