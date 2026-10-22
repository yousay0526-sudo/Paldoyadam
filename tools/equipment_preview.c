/* Standalone visual proof. Does not link or modify the game's event loop. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "draw.h"
#include "equipment.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    EquipmentLoadout gear;
    EquipmentMotion motion;
    float elapsed;
    bool paused;
    SDL_FRect buttons[7];
} Preview;
static void choose(Preview *p, int action) {
    if (action==0) p->gear.upper=!p->gear.upper;
    if (action==1) p->gear.lower=!p->gear.lower;
    if (action==2) p->gear.helmet=!p->gear.helmet;
    if (action==3) p->gear.weapon=(p->gear.weapon+1)%3;
    if (action>=4 && action<=6) {p->motion=(EquipmentMotion)(action-4); p->elapsed=0;}
}
static bool preview_event(Preview *p, const SDL_Event *event, int window_w, int window_h,
                          int width, int height) {
    if (event->type==SDL_QUIT) return false;
    if (event->type==SDL_KEYDOWN && !event->key.repeat) {
        SDL_Keycode k=event->key.keysym.sym;
        if (k==SDLK_ESCAPE) return false;
        if (k>=SDLK_1 && k<=SDLK_4) choose(p,(int)(k-SDLK_1));
        if (k==SDLK_i) choose(p,4);
        if (k==SDLK_w) choose(p,5);
        if (k==SDLK_a) choose(p,6);
        if (k==SDLK_SPACE) p->paused=!p->paused;
    }
    if (event->type==SDL_MOUSEBUTTONUP && event->button.button==SDL_BUTTON_LEFT &&
        window_w>0 && window_h>0) {
        SDL_FPoint at={(float)event->button.x*width/window_w,
                      (float)event->button.y*height/window_h};
        for(int b=0;b<7;b++) {
            SDL_FRect box=p->buttons[b];
            if(at.x>=box.x && at.x<box.x+box.w && at.y>=box.y && at.y<box.y+box.h) choose(p,b);
        }
    }
    return true;
}
static void scene(Gfx *g, EquipmentArt *art, Preview *p, int width, int height, bool hold_phase) {
    SDL_Renderer *r=g->renderer;
    SDL_SetRenderDrawColor(r,20,35,40,255); SDL_RenderClear(r);
    float w=(float)width,h=(float)height;
    gfx_text(g,"팔도야담 · 장비 외형 시제품",24,14,26,w-48,PAPER);
    gfx_text(g,"남쪽 한 방향 / 몸과 장비가 같은 관절·동작을 공유합니다",24,51,15,w-48,MUTED);
    float top=86, bottom=h-116, cardw=(w-68)/2, cardh=bottom-top;
    for (int i=0;i<2;i++) {
        SDL_FRect card={24+i*(cardw+20),top,cardw,cardh};
        gfx_round(g,card,12,(SDL_Color){34,53,57,255});
        gfx_center(g,i?"선택한 장비":"기본 몸",card.x+card.w/2,top+10,17,GOLD);
        SDL_FPoint feet={card.x+card.w/2,bottom-15};
        /* Reserve the full overhead sword sweep above the fixed foot root. */
        float scale=fminf((cardh-52)/230,cardw/230);
        float cycle=p->motion==EQ_SLASH ? HERO_SKILL_SECONDS : .8f;
        float phase=hold_phase?p->elapsed:fmodf(p->elapsed/cycle,1);
        EquipmentLoadout gear=i?p->gear:(EquipmentLoadout){0};
        EquipmentPose pose=equipment_pose(gear,p->motion,phase,feet,scale);
        gfx_line(g,(Vec2){card.x+12,feet.y},(Vec2){card.x+card.w-12,feet.y},(SDL_Color){70,89,88,255});
        equipment_draw(art,r,&pose);
    }
    const char *names[7]={p->gear.upper?"상의 · 착용":"상의 · 해제",
        p->gear.lower?"하의 · 착용":"하의 · 해제",p->gear.helmet?"투구 · 착용":"투구 · 해제",
        p->gear.weapon==0?"무기 · 없음":p->gear.weapon==1?"무기 · 기본 환도":"무기 · 장식 환도",
        "정지","걷기","베기"};
    for (int i=0;i<7;i++) {
        int n=i<4?4:3, col=i<4?i:i-4;
        float bw=(w-48-(n-1)*8)/n;
        SDL_FRect b={24+col*(bw+8),h-(i<4?100:55),bw,36};
        p->buttons[i]=b;
        bool selected=i>=4 && (int)p->motion==i-4;
        gfx_round(g,b,8,selected?(SDL_Color){76,98,87,255}:(SDL_Color){43,65,67,255});
        gfx_label(g,names[i],b,15,selected?GOLD:PAPER);
    }
}
int main(int argc,char **argv) {
    const char *assets="assets",*parts="art/hero/equipment/cutout-south-v1.png",*smoke=NULL;
    int width=960,height=600;
    Preview p={.gear={true,true,true,1},.motion=EQ_WALK};
    bool hold_phase=false;
    for(int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--assets") && i+1<argc) assets=argv[++i];
        else if (!strcmp(argv[i],"--parts") && i+1<argc) parts=argv[++i];
        else if (!strcmp(argv[i],"--smoke") && i+1<argc) smoke=argv[++i];
        else if (!strcmp(argv[i],"--size") && i+1<argc) {
            if (sscanf(argv[++i],"%dx%d",&width,&height)!=2 || width<640 || height<390 || width>4096 || height>2160) return 2;
        } else if (!strcmp(argv[i],"--phase") && i+1<argc) {
            char *end; p.elapsed=strtof(argv[++i],&end);
            if (*end || !isfinite(p.elapsed) || p.elapsed<0 || p.elapsed>1) return 2;
            hold_phase=true;
        } else if (!strcmp(argv[i],"--motion") && i+1<argc) {
            const char *m=argv[++i];
            if (!strcmp(m,"idle")) p.motion=EQ_IDLE;
            else if (!strcmp(m,"walk")) p.motion=EQ_WALK;
            else if (!strcmp(m,"slash")) p.motion=EQ_SLASH;
            else return 2;
        } else if (!strcmp(argv[i],"--loadout") && i+1<argc) {
            char *end; long n=strtol(argv[++i],&end,10);
            if (*end || n<0 || n>23) return 2;
            p.gear=(EquipmentLoadout){(n&1)!=0,(n&2)!=0,(n&4)!=0,(int)(n/8)};
        } else if (!strcmp(argv[i],"--help")) {
            puts("paldo-equipment-preview [--assets PATH] [--parts PNG] [--size 960x600]\n"
                 "  1/2/3: upper/lower/helmet, 4: sword, I/W/A: idle/walk/slash, Space: pause\n"
                 "  --smoke frame.bmp --motion idle|walk|slash --phase 0..1 --loadout 0..23\n"
                 "  Mouse/touch: select the matching on-screen button. South view only.");
            return 0;
        } else return 2;
    }
    SDL_SetMainReady();
    if (smoke) SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0) {SDL_Log("%s",SDL_GetError());return 1;}
    SDL_Window *window=SDL_CreateWindow("Paldoyadam - equipment prototype",SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,width,height,smoke?SDL_WINDOW_HIDDEN:SDL_WINDOW_RESIZABLE);
    if (!window) {SDL_Quit();return 1;}
    SDL_SetWindowMinimumSize(window,640,390);
    SDL_Renderer *r=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    Gfx g; EquipmentArt art={0}; int status=0;
    if (!r || !gfx_load(&g,r,assets)) {SDL_Log("Cannot load game reference assets");status=1;goto done;}
    if (!equipment_load(&art,r,parts)) {SDL_Log("Cannot load cutout parts: %s",parts);status=1;goto free_gfx;}
    bool running=true; Uint64 previous=SDL_GetPerformanceCounter();
    while(running) {
        Uint64 now=SDL_GetPerformanceCounter();
        float dt=(float)((double)(now-previous)/SDL_GetPerformanceFrequency()); previous=now;
        if (!p.paused && !hold_phase) p.elapsed+=fminf(dt,.1f);
        SDL_GetRendererOutputSize(r,&width,&height);
        scene(&g,&art,&p,width,height,hold_phase);
        if (smoke) {
            SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
            if (!s || SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,s->pixels,s->pitch)<0 ||
                SDL_SaveBMP(s,smoke)<0) status=1;
            SDL_FreeSurface(s); break;
        }
        SDL_RenderPresent(r);
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
            if (!preview_event(&p,&event,ww,wh,width,height)) running=false;
            /* SDL's default touch-to-mouse translation feeds the same buttons. */
        }
        SDL_Delay(8);
    }
    equipment_free(&art);
free_gfx:
    gfx_free(&g);
done:
    SDL_DestroyRenderer(r);SDL_DestroyWindow(window);SDL_Quit();return status;
}
