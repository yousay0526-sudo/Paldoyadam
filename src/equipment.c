#include "equipment.h"
#include "stb_image.h"
#include <math.h>
#include <string.h>
#define EQ_PI 3.14159265358979323846f
#include "equipment_atlas_data.h"

bool equipment_load(EquipmentArt *art, SDL_Renderer *renderer, const char *path) {
    memset(art, 0, sizeof *art);
    int w, h, channels;
    unsigned char *data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) return false;
    if (w != 1024 || h != 1024) { stbi_image_free(data); return false; }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(data, w, h, 32, w*4, SDL_PIXELFORMAT_RGBA32);
    if (surface) {
        art->texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
    stbi_image_free(data);
    if (!art->texture) return false;
    SDL_SetTextureBlendMode(art->texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(art->texture, SDL_ScaleModeLinear);
    return true;
}
void equipment_free(EquipmentArt *art) {
    SDL_DestroyTexture(art->texture);
    memset(art, 0, sizeof *art);
}
static SDL_FPoint rotated(SDL_FPoint p, float degrees) {
    float a = degrees * EQ_PI / 180, c = cosf(a), s = sinf(a);
    return (SDL_FPoint){p.x*c-p.y*s, p.x*s+p.y*c};
}
static SDL_FPoint plus(SDL_FPoint a, SDL_FPoint b) {
    return (SDL_FPoint){a.x+b.x, a.y+b.y};
}
static void add(EquipmentPose *p, EquipmentPart part, SDL_FPoint root,
                float w, float h, float ax, float ay, float degrees) {
    EquipmentLayer *l = &p->layers[p->count++];
    *l = (EquipmentLayer){part, {root.x-w*ax, root.y-h*ay, w, h}, {w*ax, h*ay}, degrees, false};
}
static float key(const float *keys, float phase) {
    float x = phase*5; int i = (int)x;
    if (i >= 5) return keys[5];
    float t = x-i; t = t*t*(3-2*t);
    return keys[i]+(keys[i+1]-keys[i])*t;
}
static SDL_FPoint arm(EquipmentPose *p, bool right, SDL_FPoint shoulder, float upper, float fore) {
    float sign = right ? 1 : -1;
    EquipmentPart u = right ? EQ_RIGHT_UPPER : EQ_LEFT_UPPER;
    EquipmentPart f = right ? EQ_RIGHT_FORE : EQ_LEFT_FORE;
    SDL_FPoint elbow = plus(shoulder, rotated((SDL_FPoint){-sign*2, 23}, upper));
    /* Forearm angle is relative to its parent's bone, not a second world axis. */
    add(p, u, shoulder, 17, 32, .5f, .08f, upper);
    add(p, f, elbow, 14, 31, .5f, .08f, upper+fore);
    return plus(elbow, rotated((SDL_FPoint){0, 24}, upper+fore));
}
EquipmentPose equipment_pose(EquipmentLoadout e, EquipmentMotion motion, float phase,
                             SDL_FPoint feet, float scale) {
    EquipmentPose p = {0};
    if (!isfinite(phase)) phase = 0;
    if (!isfinite(scale) || scale <= 0 || !isfinite(feet.x) || !isfinite(feet.y)) return p;
    if (motion == EQ_SLASH) phase = fminf(1, fmaxf(0, phase));
    else phase -= floorf(phase);
    float step = motion == EQ_WALK ? sinf(phase*2*EQ_PI) : 0;
    float bob = motion == EQ_WALK ? -1.5f*(1-cosf(phase*4*EQ_PI)) : 0;
    SDL_FPoint hip = {64, 109+bob};
    for (int side=0; side<2; side++) {
        float sign = side ? -1 : 1;
        EquipmentPart part = e.lower ? (side ? EQ_LEFT_ARMOR_LEG : EQ_RIGHT_ARMOR_LEG)
                                     : (side ? EQ_LEFT_LEG : EQ_RIGHT_LEG);
        /* Modest cutout swing, not a claim of authored eight-view locomotion. */
        add(&p, part, (SDL_FPoint){64-sign*11,109}, 27, 69, .5f, .03f, sign*step*7);
    }
    add(&p, EQ_PELVIS, hip, 48, 31, .5f, .25f, 0);
    float upper = 12+step*8, fore = -12;
    if (motion == EQ_SLASH) {
        static const float u[6] = {12,115,165,-35,-8,12};
        static const float f[6] = {-12,30,45,-55,-20,-12};
        upper = key(u,phase); fore = key(f,phase);
    }
    arm(&p, false, (SDL_FPoint){87,67+bob}, -8-step*8, 5);
    add(&p, EQ_TUNIC, (SDL_FPoint){64,56+bob}, 46, 58, .5f, 0, 0);
    if (e.upper) add(&p, EQ_ARMOR, (SDL_FPoint){64,56+bob}, 52, 59, .5f, 0, 0);
    add(&p, EQ_SASH, (SDL_FPoint){64,103+bob}, 56, 43, .5f, .22f, 0);
    add(&p, EQ_HEAD, (SDL_FPoint){64,63+bob}, 36, 53, .5f, 1, 0);
    if (e.helmet) add(&p, EQ_HELMET, (SDL_FPoint){64,39+bob}, 44, 55, .5f, .55f, 0);
    p.right_hand = arm(&p, true, (SDL_FPoint){41,67+bob}, upper, fore);
    EquipmentLayer right_fore = p.layers[p.count-1];
    if (e.weapon >= 1 && e.weapon <= 2) {
        p.weapon_grip = p.right_hand;
        add(&p, e.weapon == 1 ? EQ_PLAIN_SWORD : EQ_ORNATE_SWORD,
            p.weapon_grip, 17, 69, .38f, .18f, upper+fore-18);
        /* Redraw only the grip-hand region over the hilt, using the exact
         * same forearm transform. The full forearm must not duplicate. */
        right_fore.hand_only = true;
        p.layers[p.count++] = right_fore;
    }
    /* Transform every layer and compute the full rotated bounds for clipping. */
    float minx=1e9f, miny=1e9f, maxx=-1e9f, maxy=-1e9f;
    for (int i=0;i<p.count;i++) {
        EquipmentLayer *l=&p.layers[i];
        l->box.x=feet.x+(l->box.x-64)*scale;
        l->box.y=feet.y+(l->box.y-176)*scale;
        l->box.w*=scale; l->box.h*=scale; l->pivot.x*=scale; l->pivot.y*=scale;
        SDL_FPoint origin={l->box.x+l->pivot.x,l->box.y+l->pivot.y};
        for(int c=0;c<4;c++) {
            SDL_FPoint v=rotated((SDL_FPoint){(c&1?l->box.w:0)-l->pivot.x,
                (c&2?l->box.h:0)-l->pivot.y}, (float)l->angle);
            v=plus(origin,v);
            minx=fminf(minx,v.x); maxx=fmaxf(maxx,v.x);
            miny=fminf(miny,v.y); maxy=fmaxf(maxy,v.y);
        }
    }
    p.right_hand=(SDL_FPoint){feet.x+(p.right_hand.x-64)*scale,feet.y+(p.right_hand.y-176)*scale};
    p.weapon_grip=(SDL_FPoint){feet.x+(p.weapon_grip.x-64)*scale,feet.y+(p.weapon_grip.y-176)*scale};
    p.bounds=(SDL_FRect){minx,miny,maxx-minx,maxy-miny};
    return p;
}
bool equipment_draw(const EquipmentArt *art, SDL_Renderer *r, const EquipmentPose *p) {
    if (!art->texture || p->count <= 0) return false;
    for (int i=0;i<p->count;i++) {
        const EquipmentLayer *l=&p->layers[i];
        SDL_Rect source=EQUIPMENT_RECTS[l->part];
        SDL_FRect box=l->box; SDL_FPoint pivot=l->pivot;
        /* Last layer is the original hand, crop away its sleeve so it cannot
         * cover armor. Preserve the transform about its original elbow. */
        if (l->hand_only) {
            int trim=(int)(source.h*.72f);
            float dy=box.h*trim/source.h;
            source.y+=trim; source.h-=trim;
            box.h-=dy; pivot.y-=dy;
            /* Pivot is expressed relative to the unrotated destination. */
            box.x=l->box.x; box.y=l->box.y+dy;
        }
        if (SDL_RenderCopyExF(r,art->texture,&source,&box,l->angle,&pivot,SDL_FLIP_NONE)<0) return false;
    }
    return true;
}
