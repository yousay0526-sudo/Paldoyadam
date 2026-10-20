#ifndef PALDO_DRAW_H
#define PALDO_DRAW_H
#include <SDL.h>
#include "game.h"
#include "stb_truetype.h"

/* Uniform world zoom enlarges actors and scenery together; UI stays unchanged. */
#define WORLD_ZOOM 1.20f

typedef struct {
    SDL_Texture *texture;
    int w, h;
} Picture;
typedef struct {
    int codepoint, w, h, xoff, yoff;
    float advance;
    SDL_Texture *texture;
} Glyph;
typedef struct {
    unsigned char *font_bytes;
    stbtt_fontinfo font;
    Glyph glyphs[1024];
    int glyph_count;
    float font_scale, ascent;
} Typeface;
typedef struct {
    SDL_Renderer *renderer;
    Picture map, hero, hero_walk, hero_attack, messenger, boss, boar, npcs, prologue[3];
    Picture hero_techniques[TECH_COUNT];
    Typeface fonts[2];
    int face;
} Gfx;
extern const SDL_Color INK, PAPER, GOLD, MUTED, RED;
bool gfx_load(Gfx *gfx, SDL_Renderer *renderer, const char *asset_root);
void gfx_free(Gfx *gfx);
void gfx_rect(Gfx *gfx, SDL_FRect r, SDL_Color c);
void gfx_border(Gfx *gfx, SDL_FRect r, SDL_Color c);
void gfx_round(Gfx *gfx, SDL_FRect r, float radius, SDL_Color c);
void gfx_round_border(Gfx *gfx, SDL_FRect r, float radius, SDL_Color c);
void gfx_line(Gfx *gfx, Vec2 a, Vec2 b, SDL_Color c);
void gfx_circle(Gfx *gfx, Vec2 p, float radius, SDL_Color c);
/* High-resolution filled disc and matching outline for the attack control. */
void gfx_disc(Gfx *gfx, Vec2 p, float radius, SDL_Color fill, SDL_Color rim);
void gfx_sector(Gfx *gfx, Vec2 p, float radius, float angle, float half, SDL_Color c);
float gfx_measure(Gfx *gfx, const char *text, float size);
float gfx_text(Gfx *gfx, const char *text, float x, float y, float size, float max_width, SDL_Color c);
float gfx_text_height(Gfx *gfx, const char *text, float size, float max_width);
float gfx_text_box(Gfx *gfx, const char *text, SDL_FRect bounds, float size, float min_size, SDL_Color c);
void gfx_center(Gfx *gfx, const char *text, float x, float y, float size, SDL_Color c);
void gfx_label(Gfx *gfx, const char *text, SDL_FRect r, float size, SDL_Color c);
void gfx_heading(Gfx *gfx, const char *text, SDL_FRect r, float size, SDL_Color c);
void gfx_sprite(Gfx *gfx, Picture *picture, Vec2 p, float box_w, float box_h, bool flip);
void gfx_attack_trace(Gfx *gfx, const Game *game, Vec2 feet);
void gfx_hero(Gfx *gfx, const Game *game, Vec2 feet);
void gfx_story(Gfx *gfx, int image, float width, float height, float elapsed, float footer_height);
void gfx_collision(Gfx *gfx, Vec2 camera);
void gfx_world(Gfx *gfx, const Game *game, float width, float height, Vec2 camera,
               bool target_set, Vec2 target);
Vec2 world_camera(float width, float height, Vec2 focus);
Vec2 world_from_screen(Vec2 screen, Vec2 camera);
#endif
