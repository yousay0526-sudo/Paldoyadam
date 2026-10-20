/* Exercise the same atlas renderer used by the native and WebAssembly game. */
#define SDL_MAIN_HANDLED
#include "draw.h"
#include "stb_image.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

/* The map loader must preserve authored pixels, including roofs and canopy
 * fragments later used for occlusion. Catch accidental runtime filtering. */
static void check_map_pixels(Gfx *gfx, SDL_Surface *surface, const char *root) {
    char path[2048];
    snprintf(path, sizeof path, "%s/village.png", root);
    int w, h, channels;
    unsigned char *source = stbi_load(path, &w, &h, &channels, 4);
    CHECK(source != NULL);
    CHECK(w == 1536 && h == 1024);
    unsigned char *pixels = malloc((size_t)surface->w * surface->h * 4);
    CHECK(pixels != NULL);
    const SDL_Rect crops[] = {{100, 50, 288, 288}, {380, 290, 288, 288},
                              {650, 730, 288, 288}};
    for (size_t i = 0; i < sizeof crops / sizeof *crops; i++) {
        CHECK(SDL_RenderCopy(gfx->renderer, gfx->map.texture, &crops[i], NULL) == 0);
        CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, SDL_PIXELFORMAT_RGBA32,
                                  pixels, surface->w * 4) == 0);
        for (int y = 0; y < surface->h; y++)
            CHECK(memcmp(pixels + y * surface->w * 4,
                         source + ((crops[i].y + y) * w + crops[i].x) * 4,
                         (size_t)surface->w * 4) == 0);
    }
    free(pixels);
    stbi_image_free(source);
}

static void capture(Gfx *gfx, const Game *game, SDL_Surface *surface, Uint32 *pixels) {
    SDL_SetRenderDrawColor(gfx->renderer, 11, 15, 19, 255);
    SDL_RenderClear(gfx->renderer);
    gfx_hero(gfx, game, (Vec2){48, 72});
    CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, surface->format->format,
                              pixels, surface->pitch) == 0);
}
/* Inspect actual rendered trail pixels, independent of the upright atlas.
 * A coordinate offset must not move the retreat trail with the hero. */
static void directional_trails(Gfx *gfx, SDL_Surface *surface, Uint32 *pixels) {
    SDL_RenderSetScale(gfx->renderer, 1, 1);
    Uint32 background = SDL_MapRGBA(surface->format, 11, 15, 19, 255);
    for (int move = MOVE_ATTACK; move < TECH_COUNT; move++) {
        for (int direction = 0; direction < 8; direction++) {
            Game game; game_reset(&game); game_start(&game);
            Hero *h = &game.hero;
            h->attacking = move == MOVE_ATTACK; h->skilling = !h->attacking;
            h->technique = move >= 0 ? (Technique)move : TECH_CLEAVE;
            h->attack_angle = h->skill_angle = direction * .785398163f;
            h->attack_elapsed = HERO_ATTACK_IMPACT;
            h->skill_elapsed = HERO_SKILL_IMPACT;
            h->attack_hit = h->skill_hit = true;
            h->impact_start = h->impact_origin = h->pos;
            if (move == TECH_RETREAT) h->pos.x -= 36;
            SDL_SetRenderDrawColor(gfx->renderer, 11, 15, 19, 255); SDL_RenderClear(gfx->renderer);
            gfx_attack_trace(gfx, &game, (Vec2){144,166});
            CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, surface->format->format, pixels, surface->pitch) == 0);
            float cx = 144 + h->impact_origin.x - h->pos.x, cy = 144;
            float radius = game_attack_range(&game, move), half = game_attack_half_angle(move);
            int visible = 0, wrong = 0;
            for (int y = 0; y < surface->h; y++) for (int x = 0; x < surface->w; x++) {
                if (pixels[y * surface->w + x] == background) continue;
                visible++;
                float dx = x - cx, dy = y - cy, r = hypotf(dx,dy);
                float a = atan2f(dy,dx) - h->skill_angle;
                float delta = fabsf(atan2f(sinf(a),cosf(a)));
                if (r > radius + 2 || (r > 5 && delta > half + .25f)) wrong++;
            }
            CHECK(visible > 80 && wrong == 0);
        }
    }
    /* No world-space trail while a menu pauses the attack. */
    Game paused; game_reset(&paused); paused.hero.attacking = true; paused.phase = PH_BAG;
    SDL_SetRenderDrawColor(gfx->renderer, 11, 15, 19, 255); SDL_RenderClear(gfx->renderer);
    gfx_attack_trace(gfx, &paused, (Vec2){144,166});
    CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, surface->format->format, pixels, surface->pitch) == 0);
    for (int i = 0; i < surface->w * surface->h; i++) CHECK(pixels[i] == background);
}

/* Compare the actual hero path with the authored technique, including source
 * cell, foot root and both facing directions. Basic-attack reuse must fail. */
static void technique_atlases(Gfx *gfx, SDL_Surface *surface, Uint32 *pixels, Uint32 *expected) {
    const size_t bytes = (size_t)surface->pitch * surface->h;
    Game game; game_reset(&game); game_start(&game);
    game.hero.skilling = true;
    for (int scale = 1; scale <= 3; scale++) {
        SDL_RenderSetScale(gfx->renderer, (float)scale, (float)scale);
        for (int t = 0; t < TECH_COUNT; t++) {
            Picture *p = &gfx->hero_techniques[t];
            CHECK(p->texture && p->texture != gfx->hero_attack.texture);
            CHECK(p->w == 576 && p->h == 384);
            for (int other = 0; other < t; other++)
                CHECK(p->texture != gfx->hero_techniques[other].texture);
            SDL_ScaleMode sampling;
            CHECK(SDL_GetTextureScaleMode(p->texture, &sampling) == 0);
            CHECK(sampling == SDL_ScaleModeNearest);
            game.hero.technique = (Technique)t;
            game.hero.skill_elapsed = HERO_SKILL_IMPACT;
            CHECK(game_hero_skill_frame(&game) == 2);
            game.hero.skill_elapsed = HERO_SKILL_IMPACT - .01f;
            CHECK(game_hero_skill_frame(&game) < 2);
            unsigned poses = 0;
            for (int tick = 0; tick < 8; tick++) {
                game.hero.skill_elapsed = (tick + .1f) * HERO_SKILL_SECONDS / 8;
                int frame = game_hero_skill_frame(&game);
                poses |= 1u << frame;
                SDL_Rect source = {frame % 3 * 192, frame / 3 * 192, 192, 192};
                for (int face = -1; face <= 1; face += 2) {
                    game.hero.attack_face = face;
                    capture(gfx, &game, surface, pixels);
                    float root = face > 0 ? 1 - 116.0f / 192 : 116.0f / 192;
                    SDL_FRect dest = {48 - root * 80, 72 - 176.0f / 192 * 80, 80, 80};
                    SDL_SetRenderDrawColor(gfx->renderer, 11, 15, 19, 255);
                    SDL_RenderClear(gfx->renderer);
                    SDL_RenderCopyExF(gfx->renderer, p->texture, &source, &dest, 0,
                                      NULL, face > 0 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
                    gfx_attack_trace(gfx, &game, (Vec2){48, 72});
                    CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, surface->format->format,
                                              expected, surface->pitch) == 0);
                    CHECK(memcmp(pixels, expected, bytes) == 0);
                }
            }
            CHECK(poses == (1u << HERO_ATTACK_FRAMES) - 1);
            /* The latest main's contact flash must use the chosen skill
             * silhouette, and leave its texture ready for the next draw. */
            game.hero.hurt_time = HIT_FLASH_SECONDS;
            capture(gfx, &game, surface, pixels);
            CHECK(memcmp(pixels, expected, bytes) != 0);
            Uint8 red, green, blue, alpha; SDL_BlendMode blend;
            SDL_GetTextureColorMod(p->texture, &red, &green, &blue);
            SDL_GetTextureAlphaMod(p->texture, &alpha);
            SDL_GetTextureBlendMode(p->texture, &blend);
            CHECK(red == 255 && green == 255 && blue == 255 && alpha == 255);
            CHECK(blend == SDL_BLENDMODE_BLEND);
            game.hero.hurt_time = 0;
            capture(gfx, &game, surface, pixels);
            CHECK(memcmp(pixels, expected, bytes) == 0);
        }
    }
    SDL_RenderSetScale(gfx->renderer, 1, 1);
}


static void feedback_pixels(Gfx *gfx, SDL_Surface *surface, Uint32 *pixels, Uint32 *baseline) {
    SDL_RenderSetScale(gfx->renderer, 1, 1);
    size_t bytes = (size_t)surface->pitch * surface->h;
    Uint32 background = SDL_MapRGBA(surface->format, 11, 15, 19, 255);
    Game game; game_reset(&game); game_start(&game);
    capture(gfx, &game, surface, baseline);
    game.hero.hurt_time = HIT_FLASH_SECONDS; game.hero.invul = .75f;
    capture(gfx, &game, surface, pixels);
    CHECK(memcmp(pixels, baseline, bytes) != 0);
    for (int i = 0; i < surface->w * surface->h; i++)
        if (baseline[i] == background) CHECK(pixels[i] == background);
    game.hero.hurt_time = game.hero.invul = 0;
    capture(gfx, &game, surface, pixels); CHECK(memcmp(pixels, baseline, bytes) == 0);
    game.hero.hurt_time = HIT_FLASH_SECONDS; game.phase = PH_BAG;
    capture(gfx, &game, surface, pixels); CHECK(memcmp(pixels, baseline, bytes) == 0);
    Picture *actors[] = {&gfx->boar, &gfx->boss};
    for (int actor = 0; actor < 2; actor++) {
        Picture *picture = actors[actor];
        SDL_SetTextureColorMod(picture->texture, 201, 211, 221);
        SDL_SetTextureAlphaMod(picture->texture, 191);
        SDL_SetRenderDrawColor(gfx->renderer, 11, 15, 19, 255); SDL_RenderClear(gfx->renderer);
        gfx_hit_sprite(gfx, picture, (Vec2){144,200}, 83, 96, false, 0);
        CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, surface->format->format, baseline, surface->pitch) == 0);
        SDL_SetRenderDrawColor(gfx->renderer, 11, 15, 19, 255); SDL_RenderClear(gfx->renderer);
        gfx_hit_sprite(gfx, picture, (Vec2){144,200}, 83, 96, false, HIT_FLASH_SECONDS);
        CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, surface->format->format, pixels, surface->pitch) == 0);
        CHECK(memcmp(pixels, baseline, bytes) != 0);
        float fit = fminf(83.0f / picture->w, 96.0f / picture->h);
        for (int y = 0; y < surface->h; y++) for (int x = 0; x < surface->w; x++)
            if (x < 144 - picture->w * fit / 2 - 1 || x > 144 + picture->w * fit / 2 + 1 ||
                y < 200 - picture->h * fit - 1 || y > 201)
                CHECK(pixels[y * surface->w + x] == background);
        Uint8 r, green, b, alpha; SDL_BlendMode blend;
        SDL_GetTextureColorMod(picture->texture, &r, &green, &b);
        SDL_GetTextureAlphaMod(picture->texture, &alpha); SDL_GetTextureBlendMode(picture->texture, &blend);
        CHECK(r == 201 && green == 211 && b == 221 && alpha == 191 && blend == SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(gfx->renderer, 11, 15, 19, 255); SDL_RenderClear(gfx->renderer);
        gfx_hit_sprite(gfx, picture, (Vec2){144,200}, 83, 96, false, 0);
        CHECK(SDL_RenderReadPixels(gfx->renderer, NULL, surface->format->format, pixels, surface->pitch) == 0);
        CHECK(memcmp(pixels, baseline, bytes) == 0);
        SDL_SetTextureColorMod(picture->texture, 255,255,255); SDL_SetTextureAlphaMod(picture->texture, 255);
    }
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
    /* Repaint keeps the source-to-world registration and existing sampling. */
    CHECK(gfx->map.w == 1536 && gfx->map.h == 1024);
    SDL_ScaleMode village_mode;
    CHECK(SDL_GetTextureScaleMode(gfx->map.texture, &village_mode) == 0);
    CHECK(village_mode == SDL_ScaleModeNearest);
    check_map_pixels(gfx, surface, argc > 1 ? argv[1] : "assets");
    size_t bytes = (size_t)surface->pitch * surface->h;
    Uint32 *pixels = malloc(bytes), *previous = malloc(bytes), *idle = malloc(bytes);
    if (!pixels || !previous || !idle) return 1;
    directional_trails(gfx, surface, pixels);
    technique_atlases(gfx, surface, pixels, previous);

    feedback_pixels(gfx, surface, pixels, previous);
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
                        CHECK(x < 120 * scale && y < 120 * scale);
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
