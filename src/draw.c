#include "draw.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define PI 3.14159265358979323846f

const SDL_Color INK = {16, 34, 38, 244}, PAPER = {238, 227, 202, 255}, GOLD = {227, 193, 131, 255},
                MUTED = {174, 194, 172, 255}, RED = {190, 89, 80, 255};
static unsigned char *read_asset(const char *path, size_t *length) {
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (!rw)
        return NULL;
    Sint64 n = SDL_RWsize(rw);
    if (n <= 0 || n > 64 * 1024 * 1024) {
        SDL_RWclose(rw);
        return NULL;
    }
    unsigned char *data = malloc((size_t)n);
    if (!data) {
        SDL_RWclose(rw);
        return NULL;
    }
    size_t got = SDL_RWread(rw, data, 1, (size_t)n);
    SDL_RWclose(rw);
    if (got != (size_t)n) {
        free(data);
        return NULL;
    }
    *length = got;
    return data;
}
static void asset_path(char *out, size_t n, const char *root, const char *name) {
    snprintf(out, n, "%s%s%s", root, root[0] ? "/" : "", name);
}
static Picture load_picture(Gfx *g, const char *path) {
    Picture p = {0};
    size_t size = 0;
    unsigned char *data = read_asset(path, &size);
    if (!data)
        return p;
    int channels;
    unsigned char *pixels = stbi_load_from_memory(data, (int)size, &p.w, &p.h, &channels, 4);
    free(data);
    if (!pixels)
        return p;
    SDL_Surface *surface =
        SDL_CreateRGBSurfaceWithFormatFrom(pixels, p.w, p.h, 32, p.w * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface) {
        p.texture = SDL_CreateTextureFromSurface(g->renderer, surface);
        SDL_FreeSurface(surface);
    }
    stbi_image_free(pixels);
    if (p.texture)
        SDL_SetTextureBlendMode(p.texture, SDL_BLENDMODE_BLEND);
    return p;
}
bool gfx_load(Gfx *g, SDL_Renderer *renderer, const char *root) {
    memset(g, 0, sizeof *g);
    g->renderer = renderer;
    char path[2048];
    Picture *pictures[] = {&g->map,  &g->hero,        &g->boss,        &g->boar,
                           &g->npcs, &g->prologue[0], &g->prologue[1], &g->prologue[2]};
    const char *names[] = {"village.png",
                           "warrior.png",
                           "butcher.png",
                           "minion.png",
                           "npcs.png",
                           "prologue/01-homeward.png",
                           "prologue/02-hunger.png",
                           "prologue/03-reflection.png"};
    for (int i = 0; i < 8; i++) {
        asset_path(path, sizeof path, root, names[i]);
        *pictures[i] = load_picture(g, path);
        if (!pictures[i]->texture) {
            SDL_Log("Cannot load %s: %s", path, SDL_GetError());
            gfx_free(g);
            return false;
        }
        if (i >= 5)
            SDL_SetTextureScaleMode(pictures[i]->texture, SDL_ScaleModeLinear);
    }
    const char *font_names[] = {"fonts/NanumMyeongjo-Bold.ttf", "fonts/NanumBrushScript-Regular.ttf"};
    for (int i = 0; i < 2; i++) {
        Typeface *face = &g->fonts[i];
        asset_path(path, sizeof path, root, font_names[i]);
        size_t size;
        face->font_bytes = read_asset(path, &size);
        if (!face->font_bytes || !stbtt_InitFont(&face->font, face->font_bytes,
                stbtt_GetFontOffsetForIndex(face->font_bytes, 0))) {
            SDL_Log("Cannot load Korean font %s", path);
            gfx_free(g);
            return false;
        }
        face->font_scale = stbtt_ScaleForMappingEmToPixels(&face->font, 80);
        int ascent;
        stbtt_GetFontVMetrics(&face->font, &ascent, NULL, NULL);
        face->ascent = (float)ascent * face->font_scale;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    return true;
}
void gfx_free(Gfx *g) {
    Picture *pictures[] = {&g->map,  &g->hero,        &g->boss,        &g->boar,
                           &g->npcs, &g->prologue[0], &g->prologue[1], &g->prologue[2]};
    for (int i = 0; i < 8; i++) {
        if (pictures[i]->texture)
            SDL_DestroyTexture(pictures[i]->texture);
        pictures[i]->texture = NULL;
    }
    for (int j = 0; j < 2; j++) {
        Typeface *face = &g->fonts[j];
        for (int i = 0; i < face->glyph_count; i++)
            if (face->glyphs[i].texture)
                SDL_DestroyTexture(face->glyphs[i].texture);
        face->glyph_count = 0;
        free(face->font_bytes);
        face->font_bytes = NULL;
    }
}
static void color(Gfx *g, SDL_Color c) {
    SDL_SetRenderDrawColor(g->renderer, c.r, c.g, c.b, c.a);
}
void gfx_rect(Gfx *g, SDL_FRect r, SDL_Color c) {
    color(g, c);
    SDL_RenderFillRectF(g->renderer, &r);
}
void gfx_border(Gfx *g, SDL_FRect r, SDL_Color c) {
    color(g, c);
    SDL_RenderDrawRectF(g->renderer, &r);
}
static void round_points(SDL_FRect r, float radius, SDL_FPoint *p) {
    radius = fminf(radius, fminf(r.w, r.h) / 2);
    for (int corner = 0; corner < 4; corner++) {
        float cx = r.x + (corner == 0 || corner == 3 ? r.w - radius : radius);
        float cy = r.y + (corner < 2 ? r.h - radius : radius);
        for (int j = 0; j <= 10; j++) {
            float angle = (corner + j / 10.0f) * PI / 2;
            p[corner * 11 + j] = (SDL_FPoint){cx + cosf(angle) * radius, cy + sinf(angle) * radius};
        }
    }
    p[44] = p[0];
}
void gfx_round(Gfx *g, SDL_FRect r, float radius, SDL_Color c) {
    if (r.w <= 0 || r.h <= 0) return;
    SDL_FPoint points[45];
    SDL_Vertex vertices[46];
    int indices[132];
    round_points(r, radius, points);
    vertices[0] = (SDL_Vertex){{r.x + r.w / 2, r.y + r.h / 2}, c, {0, 0}};
    for (int i = 0; i < 45; i++)
        vertices[i + 1] = (SDL_Vertex){points[i], c, {0, 0}};
    for (int i = 0; i < 44; i++) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(g->renderer, NULL, vertices, 46, indices, 132);
}
void gfx_round_border(Gfx *g, SDL_FRect r, float radius, SDL_Color c) {
    SDL_FPoint points[45];
    round_points(r, radius, points);
    color(g, c);
    SDL_RenderDrawLinesF(g->renderer, points, 45);
}
void gfx_line(Gfx *g, Vec2 a, Vec2 b, SDL_Color c) {
    color(g, c);
    SDL_RenderDrawLineF(g->renderer, a.x, a.y, b.x, b.y);
}
void gfx_circle(Gfx *g, Vec2 p, float radius, SDL_Color c) {
    SDL_FPoint points[33];
    for (int i = 0; i <= 32; i++) {
        float a = (float)i * PI / 16;
        points[i] = (SDL_FPoint){p.x + cosf(a) * radius, p.y + sinf(a) * radius};
    }
    color(g, c);
    SDL_RenderDrawLinesF(g->renderer, points, 33);
}
void gfx_sector(Gfx *g, Vec2 p, float radius, float angle, float half, SDL_Color c) {
    SDL_Vertex vertices[34];
    int indices[96];
    vertices[0] = (SDL_Vertex){{p.x, p.y}, c, {0, 0}};
    for (int i = 0; i <= 32; i++) {
        float a = angle - half + half * 2 * (float)i / 32;
        vertices[i + 1] = (SDL_Vertex){{p.x + cosf(a) * radius, p.y + sinf(a) * radius}, c, {0, 0}};
        if (i < 32) {
            indices[i * 3] = 0;
            indices[i * 3 + 1] = i + 1;
            indices[i * 3 + 2] = i + 2;
        }
    }
    SDL_RenderGeometry(g->renderer, NULL, vertices, 34, indices, 96);
}
/* Decode UTF-8 once per glyph; all dialogue remains readable Korean source text. */
static int next_codepoint(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    int c = *p++;
    if (c >= 0xc2 && c < 0xe0 && p[0] && ((p[0] & 0xc0) == 0x80)) {
        c = ((c & 31) << 6) | (p[0] & 63);
        p++;
    } else if (c >= 0xe0 && c < 0xf0 && p[0] && p[1] && ((p[0] & 0xc0) == 0x80) && ((p[1] & 0xc0) == 0x80)) {
        c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63);
        p += 2;
    } else if (c >= 0xf0 && c < 0xf5 && p[0] && p[1] && p[2] && ((p[0] & 0xc0) == 0x80) &&
               ((p[1] & 0xc0) == 0x80) && ((p[2] & 0xc0) == 0x80)) {
        c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63);
        p += 3;
    }
    *s = (const char *)p;
    return c;
}
static Glyph *glyph(Gfx *g, int codepoint) {
    Typeface *face = &g->fonts[g->face];
    for (int i = 0; i < face->glyph_count; i++)
        if (face->glyphs[i].codepoint == codepoint)
            return &face->glyphs[i];
    if (face->glyph_count >= 1024)
        return NULL;
    Glyph *out = &face->glyphs[face->glyph_count++];
    memset(out, 0, sizeof *out);
    out->codepoint = codepoint;
    int advance;
    stbtt_GetCodepointHMetrics(&face->font, codepoint, &advance, NULL);
    out->advance = (float)advance * face->font_scale;
    unsigned char *bitmap = stbtt_GetCodepointBitmap(&face->font, 0, face->font_scale, codepoint, &out->w, &out->h,
                                                     &out->xoff, &out->yoff);
    if (bitmap && out->w > 0 && out->h > 0) {
        size_t count = (size_t)out->w * (size_t)out->h;
        unsigned char *rgba = malloc(count * 4);
        if (rgba) {
            for (size_t i = 0; i < count; i++) {
                rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
                rgba[i * 4 + 3] = bitmap[i];
            }
            out->texture = SDL_CreateTexture(g->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                             out->w, out->h);
            if (out->texture) {
                SDL_UpdateTexture(out->texture, NULL, rgba, out->w * 4);
                SDL_SetTextureBlendMode(out->texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(out->texture, SDL_ScaleModeLinear);
            }
            free(rgba);
        }
    }
    stbtt_FreeBitmap(bitmap, NULL);
    return out;
}
float gfx_measure(Gfx *g, const char *text, float size) {
    float line = 0, best = 0;
    while (*text) {
        int c = next_codepoint(&text);
        if (c == '\n') {
            best = fmaxf(best, line);
            line = 0;
            continue;
        }
        Glyph *v = glyph(g, c);
        if (v)
            line += v->advance * size / 80;
    }
    return fmaxf(best, line);
}
float gfx_text(Gfx *g, const char *text, float x, float y, float size, float max_width, SDL_Color c) {
    float start = x, top = y, scale = size / 80;
    while (*text) {
        int cp = next_codepoint(&text);
        if (cp == '\n') {
            x = start;
            y += size * 1.5f;
            continue;
        }
        Glyph *v = glyph(g, cp);
        if (!v)
            continue;
        if (max_width > 0 && x > start && x + v->advance * scale > start + max_width) {
            x = start;
            y += size * 1.5f;
            if (cp == ' ')
                continue;
        }
        if (v->texture) {
            SDL_FRect r = {x + (float)v->xoff * scale, y + (g->fonts[g->face].ascent + (float)v->yoff) * scale,
                           (float)v->w * scale, (float)v->h * scale};
            SDL_SetTextureColorMod(v->texture, c.r, c.g, c.b);
            SDL_SetTextureAlphaMod(v->texture, c.a);
            SDL_RenderCopyF(g->renderer, v->texture, NULL, &r);
        }
        x += v->advance * scale;
    }
    return y - top + size * 1.5f;
}
void gfx_center(Gfx *g, const char *text, float x, float y, float size, SDL_Color c) {
    gfx_text(g, text, x - gfx_measure(g, text, size) * .5f, y, size, 0, c);
}
/* Center actual visible glyph bounds, including brush overhangs and descenders. */
void gfx_label(Gfx *g, const char *text, SDL_FRect r, float size, SDL_Color c) {
    float fit = gfx_measure(g, text, size);
    if (fit > r.w - 16 && fit > 0)
        size *= fmaxf(1, r.w - 16) / fit;
    float scale = size / 80, pen = 0, left = 1e9f, right = -1e9f, top = 1e9f, bottom = -1e9f;
    const char *s = text;
    while (*s) {
        Glyph *v = glyph(g, next_codepoint(&s));
        if (!v) continue;
        if (v->w && v->h) {
            left = fminf(left, pen + v->xoff * scale);
            right = fmaxf(right, pen + (v->xoff + v->w) * scale);
            top = fminf(top, v->yoff * scale);
            bottom = fmaxf(bottom, (v->yoff + v->h) * scale);
        }
        pen += v->advance * scale;
    }
    if (left > right) return;
    gfx_text(g, text, r.x + (r.w - (right - left)) / 2 - left,
             r.y + (r.h - (bottom - top)) / 2 - top - g->fonts[g->face].ascent * scale,
             size, 0, c);
}
void gfx_heading(Gfx *g, const char *text, SDL_FRect r, float size, SDL_Color c) {
    int previous = g->face;
    g->face = 1;
    gfx_label(g, text, r, size, c);
    g->face = previous;
}
void gfx_sprite(Gfx *g, Picture *p, Vec2 v, float w, float h, bool flip) {
    float scale = fminf(w / (float)p->w, h / (float)p->h);
    SDL_FRect r = {v.x - (float)p->w * scale / 2, v.y - (float)p->h * scale, (float)p->w * scale,
                   (float)p->h * scale};
    SDL_RenderCopyExF(g->renderer, p->texture, NULL, &r, 0, NULL, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
/* Preserve the entire illustration above the dialogue. A dimmed copy fills
 * the widescreen background, so even the reflected wolf stays in view. */
void gfx_story(Gfx *f, int index, float w, float h, float elapsed) {
    Picture *p = &f->prologue[index];
    float cover = fmaxf(w / p->w, h / p->h);
    SDL_FRect backdrop = {(w - p->w * cover) / 2, (h - p->h * cover) / 2, p->w * cover, p->h * cover};
    SDL_RenderCopyF(f->renderer, p->texture, NULL, &backdrop);
    gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){12, 18, 20, 213});
    float available = h - 126;
    float fit = fminf((w - 24) / p->w, available / p->h);
    SDL_FRect art = {(w - p->w * fit) / 2, 6, p->w * fit, p->h * fit};
    SDL_SetTextureAlphaMod(p->texture, (Uint8)(255 * clampf(elapsed / .5f, 0, 1)));
    SDL_RenderCopyF(f->renderer, p->texture, NULL, &art);
    SDL_SetTextureAlphaMod(p->texture, 255);
}
Vec2 world_camera(float w, float h, Vec2 p) {
    return (Vec2){w >= WORLD_W ? (WORLD_W - w) / 2 : clampf(p.x - w / 2, 0, WORLD_W - w),
                  h >= WORLD_H ? (WORLD_H - h) / 2 : clampf(p.y - h / 2, 0, WORLD_H - h)};
}
static Vec2 screen_pos(Vec2 p, Vec2 camera) {
    return (Vec2){p.x - camera.x, p.y - camera.y};
}
static void label(Gfx *f, const char *s, Vec2 p, SDL_Color c) {
    gfx_center(f, s, p.x + 1, p.y + 1, 13, (SDL_Color){5, 15, 18, 255});
    gfx_center(f, s, p.x, p.y, 13, c);
}
static void marker(Gfx *f, Vec2 p) {
    gfx_line(f, (Vec2){p.x, p.y - 6}, (Vec2){p.x + 5, p.y}, GOLD);
    gfx_line(f, (Vec2){p.x + 5, p.y}, (Vec2){p.x, p.y + 6}, GOLD);
    gfx_line(f, (Vec2){p.x, p.y + 6}, (Vec2){p.x - 5, p.y}, GOLD);
    gfx_line(f, (Vec2){p.x - 5, p.y}, (Vec2){p.x, p.y - 6}, GOLD);
}
static void shadow(Gfx *f, Vec2 p, float r) {
    for (int y = -4; y <= 4; y++) {
        float length = r * sqrtf(fmaxf(0, 1 - (float)(y * y) / 25));
        gfx_line(f, (Vec2){p.x - length, p.y + (float)y}, (Vec2){p.x + length, p.y + (float)y},
                 (SDL_Color){4, 18, 16, 110});
    }
}
/* Restore only the canopy/roof pixels over this actor. Scanline spans also
 * support concave polygons, without drawing an opaque rectangular cutout. */
static void cover_actor(Gfx *f, Vec2 feet, Vec2 camera, float width, float height) {
    float left = feet.x - width / 2, right = feet.x + width / 2;
    float top = feet.y - height, bottom = feet.y + 8;
    SDL_Rect previous_clip;
    bool had_clip = SDL_RenderIsClipEnabled(f->renderer);
    SDL_RenderGetClipRect(f->renderer, &previous_clip);
    SDL_FRect map = {-camera.x, -camera.y, WORLD_W, WORLD_H};
    for (int k = 0; k < WORLD_COVER_COUNT; k++) {
        const WorldCover *cover = &WORLD_COVERS[k];
        if (feet.y >= cover->depth)
            continue;
        const WorldShape *shape = &cover->shape;
        float minx = WORLD_W, maxx = 0, miny = WORLD_H, maxy = 0;
        for (int i = 0; i < shape->count; i++) {
            Vec2 p = shape->points[i];
            minx = fminf(minx, p.x);
            maxx = fmaxf(maxx, p.x);
            miny = fminf(miny, p.y);
            maxy = fmaxf(maxy, p.y);
        }
        if (right < minx || left > maxx || bottom < miny || top > maxy)
            continue;
        for (float y = floorf(fmaxf(top, miny)); y < fminf(bottom, maxy); y += 1) {
            float xs[SHAPE_POINTS];
            int count = 0;
            for (int i = 0, j = shape->count - 1; i < shape->count; j = i++) {
                Vec2 a = shape->points[i], b = shape->points[j];
                if ((a.y > y + .5f) != (b.y > y + .5f)) {
                    float x = a.x + (y + .5f - a.y) * (b.x - a.x) / (b.y - a.y);
                    int c = count++;
                    while (c > 0 && xs[c - 1] > x) {
                        xs[c] = xs[c - 1];
                        c--;
                    }
                    xs[c] = x;
                }
            }
            for (int i = 0; i + 1 < count; i += 2) {
                float x0 = fmaxf(left, xs[i]), x1 = fminf(right, xs[i + 1]);
                if (x1 <= x0)
                    continue;
                SDL_Rect clip = {(int)floorf(x0 - camera.x), (int)floorf(y - camera.y),
                                 (int)ceilf(x1 - camera.x) - (int)floorf(x0 - camera.x), 1};
                if (had_clip && !SDL_IntersectRect(&clip, &previous_clip, &clip))
                    continue;
                SDL_RenderSetClipRect(f->renderer, &clip);
                /* Match the base layer sampling on software renderers and HiDPI. */
                SDL_RenderCopyF(f->renderer, f->map.texture, NULL, &map);
            }
        }
    }
    SDL_RenderSetClipRect(f->renderer, had_clip ? &previous_clip : NULL);
}
void gfx_collision(Gfx *f, Vec2 camera) {
    for (int i = 0; i < WORLD_SOLID_COUNT; i++) {
        const WorldShape *s = &WORLD_SOLIDS[i];
        for (int j = 0; j < s->count; j++)
            gfx_line(f, screen_pos(s->points[j], camera), screen_pos(s->points[(j + 1) % s->count], camera),
                     (SDL_Color){255, 95, 84, 235});
    }
}
void gfx_world(Gfx *f, const Game *g, float w, float h, Vec2 camera, bool walking, bool has_target,
               Vec2 target) {
    gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){16, 32, 30, 255});
    SDL_FRect map = {-camera.x, -camera.y, WORLD_W, WORLD_H};
    SDL_RenderCopyF(f->renderer, f->map.texture, NULL, &map);
    if (g->phase == PH_TITLE || g->phase == PH_PROLOGUE) {
        gfx_sprite(f, &f->hero, screen_pos(HOME, camera), 46, 46, false);
        return;
    }
    for (int i = 0; i < TRACE_COUNT; i++)
        if (g->quest == Q_TRACES && !g->traces[i]) {
            Vec2 p = screen_pos(TRACES[i].pos, camera);
            marker(f, (Vec2){p.x, p.y - 16});
            label(f, TRACES[i].name, (Vec2){p.x, p.y - 48}, GOLD);
            gfx_circle(f, p, 12, GOLD);
        }
    for (int i = 0; i < BOAR_COUNT; i++)
        if (g->drops[i].exists && !g->drops[i].collected) {
            Vec2 p = screen_pos(g->drops[i].pos, camera);
            marker(f, (Vec2){p.x, p.y - 9});
            if (g->drops[i].attracting) {
                Vec2 tail = {g->drops[i].pos.x - g->hero.pos.x, g->drops[i].pos.y - g->hero.pos.y};
                float d = fmaxf(1, hypotf(tail.x, tail.y));
                for (int j = 1; j <= 3; j++) {
                    Vec2 dot = {p.x + tail.x / d * j * 7, p.y - 9 + tail.y / d * j * 7};
                    gfx_sector(f, dot, 2, 0, PI, (SDL_Color){227, 193, 131, (Uint8)(160 - j * 35)});
                }
            } else
                label(f, "약재", (Vec2){p.x, p.y - 36}, GOLD);
        }
    typedef struct {
        int kind, index;
        float y;
    } Actor;
    Actor actors[10];
    int count = 0, npcs = g->quest == Q_MESSENGER ? 5 : 4;
    for (int i = 0; i < npcs; i++)
        actors[count++] = (Actor){0, i, NPCS[i].pos.y};
    if (game_boars_active(g))
        for (int i = 0; i < BOAR_COUNT; i++)
            if (g->boars[i].hp > 0) {
                actors[count++] = (Actor){1, i, g->boars[i].pos.y};
                if (g->boars[i].state == AI_WINDUP)
                    gfx_sector(f, screen_pos(g->boars[i].pos, camera), 64, 0, PI,
                               (SDL_Color){200, 90, 45, 72});
            }
    if (g->quest == Q_HUNT || g->quest == Q_COMBAT) {
        actors[count++] = (Actor){2, 0, g->boss.pos.y};
        if (g->boss.state == AI_WINDUP) {
            Vec2 p = screen_pos(g->boss.pos, camera);
            gfx_sector(f, p, 109, g->boss.angle, 1.15f, (SDL_Color){205, 55, 41, 110});
            gfx_line(f, p,
                     (Vec2){p.x + cosf(g->boss.angle - 1.15f) * 109, p.y + sinf(g->boss.angle - 1.15f) * 109},
                     (SDL_Color){255, 176, 129, 255});
            gfx_line(f, p,
                     (Vec2){p.x + cosf(g->boss.angle + 1.15f) * 109, p.y + sinf(g->boss.angle + 1.15f) * 109},
                     (SDL_Color){255, 176, 129, 255});
        }
    }
    actors[count++] = (Actor){3, 0, g->hero.pos.y};
    for (int i = 1; i < count; i++) {
        Actor a = actors[i];
        int j = i - 1;
        while (j >= 0 && actors[j].y > a.y) {
            actors[j + 1] = actors[j];
            j--;
        }
        actors[j + 1] = a;
    }
    const SDL_Rect crops[4] = {
        {128, 0, 448, 724}, {606, 0, 438, 724}, {1142, 0, 368, 724}, {1674, 0, 423, 724}};
    for (int a = 0; a < count; a++) {
        int i = actors[a].index;
        Vec2 p;
        if (actors[a].kind == 0) {
            p = screen_pos(NPCS[i].pos, camera);
            shadow(f, p, 11);
            if (i == MESSENGER)
                gfx_sprite(f, &f->hero, p, 46, 46, false);
            else {
                SDL_FRect r = {p.x - 46 * (float)crops[i].w / 724 / 2, p.y - 46, 46 * (float)crops[i].w / 724,
                               46};
                SDL_RenderCopyF(f->renderer, f->npcs.texture, &crops[i], &r);
            }
            cover_actor(f, NPCS[i].pos, camera, 46, 48);
            label(f, i == ELDER ? NPCS[i].name : NPCS[i].role, (Vec2){p.x, p.y - 69}, PAPER);
            if (distance(game_objective(g), NPCS[i].pos) < 1)
                marker(f, (Vec2){p.x, p.y - 79 + sinf(g->time * 3) * 2});
        } else if (actors[a].kind == 1) {
            const Enemy *e = &g->boars[i];
            p = screen_pos(e->pos, camera);
            shadow(f, p, 17);
            gfx_sprite(f, &f->boar, (Vec2){p.x, p.y + 3}, 55, 48, e->pos.x < g->hero.pos.x);
            cover_actor(f, e->pos, camera, 55, 50);
            if (e->hp < e->max_hp) {
                gfx_rect(f, (SDL_FRect){p.x - 20, p.y - 55, 40, 4}, INK);
                gfx_rect(f, (SDL_FRect){p.x - 20, p.y - 55, 40 * (float)e->hp / (float)e->max_hp, 4}, RED);
            } else
                label(f, "변이한 멧돼지", (Vec2){p.x, p.y - 65}, (SDL_Color){219, 186, 162, 255});
        } else if (actors[a].kind == 2) {
            p = screen_pos(g->boss.pos, camera);
            shadow(f, p, 23);
            gfx_sprite(f, &f->boss, (Vec2){p.x, p.y + 7}, 83, 96, g->boss.pos.x < g->hero.pos.x);
            cover_actor(f, g->boss.pos, camera, 83, 98);
            if (g->boss.state == AI_RECOVER)
                label(f, "빈틈", (Vec2){p.x, p.y - 98}, GOLD);
            else if (g->quest == Q_HUNT)
                label(f, "변이한 백정", (Vec2){p.x, p.y - 98}, PAPER);
        } else {
            p = screen_pos(g->hero.pos, camera);
            shadow(f, p, 10);
            SDL_SetTextureAlphaMod(f->hero.texture,
                                   g->hero.invul > 0 && ((int)(g->time * 18) % 2) ? 110 : 255);
            gfx_sprite(f, &f->hero, (Vec2){p.x, p.y + (walking ? sinf(g->time * 14) * 1.5f : 0)}, 46, 46,
                       g->hero.face < 0);
            SDL_SetTextureAlphaMod(f->hero.texture, 255);
            /* A faint silhouette keeps the player readable behind tall scenery. */
            SDL_SetTextureAlphaMod(f->map.texture, 205);
            cover_actor(f, g->hero.pos, camera, 46, 48);
            SDL_SetTextureAlphaMod(f->map.texture, 255);
        }
    }
    if (has_target)
        gfx_circle(f, screen_pos(target, camera), 5, GOLD);
    for (int i = 0; i < g->effect_count; i++) {
        const Effect *e = &g->effects[i];
        Vec2 p = screen_pos(e->pos, camera), q = screen_pos(e->target, camera);
        SDL_Color c = e->damage ? (SDL_Color){255, 184, 165, 255} : GOLD;
        if (e->kind == FX_NUMBER) {
            char s[24];
            snprintf(s, sizeof s, "%s%d", e->damage ? "-" : "", e->amount);
            gfx_center(f, s, p.x, p.y - (1 - e->life) * 22, 16, c);
        } else if (e->kind == FX_SKILL)
            gfx_circle(f, (Vec2){p.x, p.y - 13}, 45, c);
        else if (e->kind == FX_WIND)
            gfx_line(f, (Vec2){p.x, p.y - 17}, (Vec2){q.x, q.y - 22}, (SDL_Color){204, 238, 203, 255});
        else if (e->kind == FX_BOSS_SWING)
            gfx_sector(f, p, 65, e->angle, 1.1f, (SDL_Color){244, 178, 154, 100});
        else
            gfx_line(f, (Vec2){q.x - 16, q.y - 35}, (Vec2){q.x + 17, q.y - 12}, PAPER);
    }
    Vec2 objective = game_objective(g);
    if (g->phase == PH_PLAY && g->quest != Q_COMBAT && distance(objective, g->hero.pos) > 70) {
        float angle = atan2f(objective.y - g->hero.pos.y, objective.x - g->hero.pos.x);
        Vec2 p = screen_pos(g->hero.pos, camera);
        p.x += cosf(angle) * 42;
        p.y += sinf(angle) * 42 - 17;
        gfx_sector(f, p, 9, angle, .55f, GOLD);
    }
}
