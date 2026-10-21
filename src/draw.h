#ifndef PALDO_DRAW_H
#define PALDO_DRAW_H
#include <SDL.h>
#include "game.h"
#include "stb_truetype.h"

/* World zoom follows the screen height so every screen shows about the same
 * slice of the world: phones (390 tall) stay at 1, a 1080p window zooms to
 * 2.4. It enlarges actors and scenery together; UI stays unchanged. */
#define ZOOM_VIEW_HEIGHT 450.0f
#define ZOOM_MIN 1.0f
#define ZOOM_MAX 3.0f
/* Actors are drawn this much larger than the scenery's own scale so they
 * match its doors, steps and tables. Drawing only; hit areas, ranges and
 * collision stay in world units. */
#define ACTOR_SCALE 1.4f

/* Clockwise, starting at the back view. Each view is independently authored. */
typedef enum {
    HERO_N, HERO_NE, HERO_E, HERO_SE, HERO_S, HERO_SW, HERO_W, HERO_NW,
    HERO_DIRECTIONS
} HeroDirection;

typedef enum {
    MONSTER_BOAR_IDLE, MONSTER_BOAR_WALK, MONSTER_BOAR_ATTACK,
    MONSTER_BUTCHER_IDLE, MONSTER_BUTCHER_WALK, MONSTER_BUTCHER_SKILL,
    MONSTER_ATLAS_COUNT
} MonsterAtlas;

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
    Picture hero_idle_directions, hero_walk_directions[HERO_DIRECTIONS];
    Picture monster_atlases[MONSTER_ATLAS_COUNT];
    Typeface fonts[2];
    int face;
    char root[1024];
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
void gfx_hit_sprite(Gfx *gfx, Picture *picture, Vec2 p, float box_w, float box_h, bool flip, float remaining);
void gfx_attack_trace(Gfx *gfx, const Game *game, Vec2 feet);
void gfx_hero(Gfx *gfx, const Game *game, Vec2 feet);
HeroDirection gfx_hero_direction(Vec2 direction);
/* Returns the complete rotated picture bounds for roof/canopy restoration. */
SDL_FRect gfx_enemy(Gfx *gfx, const Enemy *enemy, Vec2 feet, bool boss, float time, bool live, bool fall_left);
/* Loads the requested prologue illustration on first use and frees the others. */
void gfx_story(Gfx *gfx, int image, float width, float height, float elapsed, float footer_height);
/* Frees every prologue illustration; gfx_story loads them again when needed. */
void gfx_story_release(Gfx *gfx);
void gfx_collision(Gfx *gfx, Vec2 camera);
void gfx_world(Gfx *gfx, const Game *game, float width, float height, Vec2 camera,
               bool target_set, Vec2 target);
Vec2 world_camera(float width, float height, Vec2 focus);
float world_zoom(float screen_height);
Vec2 world_from_screen(Vec2 screen, Vec2 camera, float zoom);
#endif
