#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "equipment.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
/* Exercise the standalone tool's actual SDL handler and button layout. */
#define main equipment_preview_entry
#include "../tools/equipment_preview.c"
#undef main

static int checks;
#define CHECK(x) do { checks++; if(!(x)) {fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static bool near(float a,float b) {return fabsf(a-b)<.001f;}
static bool part(const EquipmentPose *p, EquipmentPart id) {
    for(int i=0;i<p->count;i++) if(p->layers[i].part==id) return true;
    return false;
}
static void preview_controls(Gfx *g, EquipmentArt *art) {
    const int sizes[][2]={{640,390},{844,390},{1280,720}};
    for(int n=0;n<3;n++) {
        int w=sizes[n][0],h=sizes[n][1];
        Preview p={.motion=EQ_IDLE};
        scene(g,art,&p,w,h,true);
        for(int b=0;b<7;b++) {
            SDL_FRect r=p.buttons[b];
            CHECK(r.x>=0 && r.y>=0 && r.x+r.w<=w && r.y+r.h<=h);
            for(int j=0;j<b;j++) {
                SDL_FRect other=p.buttons[j];
                CHECK(r.x+r.w<=other.x || other.x+other.w<=r.x ||
                      r.y+r.h<=other.y || other.y+other.h<=r.y);
            }
            SDL_Event event={0};event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;
            /* Exercise the same inverse transform at 2x pixel density. */
            event.button.x=(int)((r.x+r.w/2)/2);event.button.y=(int)((r.y+r.h/2)/2);
            CHECK(preview_event(&p,&event,w/2,h/2,w,h));
        }
        CHECK(p.gear.upper && p.gear.lower && p.gear.helmet && p.gear.weapon==1);
        CHECK(p.motion==EQ_SLASH);
        SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_1;
        event.key.repeat=1;CHECK(preview_event(&p,&event,w,h,w,h));CHECK(p.gear.upper);
        event.key.repeat=0;
        /* Dispatch actual queued SDL events, as the running tool does. */
        SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        CHECK(SDL_PushEvent(&event)==1);
        CHECK(SDL_PollEvent(&event)==1);
        CHECK(preview_event(&p,&event,w,h,w,h));CHECK(!p.gear.upper);
        const SDL_Keycode keys[]={SDLK_2,SDLK_3,SDLK_4,SDLK_4,SDLK_i,SDLK_w,SDLK_a,SDLK_SPACE};
        for(int k=0;k<8;k++) {
            event.type=SDL_KEYDOWN;event.key.repeat=0;event.key.keysym.sym=keys[k];
            CHECK(preview_event(&p,&event,w,h,w,h));
        }
        CHECK(!p.gear.lower && !p.gear.helmet && p.gear.weapon==0 && p.paused && p.motion==EQ_SLASH);
        event.key.keysym.sym=SDLK_ESCAPE;CHECK(!preview_event(&p,&event,w,h,w,h));
    }
}
static uint64_t capture(SDL_Renderer *r,SDL_Surface *s,const EquipmentArt *art,
                        const EquipmentPose *p,unsigned char *pixels) {
    SDL_SetRenderDrawColor(r,0,0,0,0);SDL_RenderClear(r);
    CHECK(equipment_draw(art,r,p));
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,pixels,s->w*4)==0);
    uint64_t hash=1469598103934665603ULL; int visible=0;
    for(int y=0;y<s->h;y++) for(int x=0;x<s->w;x++) {
        unsigned char *v=pixels+((size_t)y*s->w+x)*4;
        /* SDL software backends can return opaque alpha for the clear color. */
        if (!v[0] && !v[1] && !v[2]) continue;
        visible++;
        CHECK(x>0 && x<s->w-1 && y>0 && y<s->h-1);
        CHECK(x>=p->bounds.x-2 && y>=p->bounds.y-2 &&
              x<p->bounds.x+p->bounds.w+2 && y<p->bounds.y+p->bounds.h+2);
        for(int k=0;k<4;k++) hash=(hash^v[k])*1099511628211ULL;
    }
    CHECK(visible>500);return hash;
}
int main(int argc,char **argv) {
    CHECK(argc==3);SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1024,1024,32,SDL_PIXELFORMAT_RGBA32);
    CHECK(s!=NULL);SDL_Renderer *r=SDL_CreateSoftwareRenderer(s);CHECK(r!=NULL);
    EquipmentArt art;CHECK(equipment_load(&art,r,argv[1]));
    Gfx gfx;CHECK(gfx_load(&gfx,r,argv[2]));preview_controls(&gfx,&art);gfx_free(&gfx);
    unsigned char *pixels=malloc(1024*1024*4);CHECK(pixels!=NULL);
    SDL_FPoint feet={512,900};
    uint64_t outfits[24];
    for(int n=0;n<24;n++) {
        EquipmentLoadout gear={(n&1)!=0,(n&2)!=0,(n&4)!=0,n/8};
        EquipmentPose p=equipment_pose(gear,EQ_IDLE,0,feet,2);
        CHECK(part(&p,EQ_SASH));CHECK(part(&p,EQ_ARMOR)==gear.upper);
        CHECK(part(&p,EQ_HELMET)==gear.helmet);
        CHECK(part(&p,EQ_RIGHT_ARMOR_LEG)==gear.lower);
        CHECK(part(&p,EQ_LEFT_ARMOR_LEG)==gear.lower);
        CHECK(part(&p,EQ_RIGHT_LEG)!=gear.lower);
        outfits[n]=capture(r,s,&art,&p,pixels);
        for(int k=0;k<n;k++) CHECK(outfits[n]!=outfits[k]);
        /* Every selection uses exactly the same body root and right-hand path. */
        for(int m=0;m<3;m++) for(int k=0;k<=30;k++) {
            float t=k/30.0f;
            EquipmentPose a=equipment_pose(gear,(EquipmentMotion)m,t,feet,1);
            EquipmentPose b=equipment_pose((EquipmentLoadout){0},(EquipmentMotion)m,t,feet,1);
            CHECK(near(a.right_hand.x,b.right_hand.x) && near(a.right_hand.y,b.right_hand.y));
            if(gear.weapon) CHECK(near(a.right_hand.x,a.weapon_grip.x) && near(a.right_hand.y,a.weapon_grip.y));
            CHECK(a.count<=20);
        }
    }
    for(int m=0;m<3;m++) {
        uint64_t frames[31];int unique=0;
        for(int k=0;k<=30;k++) {
            EquipmentPose p=equipment_pose((EquipmentLoadout){true,true,true,2},
                                           (EquipmentMotion)m,k/30.0f,feet,2);
            frames[k]=capture(r,s,&art,&p,pixels);
            bool found=false;for(int j=0;j<k;j++) if(frames[j]==frames[k]) found=true;
            if(!found) unique++;
        }
        CHECK(m==EQ_IDLE?unique==1:unique>15);
        if(m==EQ_WALK) CHECK(frames[0]==frames[30]);
        if(m==EQ_SLASH) CHECK(frames[0]==frames[30]);
    }
    for(int scale=1;scale<=3;scale++) for(int k=0;k<6;k++) {
        EquipmentPose p=equipment_pose((EquipmentLoadout){true,true,true,2},EQ_SLASH,k/5.0f,feet,(float)scale);
        capture(r,s,&art,&p,pixels);
    }
    EquipmentPose invalid=equipment_pose((EquipmentLoadout){0},EQ_WALK,0,feet,NAN);
    CHECK(invalid.count==0);
    CHECK(!equipment_draw(&art,r,&invalid));
    EquipmentPose clean=equipment_pose((EquipmentLoadout){0},EQ_IDLE,NAN,feet,1);
    CHECK(clean.count>0);
    EquipmentArt missing;CHECK(!equipment_load(&missing,r,"missing-equipment.png"));equipment_free(&missing);
    equipment_free(&art);equipment_free(&art);
    free(pixels);SDL_DestroyRenderer(r);SDL_FreeSurface(s);SDL_Quit();
    printf("Equipment: %d checks; 24 loadouts, shared grips, three motions, 1/2/3x bounds\n",checks);
    return 0;
}
