/* Exercise the same atlas renderer used by the native and WebAssembly game. */
#define SDL_MAIN_HANDLED
#include "draw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static void capture(Gfx *gfx, const Game *game, SDL_Surface *surface, Uint32 *pixels) {
    SDL_SetRenderDrawColor(gfx->renderer, 11, 15, 19, 255);
    SDL_RenderClear(gfx->renderer);
    gfx_hero(gfx, game, (Vec2){48, 72});
    CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, surface->format->format,
                              pixels, surface->pitch) == 0);
}

int main(int argc, char **argv) {
    SDL_SetMainReady();
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 288, 288, 32, SDL_PIXELFORMAT_RGBA32);
    Gfx *gfx = calloc(1, sizeof *gfx);
    if (!surface || !gfx) return 1;
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer || !gfx_load(gfx, renderer, argc > 1 ? argv[1] : "assets")) return 1;
    /* The village remains at its original source size; only its color detail
     * changes. Nearest sampling preserves the established pixel-RPG style. */
    CHECK(gfx->map.w == 1536 && gfx->map.h == 1024);
    SDL_ScaleMode village_mode;
    CHECK(SDL_GetTextureScaleMode(gfx->map.texture, &village_mode) == 0);
    CHECK(village_mode == SDL_ScaleModeNearest);
    size_t bytes = (size_t)surface->pitch * surface->h;
    Uint32 *pixels = malloc(bytes), *previous = malloc(bytes), *idle = malloc(bytes);
    if (!pixels || !previous || !idle) return 1;
    Uint32 background = SDL_MapRGBA(surface->format, 11, 15, 19, 255);
    for (int scale = 1; scale <= 3; scale++) {
        SDL_RenderSetScale(renderer, (float)scale, (float)scale);
        Game game;
        game_reset(&game); game_start(&game);
        capture(gfx, &game, surface, idle);
        /* Source artwork faces left: right-facing idle must be its reflection. */
        SDL_SetRenderDrawColor(renderer, 11, 15, 19, 255);
        SDL_RenderClear(renderer);
        SDL_Rect original = {0, 0, gfx->hero.w, gfx->hero.h};
        SDL_FRect expected = {25, 26, 46, 46};
        SDL_RenderCopyExF(renderer, gfx->hero.texture, &original, &expected,
                          0, NULL, SDL_FLIP_HORIZONTAL);
        CHECK(SDL_RenderReadPixels(renderer, NULL, surface->format->format,
                                  pixels, surface->pitch) == 0);
        CHECK(memcmp(pixels, idle, bytes) == 0);
        for (int frame = 0; frame < HERO_WALK_FRAMES; frame++) {
            game.hero.walking = true;
            game.hero.walk_cycle = (frame+.1f)/HERO_WALK_FRAMES;
            capture(gfx, &game, surface, pixels);
            CHECK(memcmp(pixels, idle, bytes) != 0);
            if (frame) CHECK(memcmp(pixels, previous, bytes) != 0);
            memcpy(previous, pixels, bytes);
            int inside = 0, outside = 0;
            for (int y = 0; y < surface->h; y++)
                for (int x = 0; x < surface->w; x++) {
                    if (pixels[y*surface->w+x] == background) continue;
                    if (x >= 25*scale && x < 71*scale && y >= 26*scale && y < 72*scale) inside++;
                    else outside++;
                }
            CHECK(inside > 100 && outside == 0);
            game.hero.face = -1;
            capture(gfx, &game, surface, pixels);
            CHECK(memcmp(pixels, previous, bytes) != 0);
            game.hero.face = 1;
        }
        game.hero.walking = false;
        capture(gfx, &game, surface, pixels);
        CHECK(memcmp(pixels, idle, bytes) == 0);
        game.hero.walking = true; game.phase = PH_PAUSED;
        capture(gfx, &game, surface, pixels);
        CHECK(memcmp(pixels, idle, bytes) == 0);
        game.phase = PH_PLAY; game.hero.walking = false;
        game.hero.attacking = true; game.hero.attack_face = 1;
        for (int frame = 0; frame < HERO_ATTACK_FRAMES; frame++) {
            game.hero.attack_elapsed = (frame + .1f) * HERO_ATTACK_SECONDS / HERO_ATTACK_FRAMES;
            capture(gfx, &game, surface, pixels);
            CHECK(memcmp(pixels, idle, bytes) != 0);
            if (frame) CHECK(memcmp(pixels, previous, bytes) != 0);
            memcpy(previous, pixels, bytes);
            int visible = 0;
            for (int y = 0; y < surface->h; y++)
                for (int x = 0; x < surface->w; x++)
                    if (pixels[y * surface->w + x] != background) {
                        visible++;
                        CHECK(x < 96 * scale && y < 80 * scale);
                    }
            CHECK(visible > 100);
            game.hero.attack_face = -1;
            capture(gfx, &game, surface, pixels);
            CHECK(memcmp(pixels, previous, bytes) != 0);
            game.hero.attack_face = 1;
        }
        game.hero.attacking = false; game.hero.skilling = true;
        for (int t = 0; t < TECH_COUNT; t++) {
            game.hero.technique = (Technique)t; game.hero.skill_angle = 0;
            for (int tick = 0; tick < 8; tick++) {
                game.hero.skill_elapsed = (tick+.1f) * HERO_SKILL_SECONDS / 8;
                CHECK(game_hero_skill_frame(&game) >= 0 && game_hero_skill_frame(&game) < HERO_ATTACK_FRAMES);
                capture(gfx, &game, surface, pixels);
                CHECK(memcmp(pixels, idle, bytes) != 0);
                if (tick == 0) memcpy(previous, pixels, bytes);
                if (tick == 4) CHECK(memcmp(pixels, previous, bytes) != 0);
            }
            game.hero.skill_elapsed = .30f; game.hero.attack_face = 1;
            capture(gfx, &game, surface, previous);
            game.hero.attack_face = -1; game.hero.skill_angle = 3.14159265f;
            capture(gfx, &game, surface, pixels);
            CHECK(memcmp(pixels, previous, bytes) != 0);
            game.hero.attack_face = 1;
        }
        game.phase = PH_BAG;
        capture(gfx, &game, surface, pixels);
        CHECK(memcmp(pixels, idle, bytes) == 0);
    }
    free(pixels); free(previous); free(idle);
    gfx_free(gfx); free(gfx);
    SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface); SDL_Quit();
    printf("PASS: %d hero rendering checks: walk/attack/four techniques, bounds, flip and idle at 1/2/3x.\n", checks);
    return 0;
}
