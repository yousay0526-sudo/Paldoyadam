/* Production SDL main loop, real mouse/key events, isolated fixture save.
 * After loading the fixture no test writes Game or dispatches actions. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p,0700)
#endif
static void *observed_app;
static const char *profile;
static int stage,checks;
static bool writing,finished;
static Uint32 started,last_input;
#define CHECK(c) do { checks++; if(!(c)) { fprintf(stderr,"FAIL economy SDL:%d stage=%d: %s\n",__LINE__,stage,#c); exit(1); } } while(0)
static void *observe_app(size_t n,size_t size) { void *p=calloc(n,size); if(!observed_app) observed_app=p; return p; }
static char *test_pref(const char *org,const char *app) {
    (void)org;(void)app;char *p=SDL_malloc(strlen(profile)+2); if(p) sprintf(p,"%s/",profile); return p;
}
static void observed_present(SDL_Renderer *r);
#define calloc observe_app
#define SDL_GetPrefPath test_pref
#define SDL_RenderPresent observed_present
#define main production_main
#include "../src/main.c"
#undef main
#undef calloc
#undef SDL_GetPrefPath
#undef SDL_RenderPresent
static void key_event(SDL_Keycode key) {
    SDL_Event e={0}; e.type=SDL_KEYDOWN;e.key.keysym.sym=key;CHECK(SDL_PushEvent(&e)==1);
    e.type=SDL_KEYUP;CHECK(SDL_PushEvent(&e)==1);
}
static void tap(Vec2 p) {
    SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=(int)p.x;e.button.y=(int)p.y;CHECK(SDL_PushEvent(&e)==1);
    e.type=SDL_MOUSEBUTTONUP;CHECK(SDL_PushEvent(&e)==1);
}
static void control(App *a,Action action,int value) {
    for(int i=0;i<a->button_count;i++) { Button b=a->buttons[i];
        if(b.enabled&&b.action==action&&b.value==value) { tap((Vec2){b.rect.x+b.rect.w/2,b.rect.y+b.rect.h/2}); return; }
    } CHECK(false);
}
static void walk(App *a,Vec2 goal) {
    if(SDL_GetTicks()-last_input<150) return;
    Route route;CHECK(world_route(a->game.hero.pos,goal,HERO_RADIUS,&route));
    Vec2 next=goal;
    for(int i=0;i<route.count;i++) if(distance(route.points[i],a->game.hero.pos)>12) { next=route.points[i];break; }
    float d=distance(a->game.hero.pos,next),step=fminf(80,d);
    if(d>0) next=(Vec2){a->game.hero.pos.x+(next.x-a->game.hero.pos.x)*step/d,a->game.hero.pos.y+(next.y-a->game.hero.pos.y)*step/d};
    float z=world_zoom(a->height);Vec2 p={(next.x-a->camera.x)*z,(next.y-a->camera.y)*z};
    CHECK(p.x>=0&&p.x<a->width&&p.y>=0&&p.y<a->height);tap(p);last_input=SDL_GetTicks();
}
static void finish(void) { SDL_Event e={0};e.type=SDL_QUIT;CHECK(SDL_PushEvent(&e)==1);finished=true; }
static void observed_present(SDL_Renderer *r) {
    SDL_RenderPresent(r); App *a=observed_app;
    if(!started) started=SDL_GetTicks();
    if(SDL_GetTicks()-started>=55000) fprintf(stderr,"state: map=%d pos=%.1f,%.1f phase=%d hp=%d hunt=%d mats=%d\n",a->game.map,a->game.hero.pos.x,a->game.hero.pos.y,a->game.phase,a->game.hero.hp,a->game.hunt[0].hp,a->game.materials[0]);
    CHECK(SDL_GetTicks()-started<55000);
    if(finished) return;
    if(stage==0&&a->game.phase==PH_TITLE) { CHECK(a->save_available);key_event(SDLK_RETURN);stage=1;return; }
    if(!writing) {
        if(stage==1&&a->game.phase==PH_PLAY) {
            CHECK(a->game.map==MAP_VILLAGE&&a->game.materials[0]==0&&a->game.materials[1]==0&&a->game.coins==67);
            CHECK(a->game.quest==Q_RECOVER&&a->game.herbs==0);finish();
        } return;
    }
    if(stage==1&&a->game.phase==PH_PLAY) {
        if(a->game.hunt[0].hp>0) {
            if(SDL_GetTicks()-last_input>450) { key_event(SDLK_a);last_input=SDL_GetTicks(); }
        } else if(a->game.materials[0]==1) {
            CHECK(a->game.coins==64&&a->game.herbs==0&&a->game.quest==Q_RECOVER);
            key_event(SDLK_i);stage=2;
        }
    } else if(stage==2&&a->game.phase==PH_BAG) { control(a,ACT_BAG_TAB,2);stage=3;
    } else if(stage==3&&a->game.phase==PH_BAG) { CHECK(a->bag_tab==2);control(a,ACT_CLOSE,0);stage=4;
    } else if(stage==4&&a->game.phase==PH_PLAY) {
        if(a->game.map==MAP_PASS) {
            if(game_exit_nearby(&a->game)==2) key_event(SDLK_SPACE); else walk(a,MAP_EXITS[2].pos);
        } else if(game_nearby(&a->game).kind==NEAR_NPC&&game_nearby(&a->game).index==SMITH) { key_event(SDLK_SPACE);stage=5;
        } else walk(a,NPCS[SMITH].pos);
    } else if(stage==5&&a->game.phase==PH_SHOP) { control(a,ACT_SMITH_TAB,1);stage=6;
    } else if(stage==6&&a->smith_selling) { control(a,ACT_SELL_ONE,0);stage=7;
    } else if(stage==7&&a->game.materials[0]==0) {
        CHECK(a->game.coins==67);control(a,ACT_CLOSE,0);stage=8;
    } else if(stage==8&&a->game.phase==PH_PLAY) { key_event(SDLK_ESCAPE);stage=9;
    } else if(stage==9&&a->game.phase==PH_PAUSED) { control(a,ACT_TITLE,0);stage=10;
    } else if(stage==10&&a->game.phase==PH_TITLE) { key_event(SDLK_RETURN);stage=11;
    } else if(stage==11&&a->game.phase==PH_PLAY) {
        CHECK(a->game.coins==67&&a->game.materials[0]==0&&a->game.herbs==0);finish();
    }
}
int main(int argc,char **argv) {
    CHECK(argc==5);writing=!strcmp(argv[1],"write");profile=argv[2];MAKE_DIR(profile);
    if(writing) {
        unsigned char b[SAVE_CAPACITY];FILE *f=fopen(argv[4],"rb");CHECK(f);
        size_t n=fread(b,1,sizeof b,f);CHECK(!fclose(f));Game g;CHECK(save_decode(&g,b,n)==SAVE_OK);
        SaveStore store;save_store_init(&store,profile);CHECK(save_store_write(&store,&g)==SAVE_OK);
    }
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");
    char *args[]={argv[0],"--assets",argv[3],"--size","1100x650"};
    CHECK(production_main(5,args)==0&&finished);
    printf("PASS economy SDL %s: real attack, magnet, bag, walk, sale, continue; %d checks\n",argv[1],checks);
    return 0;
}
