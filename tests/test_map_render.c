#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "draw.h"
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
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
static bool inside(const WorldShape *shape,float x,float y) {
    bool in=false;
    for(int i=0,j=shape->count-1;i<shape->count;j=i++) {
        Vec2 a=shape->points[i],b=shape->points[j];
        if((a.y>y)!=(b.y>y) && x<a.x+(y-a.y)*(b.x-a.x)/(b.y-a.y)) in=!in;
    }
    return in;
}
/* A clear spot where a cover hides part, not all, of a body standing there. */
static Vec2 partly_covered_spot(void) {
    for(int k=0;k<WORLD_COVER_COUNT;k++) {
        const WorldShape *shape=&WORLD_COVERS[k].shape;
        for(int i=0;i<shape->count;i++) for(float dy=4;dy<=40;dy+=4) {
            Vec2 p={shape->points[i].x,shape->points[i].y+dy};
            if(p.y>=WORLD_COVERS[k].depth || !world_clear(p,HERO_RADIUS)) continue;
            int in=0,all=0;
            for(float y=p.y-60;y<p.y;y+=2) for(float x=p.x-12;x<p.x+12;x+=2,all++) in+=inside(shape,x,y);
            if(in>all/4 && in<all*3/4) return p;
        }
    }
    CHECK(!"no partly covered spot");return (Vec2){0};
}
/* Where a roof, rock or canopy sorts in front of a boar, the screen must show
 * the map exactly: the boar is hidden there, never smeared or offset. */
static void hidden_behind_cover(Gfx *f,Game *g,SDL_Surface *s,Vec2 camera) {
    int W=s->w,H=s->h;SDL_Renderer *r=f->renderer;
    gfx_world(f,g,(float)W,(float)H,camera,false,(Vec2){0});
    Uint32 *shown=malloc((size_t)W*H*4),*plain=malloc((size_t)W*H*4);CHECK(shown&&plain);
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,shown,W*4)==0);
    SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
    SDL_FRect whole={-camera.x,-camera.y,WORLD_W,WORLD_H};
    CHECK(SDL_RenderCopyF(r,f->active_map->texture,NULL,&whole)==0);
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,plain,W*4)==0);
    Vec2 feet=g->hunt[0].pos;int covered=0,differs=0;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) {
        float wx=x+camera.x+.5f,wy=y+camera.y+.5f;
        if(fabsf(wx-feet.x)>12 || wy<feet.y-60 || wy>feet.y) continue;
        bool front=false,edge=false;
        for(int k=0;k<WORLD_COVER_COUNT;k++) {
            if(feet.y>=WORLD_COVERS[k].depth) continue;
            int n=0;
            for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) n+=inside(&WORLD_COVERS[k].shape,wx+dx*1.5f,wy+dy*1.5f);
            if(n==9) front=true; else if(n) edge=true;
        }
        if(edge && !front) continue; /* Outline pixels round either way. */
        if(front) { covered++;CHECK(shown[y*W+x]==plain[y*W+x]); }
        else differs+=shown[y*W+x]!=plain[y*W+x];
    }
    printf("cover: %d hidden pixels, %d boar pixels\n",covered,differs);
    CHECK(covered>100);CHECK(differs>20); /* The boar stands partly behind it. */
    free(shown);free(plain);
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
    game_reset(&g);game_start(&g);g.map=MAP_PASS;g.quest=Q_HUNT;g.traces[0]=g.traces[1]=true;world_select(g.map);
    for(int i=0;i<HUNT_COUNT;i++) g.hunt[i].hp=0;
    g.hunt[0].hp=g.hunt[0].max_hp;g.hunt[0].pos=partly_covered_spot();
    g.hero.pos=(Vec2){300,880};
    hidden_behind_cover(f,&g,s,(Vec2){g.hunt[0].pos.x-1152.37f,g.hunt[0].pos.y-768.61f});
    hidden_behind_cover(f,&g,s,(Vec2){g.hunt[0].pos.x-1152,g.hunt[0].pos.y-768});
    gfx_free(f);free(f);SDL_DestroyRenderer(r);SDL_FreeSurface(s);SDL_Quit();
    printf("PASS: %d map resource, visibility and render checks.\n",checks);return 0;
}
