/* Compare the real monster renderer with independent authored-cell draws. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "draw.h"
#include "monster_atlas_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { \
    fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while (0)
static const Vec2 directions[8]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
static void clear(SDL_Renderer *r) {
    SDL_SetRenderDrawColor(r,29,37,31,255); SDL_RenderClear(r);
}
static void read_pixels(SDL_Renderer *r, SDL_Surface *s, void *pixels) {
    CHECK(SDL_RenderReadPixels(r,NULL,s->format->format,pixels,s->pitch)==0);
}
int main(int argc, char **argv) {
    CHECK(argc==2); SDL_SetMainReady(); SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,768,768,32,SDL_PIXELFORMAT_RGBA32);
    CHECK(surface); SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface); CHECK(r);
    Gfx *gfx=calloc(1,sizeof *gfx); CHECK(gfx); CHECK(gfx_load(gfx,r,argv[1]));
    size_t bytes=(size_t)surface->pitch*surface->h;
    Uint32 *actual=malloc(bytes), *expected=malloc(bytes); CHECK(actual&&expected);
    for(int i=0;i<MONSTER_ATLAS_COUNT;i++) {
        Picture *p=&gfx->monster_atlases[i]; CHECK(p->texture&&p->w==768);
        CHECK(p->h==(i==MONSTER_BOAR_IDLE||i==MONSTER_BUTCHER_IDLE?384:1536));
        SDL_ScaleMode mode; CHECK(SDL_GetTextureScaleMode(p->texture,&mode)==0);
        CHECK(mode==SDL_ScaleModeLinear);
        for(int j=0;j<i;j++) CHECK(p->texture!=gfx->monster_atlases[j].texture);
    }
    Vec2 feet={128,220};
    for(int scale=1;scale<=3;scale++) for(int boss=0;boss<2;boss++) for(int d=0;d<8;d++) {
        SDL_RenderSetScale(r,(float)scale,(float)scale);
        for(int pose=0;pose<11;pose++) {
            Enemy e={0}; e.hp=e.max_hp=100; e.state=AI_IDLE; e.facing=directions[d];
            e.angle=atan2f(directions[d].y,directions[d].x);
            int atlas=boss?MONSTER_BUTCHER_IDLE:MONSTER_BOAR_IDLE, frame=0;
            float sx=1,sy=1,dx=0,dy=0;
            if(pose>=1&&pose<=4) {
                e.walking=true; e.state=AI_CHASE; frame=pose-1; e.stride=frame*13.0f;
                atlas=boss?MONSTER_BUTCHER_WALK:MONSTER_BOAR_WALK;
                float bob=fabsf(sinf(e.stride*3.14159265358979323846f/26));
                sx-=.02f*bob; sy+=.03f*bob; dy-=2.5f*bob;
            } else if(pose>=5) {
                atlas=boss?MONSTER_BUTCHER_SKILL:MONSTER_BOAR_ATTACK;
                float windup=boss?BOSS_WINDUP_SECONDS:BOAR_WINDUP_SECONDS;
                e.state=AI_WINDUP; e.timer=windup;
                if(pose==6) { /* Late preparation must still use the pre-impact cell. */
                    e.timer=windup*.5f; frame=1; sx=1.025f; sy=.97f;
                    dx=-cosf(e.angle)*2; dy=-sinf(e.angle);
                } else if(pose==7) { /* First actual impact state, not earlier. */
                    e.state=boss?AI_SWING:AI_RECOVER;
                    e.timer=boss?BOSS_SWING_SECONDS:BOAR_RECOVER_SECONDS;
                    frame=2; sx=.96f; sy=1.06f;
                    dx=cosf(e.angle)*9; dy=sinf(e.angle)*5;
                } else if(pose==8) { e.state=AI_RECOVER; e.timer=.5f; frame=3; }
                else if(pose==9) { /* The very last windup tick is not impact. */
                    e.timer=.001f;frame=1;float t=1-e.timer/windup;
                    sx=1+.05f*t;sy=1-.06f*t;dx=-cosf(e.angle)*4*t;dy=-sinf(e.angle)*2*t;
                } else if(pose==10) { /* A boar's interrupted windup must not jab. */
                    e.state=AI_RECOVER;e.timer=BOAR_STAGGER_SECONDS;frame=3;e.walking=true;
                }
                /* Attack view is captured: walking or the hero moving cannot turn it. */
                e.facing=directions[(d+3)%8];
            }
            Enemy before=e; clear(r);
            SDL_FRect bounds=gfx_enemy(gfx,&e,feet,boss!=0,0,true,d<4);
            CHECK(memcmp(&before,&e,sizeof e)==0); read_pixels(r,surface,actual);
            Picture *p=&gfx->monster_atlases[atlas];
            SDL_Rect src={pose==0?d%4*192:frame*192,pose==0?d/4*192:d*192,192,192};
            float size=(boss?110:60)*ACTOR_SCALE*monster_draw_scale[atlas][d];
            SDL_FRect dest={feet.x-size*sx*.5f+dx,feet.y-size*sy*176/192+dy,size*sx,size*sy};
            clear(r); CHECK(SDL_RenderCopyF(r,p->texture,&src,&dest)==0);
            read_pixels(r,surface,expected); CHECK(memcmp(actual,expected,bytes)==0);
            Uint32 bg=SDL_MapRGBA(surface->format,29,37,31,255); int visible=0;
            for(int y=0;y<surface->h;y++) for(int x=0;x<surface->w;x++) if(actual[y*surface->w+x]!=bg) {
                visible++;
                CHECK(x>=floorf(bounds.x*scale)&&x<=ceilf((bounds.x+bounds.w)*scale));
                CHECK(y>=floorf(bounds.y*scale)&&y<=ceilf((bounds.y+bounds.h)*scale));
            }
            CHECK(visible>150*scale);
        }
        /* Fall, fade, hit overlay and atlas modulation must not leak state. */
        Enemy e={0};e.hp=100;e.facing=directions[d];e.hit_time=HIT_FLASH_SECONDS;
        Picture *p=&gfx->monster_atlases[boss?MONSTER_BUTCHER_IDLE:MONSTER_BOAR_IDLE];
        SDL_SetTextureColorMod(p->texture,180,190,200);SDL_SetTextureAlphaMod(p->texture,73);
        SDL_BlendMode blend;SDL_GetTextureBlendMode(p->texture,&blend);
        gfx_enemy(gfx,&e,feet,boss!=0,0,true,d<4);
        Uint8 red,green,blue,alpha;SDL_BlendMode after;
        SDL_GetTextureColorMod(p->texture,&red,&green,&blue);SDL_GetTextureAlphaMod(p->texture,&alpha);
        SDL_GetTextureBlendMode(p->texture,&after);
        CHECK(red==180&&green==190&&blue==200&&alpha==73&&blend==after);
        SDL_SetTextureColorMod(p->texture,255,255,255);SDL_SetTextureAlphaMod(p->texture,255);
        e.hp=0;e.dying=ENEMY_DEATH_SECONDS*.5f;
        clear(r); SDL_FRect bounds=gfx_enemy(gfx,&e,feet,boss!=0,0,true,d<4);read_pixels(r,surface,actual);
        CHECK(bounds.w>0&&bounds.h>0);
        Uint32 bg=SDL_MapRGBA(surface->format,29,37,31,255);int falling=0;
        for(int y=0;y<surface->h;y++) for(int x=0;x<surface->w;x++) if(actual[y*surface->w+x]!=bg) {
            falling++;
            CHECK(x>=floorf(bounds.x*scale)&&x<=ceilf((bounds.x+bounds.w)*scale));
            CHECK(y>=floorf(bounds.y*scale)&&y<=ceilf((bounds.y+bounds.h)*scale));
        }
        CHECK(falling>50);
        clear(r);gfx_enemy(gfx,&e,feet,boss!=0,100,true,d<4);read_pixels(r,surface,expected);
        CHECK(memcmp(actual,expected,bytes)==0); /* Frozen fall ignores wall time. */
        e.dying=0;clear(r);gfx_enemy(gfx,&e,feet,boss!=0,0,true,d<4);read_pixels(r,surface,actual);
        clear(r);read_pixels(r,surface,expected);CHECK(memcmp(actual,expected,bytes)==0);
    }
    free(actual);free(expected);gfx_free(gfx);
    for(int i=0;i<MONSTER_ATLAS_COUNT;i++) CHECK(!gfx->monster_atlases[i].texture);
    free(gfx);SDL_DestroyRenderer(r);SDL_FreeSurface(surface);SDL_Quit();
    printf("PASS: %d monster rendering checks: 8 authored views, gait, impact boundaries, roots, bounds, fall and texture restoration at 1/2/3x.\n",checks);
    return 0;
}
