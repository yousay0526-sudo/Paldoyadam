#define SDL_MAIN_HANDLED
#define main paldo_app_main
#include "../src/main.c"
#undef main

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while(0)
static Button find(App *a, Action action, int value) {
    for(int i=0;i<a->button_count;i++)
        if(a->buttons[i].action==action && a->buttons[i].value==value) return a->buttons[i];
    CHECK(false); return (Button){0};
}
static void tap(App *a, Button b) {
    Vec2 p={b.rect.x+b.rect.w/2,b.rect.y+b.rect.h/2};
    pointer_down(a,12,p); pointer_up(a,12,p);
}
static bool overlap(SDL_FRect a,SDL_FRect b) {
    return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
}
int main(int argc,char **argv) {
    CHECK(argc==2); SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy"); CHECK(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
    App *a=calloc(1,sizeof *a); CHECK(a);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32); CHECK(surface);
    a->renderer=SDL_CreateSoftwareRenderer(surface); CHECK(a->renderer);
    CHECK(gfx_load(&a->gfx,a->renderer,argv[1]));
    const Vec2 sizes[]={{432,270},{480,306},{568,320},{667,375},{844,390},{1100,650},{1920,1080}};
    for(unsigned view=0;view<sizeof sizes/sizeof sizes[0];view++) {
        a->width=sizes[view].x; a->height=sizes[view].y; a->touch_input=true;
        game_reset(&a->game); game_start(&a->game); a->game.coins=500;
        dispatch(a,ACT_BAG,0); a->button_count=0; draw_bag(a);
        tap(a,find(a,ACT_CULTIVATION,0)); CHECK(a->game.phase==PH_CULTIVATION);
        a->button_count=0; draw_cultivation(a);
        CHECK(a->button_count==9);
        for(int i=0;i<a->button_count;i++) {
            SDL_FRect r=a->buttons[i].rect;
            CHECK(r.x>=0 && r.y>=0 && r.w>0 && r.h>=44);
            CHECK(r.x+r.w<=a->width && r.y+r.h<=a->height);
            for(int j=0;j<i;j++) CHECK(!overlap(r,a->buttons[j].rect));
        }
        tap(a,find(a,ACT_CULTIVATION_SELECT,CUL_EDGE)); a->button_count=0; draw_cultivation(a);
        Button buy=find(a,ACT_CULTIVATION_LEARN,CUL_EDGE); CHECK(!buy.enabled);
        tap(a,buy); CHECK(a->game.coins==500 && !a->game.cultivation_nodes);
        tap(a,find(a,ACT_CULTIVATION_SELECT,CUL_FOUNDATION)); a->button_count=0; draw_cultivation(a);
        tap(a,find(a,ACT_CULTIVATION_LEARN,CUL_FOUNDATION));
        CHECK(a->game.cultivation_nodes==1 && a->game.coins==470 && a->game.hero.hp==100);
        a->button_count=0; draw_cultivation(a);
        CHECK(!find(a,ACT_CULTIVATION_LEARN,CUL_FOUNDATION).enabled);
        tap(a,find(a,ACT_CLOSE,0)); CHECK(a->game.phase==PH_PLAY && !a->target_set);
        a->game.map=MAP_PASS; dispatch(a,ACT_PAUSE,0);
        dispatch(a,ACT_CULTIVATION,0); a->button_count=0; draw_cultivation(a);
        CHECK(a->game.phase==PH_CULTIVATION && !find(a,ACT_CULTIVATION_LEARN,CUL_FOUNDATION).enabled);
        dispatch(a,ACT_PAUSE,0); CHECK(a->game.phase==PH_PLAY);
    }
    gfx_free(&a->gfx); SDL_DestroyRenderer(a->renderer); SDL_FreeSurface(surface); free(a); SDL_Quit();
    printf("PASS: %d cultivation menu checks\n",checks); return 0;
}
