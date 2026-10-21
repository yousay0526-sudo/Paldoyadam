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
/* Load authored pixels directly; the village repaint needs no sharpening pass. */
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
    Picture *pictures[] = {&g->map, &g->hero, &g->hero_walk, &g->hero_attack, &g->messenger, &g->boss, &g->boar,
                           &g->npcs, &g->prologue[0], &g->prologue[1], &g->prologue[2],
                           &g->hero_techniques[TECH_CLEAVE], &g->hero_techniques[TECH_SWEEP],
                           &g->hero_techniques[TECH_THRUST], &g->hero_techniques[TECH_RETREAT],
                           &g->hero_idle_directions,
                           &g->hero_walk_directions[HERO_N], &g->hero_walk_directions[HERO_NE],
                           &g->hero_walk_directions[HERO_E], &g->hero_walk_directions[HERO_SE],
                           &g->hero_walk_directions[HERO_S], &g->hero_walk_directions[HERO_SW],
                           &g->hero_walk_directions[HERO_W], &g->hero_walk_directions[HERO_NW]};
    const char *names[] = {"village.png",
                           "warrior-v2.png",
                           "warrior-walk-v1.png",
                           "warrior-attack-v1.png",
                           "warrior.png",
                           "butcher.png",
                           "minion.png",
                           "npcs.png",
                           "prologue/01-homeward.png",
                           "prologue/02-hunger.png",
                           "prologue/03-reflection.png",
                           "warrior-cleave-v1.png",
                           "warrior-sweep-v1.png",
                           "warrior-thrust-v1.png",
                           "warrior-retreat-v1.png",
                           "warrior-idle-8dir-v1.png",
                           "warrior-walk-n-v1.png", "warrior-walk-ne-v1.png",
                           "warrior-walk-e-v1.png", "warrior-walk-se-v1.png",
                           "warrior-walk-s-v1.png", "warrior-walk-sw-v1.png",
                           "warrior-walk-w-v1.png", "warrior-walk-nw-v1.png"};
    snprintf(g->root, sizeof g->root, "%s", root);
    for (size_t i = 0; i < sizeof pictures / sizeof *pictures; i++) {
        asset_path(path, sizeof path, root, names[i]);
        /* Story illustrations are decoded only while the prologue shows them
         * (gfx_story); here they must merely exist, as before. */
        if (i >= 8 && i < 11) {
            SDL_RWops *rw = SDL_RWFromFile(path, "rb");
            if (!rw) {
                SDL_Log("Cannot load %s: %s", path, SDL_GetError());
                gfx_free(g);
                return false;
            }
            SDL_RWclose(rw);
            continue;
        }
        *pictures[i] = load_picture(g, path);
        if (!pictures[i]->texture) {
            SDL_Log("Cannot load %s: %s", path, SDL_GetError());
            gfx_free(g);
            return false;
        }
        /* World sprites keep crisp sampling. The hero image is prefiltered
         * at 3x its logical size; only the story illustrations use linear.
         * Enemy and NPC sprites are prepared near 3x their drawn size
         * (scripts/prepare_character_assets.py) and filter smoothly. */
        if (i == 0 || i >= 11)
            SDL_SetTextureScaleMode(pictures[i]->texture, SDL_ScaleModeNearest);
        else if (i >= 4 && i < 8)
            SDL_SetTextureScaleMode(pictures[i]->texture, SDL_ScaleModeLinear);
    }
    if (g->hero_walk.w % HERO_WALK_COLUMNS ||
        g->hero_walk.h % (HERO_WALK_FRAMES / HERO_WALK_COLUMNS) ||
        g->hero_walk.w / HERO_WALK_COLUMNS !=
        g->hero_walk.h / (HERO_WALK_FRAMES / HERO_WALK_COLUMNS)) {
        SDL_Log("Hero walking atlas must contain eight square cells in a 4x2 grid");
        gfx_free(g);
        return false;
    }
    if (g->hero_attack.w % HERO_ATTACK_COLUMNS ||
        g->hero_attack.h % (HERO_ATTACK_FRAMES / HERO_ATTACK_COLUMNS) ||
        g->hero_attack.w / HERO_ATTACK_COLUMNS !=
        g->hero_attack.h / (HERO_ATTACK_FRAMES / HERO_ATTACK_COLUMNS)) {
        SDL_Log("Hero attack atlas must contain six square cells in a 3x2 grid");
        gfx_free(g);
        return false;
    }
    for (int i = 0; i < TECH_COUNT; i++) {
        const Picture *p = &g->hero_techniques[i];
        if (p->w != 576 || p->h != 384) {
            SDL_Log("Hero technique atlas %d must contain six 192px cells in a 3x2 grid", i);
            gfx_free(g);
            return false;
        }
    }
    for (int i = -1; i < HERO_DIRECTIONS; i++) {
        const Picture *p = i < 0 ? &g->hero_idle_directions : &g->hero_walk_directions[i];
        if (p->w != 768 || p->h != 384) {
            SDL_Log("Hero direction atlas %d must contain eight 192px cells in a 4x2 grid", i);
            gfx_free(g);
            return false;
        }
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
    Picture *pictures[] = {&g->map, &g->hero, &g->hero_walk, &g->hero_attack, &g->messenger, &g->boss, &g->boar,
                           &g->npcs, &g->prologue[0], &g->prologue[1], &g->prologue[2],
                           &g->hero_techniques[TECH_CLEAVE], &g->hero_techniques[TECH_SWEEP],
                           &g->hero_techniques[TECH_THRUST], &g->hero_techniques[TECH_RETREAT],
                           &g->hero_idle_directions,
                           &g->hero_walk_directions[HERO_N], &g->hero_walk_directions[HERO_NE],
                           &g->hero_walk_directions[HERO_E], &g->hero_walk_directions[HERO_SE],
                           &g->hero_walk_directions[HERO_S], &g->hero_walk_directions[HERO_SW],
                           &g->hero_walk_directions[HERO_W], &g->hero_walk_directions[HERO_NW]};
    for (size_t i = 0; i < sizeof pictures / sizeof *pictures; i++) {
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
/* One centered circle, shared by fill and outline. More segments than the
 * generic in-world effect circles prevent a visibly faceted attack button. */
void gfx_disc(Gfx *g, Vec2 p, float radius, SDL_Color fill, SDL_Color rim) {
    enum { SEGMENTS = 128 };
    SDL_Vertex vertices[SEGMENTS + 2];
    SDL_FPoint outline[SEGMENTS + 1];
    int indices[SEGMENTS * 3];
    vertices[0] = (SDL_Vertex){{p.x, p.y}, fill, {0, 0}};
    for (int i = 0; i <= SEGMENTS; i++) {
        float angle = 2.0f * PI * (float)i / SEGMENTS;
        SDL_FPoint point = {p.x + radius * cosf(angle), p.y + radius * sinf(angle)};
        vertices[i + 1] = (SDL_Vertex){point, fill, {0, 0}};
        outline[i] = point;
        if (i < SEGMENTS) {
            indices[i * 3] = 0;
            indices[i * 3 + 1] = i + 1;
            indices[i * 3 + 2] = i + 2;
        }
    }
    SDL_RenderGeometry(g->renderer, NULL, vertices, SEGMENTS + 2, indices, SEGMENTS * 3);
    color(g, rim);
    SDL_RenderDrawLinesF(g->renderer, outline, SEGMENTS + 1);
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
            y += size * 1.32f;
            continue;
        }
        Glyph *v = glyph(g, cp);
        if (!v)
            continue;
        if (max_width > 0 && x > start && x + v->advance * scale > start + max_width) {
            x = start;
            y += size * 1.32f;
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
    return y - top + size * 1.32f;
}
/* Measure exactly the same wrapping as gfx_text before allocating a panel. */
float gfx_text_height(Gfx *g, const char *text, float size, float max_width) {
    float x = 0, height = size * 1.32f;
    while (*text) {
        int cp = next_codepoint(&text);
        if (cp == '\n') { x = 0; height += size * 1.32f; continue; }
        Glyph *v = glyph(g, cp);
        if (!v) continue;
        float advance = v->advance * size / 80;
        if (max_width > 0 && x > 0 && x + advance > max_width) {
            x = 0;
            height += size * 1.32f;
            if (cp == ' ') continue;
        }
        x += advance;
    }
    return height;
}
static bool text_clip(Gfx *g, SDL_FRect r, SDL_Rect *previous) {
    bool enabled = SDL_RenderIsClipEnabled(g->renderer);
    SDL_RenderGetClipRect(g->renderer, previous);
    SDL_Rect clip = {(int)ceilf(r.x), (int)ceilf(r.y),
        (int)fmaxf(0, floorf(r.x + r.w) - ceilf(r.x)),
        (int)fmaxf(0, floorf(r.y + r.h) - ceilf(r.y))};
    if (enabled) SDL_IntersectRect(previous, &clip, &clip);
    SDL_RenderSetClipRect(g->renderer, &clip);
    return enabled;
}
float gfx_text_box(Gfx *g, const char *text, SDL_FRect r, float size, float min_size, SDL_Color c) {
    float width = fmaxf(1, r.w - 2);
    while (size > min_size && gfx_text_height(g, text, size, width) > r.h)
        size = fmaxf(min_size, size - .5f);
    SDL_Rect previous;
    bool enabled = text_clip(g, r, &previous);
    gfx_text(g, text, r.x + 1, r.y, size, width, c);
    SDL_RenderSetClipRect(g->renderer, enabled ? &previous : NULL);
    return size;
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
    float fit_height = fminf(1, fmaxf(1, r.h - 6) / fmaxf(1, bottom - top));
    size *= fit_height;
    scale *= fit_height;
    left *= fit_height; right *= fit_height; top *= fit_height; bottom *= fit_height;
    SDL_Rect previous;
    bool enabled = text_clip(g, r, &previous);
    gfx_text(g, text, r.x + (r.w - (right - left)) / 2 - left,
             r.y + (r.h - (bottom - top)) / 2 - top - g->fonts[g->face].ascent * scale,
             size, 0, c);
    SDL_RenderSetClipRect(g->renderer, enabled ? &previous : NULL);
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
/* Add light only through authored sprite alpha, then restore all texture state.
 * This is a contact flash, with no change to the actor's world position. */
static void hit_flash(Gfx *g, Picture *p, const SDL_Rect *source, const SDL_FRect *dest,
                      double angle, const SDL_FPoint *pivot, SDL_RendererFlip flip, float remaining) {
    if (remaining <= 0) return;
    Uint8 r, green, b, alpha;
    SDL_BlendMode blend;
    SDL_GetTextureColorMod(p->texture, &r, &green, &b);
    SDL_GetTextureAlphaMod(p->texture, &alpha);
    SDL_GetTextureBlendMode(p->texture, &blend);
    SDL_SetTextureColorMod(p->texture, 255, 228, 180);
    SDL_SetTextureAlphaMod(p->texture, (Uint8)(175 * clampf(remaining / HIT_FLASH_SECONDS, 0, 1)));
    SDL_SetTextureBlendMode(p->texture, SDL_BLENDMODE_ADD);
    SDL_RenderCopyExF(g->renderer, p->texture, source, dest, angle, pivot, flip);
    SDL_SetTextureBlendMode(p->texture, blend);
    SDL_SetTextureColorMod(p->texture, r, green, b);
    SDL_SetTextureAlphaMod(p->texture, alpha);
}
void gfx_hit_sprite(Gfx *g, Picture *p, Vec2 v, float w, float h, bool flip, float remaining) {
    gfx_sprite(g, p, v, w, h, flip);
    float scale = fminf(w / (float)p->w, h / (float)p->h);
    SDL_FRect dest = {v.x - p->w * scale / 2, v.y - p->h * scale, p->w * scale, p->h * scale};
    hit_flash(g, p, NULL, &dest, 0, NULL, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE, remaining);
}
/* Procedural motion for single-image enemies. Everything here moves only
 * the picture around the feet; positions and hit tests are untouched. */
typedef struct {
    float windup, swing, lunge;
} EnemyTiming;
static void gfx_enemy(Gfx *f, Picture *p, Vec2 feet, float w, float h, bool flip, const Enemy *e,
                      EnemyTiming timing, float time, bool live) {
    float scale = fminf(w / (float)p->w, h / (float)p->h);
    float width = p->w * scale, height = p->h * scale, sx = 1, sy = 1, dx = 0, dy = 0, tilt = 0;
    Uint8 alpha = 255;
    Vec2 aim = {cosf(e->angle), sinf(e->angle)};
    if (e->hp <= 0) {
        /* Topple away from the hero, sink a little and fade out. */
        float t = 1 - clampf(e->dying / ENEMY_DEATH_SECONDS, 0, 1), ease = 1 - (1 - t) * (1 - t);
        tilt = (flip ? -1 : 1) * 78 * ease;
        dy += 4 * t;
        alpha = (Uint8)(255 * clampf(1 - t * t, 0, 1));
    } else if (e->state == AI_WINDUP) {
        /* Crouch and draw back from the target, trembling just before. */
        float t = 1 - clampf(e->timer / timing.windup, 0, 1);
        dx -= aim.x * 4 * t;
        dy -= aim.y * 2 * t;
        sx += .05f * t;
        sy -= .06f * t;
        if (t > .5f) dx += sinf(time * 70) * (t - .5f) * 2;
    } else if (e->state == AI_SWING || (e->state == AI_RECOVER && timing.swing <= 0 &&
                                        e->timer > timing.lunge - .15f)) {
        /* Snap forward on the strike, then settle back. */
        float t = e->state == AI_SWING ? clampf(e->timer / timing.swing, 0, 1)
                                       : clampf((e->timer - (timing.lunge - .15f)) / .15f, 0, 1);
        dx += aim.x * 9 * t;
        dy += aim.y * 5 * t;
        sx -= .04f * t;
        sy += .06f * t;
    } else {
        /* Bob and squash with each step actually walked. */
        float bob = fabsf(sinf(e->stride * PI / 26));
        dy -= bob * 2.5f;
        sx -= .02f * bob;
        sy += .03f * bob;
    }
    if (live && e->hit_time > 0) {
        float k = clampf(e->hit_time / HIT_FLASH_SECONDS, 0, 1);
        sx += .08f * k;
        sy -= .08f * k;
    }
    SDL_FRect dest = {feet.x - width * sx / 2 + dx, feet.y - height * sy + dy, width * sx, height * sy};
    SDL_FPoint pivot = {dest.w / 2, dest.h};
    SDL_RendererFlip mirror = flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_SetTextureAlphaMod(p->texture, alpha);
    SDL_RenderCopyExF(f->renderer, p->texture, NULL, &dest, tilt, &pivot, mirror);
    SDL_SetTextureAlphaMod(p->texture, 255);
    if (live && e->hp > 0)
        hit_flash(f, p, NULL, &dest, tilt, &pivot, mirror, e->hit_time);
}
/* The directional sword trail uses exactly
 * the simulation heading, sector and collision-resolved impact positions. */
/* A crescent between two radii. Colour fades along the arc toward the tail
 * and inward toward the hand, so the blade edge reads brightest. */
static void slash_band(Gfx *f, Vec2 c, float inner, float outer, float from, float to,
                       SDL_Color edge, float alpha) {
    enum { STEPS = 24 };
    SDL_Vertex vertices[(STEPS + 1) * 2];
    int indices[STEPS * 6];
    for (int i = 0; i <= STEPS; i++) {
        float t = (float)i / STEPS, a = from + (to - from) * t;
        SDL_Color lit = edge;
        lit.a = (Uint8)(edge.a * alpha * t * t);
        SDL_Color clear = lit;
        clear.a = 0;
        vertices[i * 2] = (SDL_Vertex){{c.x + cosf(a) * outer, c.y + sinf(a) * outer}, lit, {0, 0}};
        vertices[i * 2 + 1] = (SDL_Vertex){{c.x + cosf(a) * inner, c.y + sinf(a) * inner}, clear, {0, 0}};
        if (i < STEPS) {
            int *k = indices + i * 6, o = i * 2;
            k[0] = o; k[1] = o + 1; k[2] = o + 2;
            k[3] = o + 1; k[4] = o + 3; k[5] = o + 2;
        }
    }
    SDL_RenderGeometry(f->renderer, NULL, vertices, (STEPS + 1) * 2, indices, STEPS * 6);
}
/* A narrow streak for the thrust: bright at the point, clear at the hand. */
static void slash_streak(Gfx *f, Vec2 c, float inner, float outer, float angle, float width,
                         SDL_Color edge, float alpha) {
    float x = cosf(angle), y = sinf(angle);
    SDL_Color lit = edge, clear = edge;
    lit.a = (Uint8)(edge.a * alpha);
    clear.a = 0;
    SDL_Vertex v[3] = {
        {{c.x + x * outer, c.y + y * outer}, lit, {0, 0}},
        {{c.x + x * inner - y * width, c.y + y * inner + x * width}, clear, {0, 0}},
        {{c.x + x * inner + y * width, c.y + y * inner - x * width}, clear, {0, 0}}};
    SDL_RenderGeometry(f->renderer, NULL, v, 3, NULL, 0);
}
/* The slash appears on the impact frame and stays inside the real hit sector
 * (same origin, angle, half angle and range as the simulation). It sweeps
 * across the sector, then the tail wipes toward the blade tip as it fades.
 * Nothing is drawn while winding up, so no hitbox outline is ever shown. */
void gfx_attack_trace(Gfx *f, const Game *g, Vec2 feet) {
    if (g->phase != PH_PLAY || (!g->hero.attacking && !g->hero.skilling)) return;
    const Hero *h = &g->hero;
    bool struck = h->skilling ? h->skill_hit : h->attack_hit;
    if (!struck) return;
    int move = h->skilling ? (int)h->technique : MOVE_ATTACK;
    float elapsed = h->skilling ? h->skill_elapsed : h->attack_elapsed;
    float since = elapsed - (h->skilling ? HERO_SKILL_IMPACT : HERO_ATTACK_IMPACT);
    float angle = h->skilling ? h->skill_angle : h->attack_angle;
    float fade = 1 - clampf((since - .04f) / .18f, 0, 1);
    if (fade <= 0) return;
    float reveal = 1 - clampf(.35f + since / .07f, 0, 1);
    float sweep = 1 - reveal * reveal * reveal, wipe = (1 - fade) * .8f;
    float radius = game_attack_range(g, move), half = game_attack_half_angle(move);
    Vec2 origin = h->impact_origin, start = h->impact_start;
    Vec2 center = {feet.x + origin.x - h->pos.x, feet.y + origin.y - h->pos.y - 22};
    SDL_BlendMode previous;
    SDL_GetRenderDrawBlendMode(f->renderer, &previous);
    SDL_SetRenderDrawBlendMode(f->renderer, SDL_BLENDMODE_ADD);
    const SDL_Color glow = {120, 170, 205, 190}, edge = {235, 248, 255, 255};
    if (distance(origin, start) > .01f) {
        /* Forward moves leave a short dash streak from where the step began. */
        Vec2 from = {feet.x + start.x - h->pos.x, feet.y + start.y - h->pos.y - 22};
        float dash = distance(from, center), along = atan2f(center.y - from.y, center.x - from.x);
        if (dash > 1)
            slash_streak(f, from, 0, dash, along, 5, glow, .55f * fade);
    }
    if (move == TECH_THRUST) {
        float tip = radius * (.3f + .7f * sweep), tail = radius * (.15f + .6f * wipe);
        slash_streak(f, center, tail, tip, angle, radius * .09f, glow, fade);
        slash_streak(f, center, tail, tip, angle, radius * .03f, edge, fade);
    } else {
        /* Swing from the back hand toward the facing side; mirrored swings
         * reverse the direction so the arc follows the blade. */
        float direction = h->attack_face > 0 ? 1.0f : -1.0f;
        float first = angle - direction * half, span = 2 * half * direction;
        float to = first + span * sweep, from = first + span * fminf(wipe, sweep);
        slash_band(f, center, radius * .68f, radius, from, to, glow, .6f * fade);
        slash_band(f, center, radius * .9f, radius, from, to, edge, fade);
    }
    SDL_SetRenderDrawBlendMode(f->renderer, previous);
}
HeroDirection gfx_hero_direction(Vec2 direction) {
    if (!isfinite(direction.x) || !isfinite(direction.y) ||
        (fabsf(direction.x) < .00001f && fabsf(direction.y) < .00001f))
        return HERO_S;
    float angle = atan2f(direction.x, -direction.y);
    int sector = (int)floorf(angle / .7853981633974483f + .5f);
    return (HeroDirection)((sector + HERO_DIRECTIONS) % HERO_DIRECTIONS);
}
void gfx_hero(Gfx *f, const Game *g, Vec2 feet) {
    int frame = game_hero_frame(g);
    int skill = game_hero_skill_frame(g);
    int attack = skill >= 0 ? skill : game_hero_attack_frame(g);
    HeroDirection direction = gfx_hero_direction(g->hero.dir);
    Picture *p = attack >= 0 ? &f->hero_attack : frame < 0 ?
        &f->hero_idle_directions : &f->hero_walk_directions[direction];
    if (skill >= 0 && g->hero.technique >= 0 && g->hero.technique < TECH_COUNT)
        p = &f->hero_techniques[g->hero.technique];
    SDL_Rect source = {0, 0, p->w, p->h};
    {
        int columns = attack >= 0 ? HERO_ATTACK_COLUMNS : HERO_WALK_COLUMNS;
        int frames = attack >= 0 ? HERO_ATTACK_FRAMES : HERO_WALK_FRAMES;
        int cell = attack >= 0 ? attack : frame >= 0 ? frame : (int)direction;
        source.w = p->w / columns;
        source.h = p->h / (frames / columns);
        source.x = cell % columns * source.w;
        source.y = cell / columns * source.h;
    }
    /* Idle/walk never reflect: the sword stays in the anatomical right hand.
     * Hero.dir already retains the last movement input, including while idle. */
    bool flip = attack >= 0 && g->hero.attack_face > 0;
    SDL_FRect dest = {feet.x - 28, feet.y - 176.0f / 192 * 56, 56, 56};
    if (skill >= 0) {
        /* All prepared techniques share this foot root and scale. The source
         * poses already contain their lean; rotating them would move the feet. */
        float root_x = 116.0f / 192;
        if (flip) root_x = 1 - root_x;
        dest = (SDL_FRect){feet.x - root_x * 80, feet.y - 176.0f / 192 * 80, 80, 80};
    } else if (attack >= 0) {
        /* Atlas-space roots keep the supporting foot planted through the swing.
         * These anchors are also the alignment contract for future equipment layers. */
        const Vec2 roots[HERO_ATTACK_FRAMES] = {
            {290, 470}, {264, 470}, {310, 470}, {340, 455}, {280, 464}, {300, 465}};
        const float fit[HERO_ATTACK_FRAMES] = {1, 1, 1, 1, .95f, .88f};
        float size = 64 * fit[attack];
        float root_x = roots[attack].x / 512;
        if (flip) root_x = 1 - root_x;
        dest = (SDL_FRect){feet.x - root_x * size,
                           feet.y - roots[attack].y / 512 * size, size, size};
    }
    SDL_SetTextureAlphaMod(p->texture,
        g->hero.hurt_time <= 0 && g->hero.invul > 0 && ((int)(g->time * 18) % 2) ? 110 : 255);
    SDL_RenderCopyExF(f->renderer, p->texture, &source, &dest, 0,
                     NULL, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(p->texture, 255);
    if (g->phase == PH_PLAY)
        hit_flash(f, p, &source, &dest, 0, NULL,
                  flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE, g->hero.hurt_time);
    gfx_attack_trace(f, g, feet);
}
void gfx_story_release(Gfx *f) {
    for (int i = 0; i < 3; i++) {
        if (f->prologue[i].texture)
            SDL_DestroyTexture(f->prologue[i].texture);
        f->prologue[i] = (Picture){0};
    }
}
/* Preserve the entire illustration above the dialogue. A dimmed copy fills
 * the widescreen background, so even the reflected wolf stays in view. */
void gfx_story(Gfx *f, int index, float w, float h, float elapsed, float footer_height) {
    static const char *names[3] = {"prologue/01-homeward.png", "prologue/02-hunger.png",
                                   "prologue/03-reflection.png"};
    Picture *p = &f->prologue[index];
    if (!p->texture) {
        /* Hold one illustration at a time: each is 9 MiB once decoded. */
        gfx_story_release(f);
        char path[2048];
        asset_path(path, sizeof path, f->root, names[index]);
        *p = load_picture(f, path);
        if (!p->texture) {
            SDL_Log("Cannot load %s: %s", path, SDL_GetError());
            *p = (Picture){0};
            return;
        }
        SDL_SetTextureScaleMode(p->texture, SDL_ScaleModeLinear);
    }
    float cover = fmaxf(w / p->w, h / p->h);
    SDL_FRect backdrop = {(w - p->w * cover) / 2, (h - p->h * cover) / 2, p->w * cover, p->h * cover};
    SDL_RenderCopyF(f->renderer, p->texture, NULL, &backdrop);
    gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){12, 18, 20, 213});
    float available = fmaxf(20, h - footer_height - 24);
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
Vec2 world_from_screen(Vec2 p, Vec2 camera) {
    return (Vec2){p.x / WORLD_ZOOM + camera.x, p.y / WORLD_ZOOM + camera.y};
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
/* A soft contact shadow: dark at the feet, fading to nothing at the rim. */
static void shadow(Gfx *f, Vec2 p, float r, float strength) {
    enum { SIDES = 24 };
    SDL_Vertex v[SIDES + 1];
    int k[SIDES * 3];
    v[0] = (SDL_Vertex){{p.x, p.y}, {4, 18, 16, (Uint8)(150 * strength)}, {0, 0}};
    for (int i = 0; i < SIDES; i++) {
        float a = 2 * PI * i / SIDES;
        v[i + 1] = (SDL_Vertex){{p.x + cosf(a) * r * 1.15f, p.y + sinf(a) * 5.5f}, {4, 18, 16, 0}, {0, 0}};
        k[i * 3] = 0; k[i * 3 + 1] = i + 1; k[i * 3 + 2] = (i + 1) % SIDES + 1;
    }
    SDL_RenderGeometry(f->renderer, NULL, v, SIDES + 1, k, SIDES * 3);
}
/* Stateless particles: every puff and chip is a pure function of the
 * effect's age and a hash of where it spawned, so nothing else is stored. */
static float scatter(Vec2 p, int i) {
    return fmodf(fabsf(sinf(p.x * 12.9898f + p.y * 78.233f + i * 37.719f) * 43758.547f), 1);
}
static void dust(Gfx *f, const Effect *e, Vec2 p) {
    const float total[] = {.35f, .35f, .45f, .55f};
    int kind = e->amount < 1 ? 1 : e->amount > 3 ? 3 : e->amount, count = kind == 1 ? 3 : kind == 2 ? 7 : 10;
    float t = clampf(1 - e->life / total[kind], 0, 1), spread = 1 - (1 - t) * (1 - t);
    for (int i = 0; i < count; i++) {
        float a = kind == 3 ? 2 * PI * (i + scatter(e->pos, i)) / count
                            : e->angle + PI + (scatter(e->pos, i) - .5f) * (kind == 1 ? 1.6f : 1.2f);
        float reach = (kind == 1 ? 10 : kind == 2 ? 20 : 26) * (.6f + .4f * scatter(e->pos, i + 9));
        Vec2 c = {p.x + cosf(a) * reach * spread, p.y + sinf(a) * reach * spread * .45f - 2 - 5 * t};
        float radius = (kind == 1 ? 3.5f : 4.5f) + (kind == 1 ? 4 : 6) * spread;
        gfx_sector(f, c, radius, 0, PI, (SDL_Color){206, 184, 140, (Uint8)(130 * (1 - t))});
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
/* An attack warning over the exact reach. The dim area shows where it lands;
 * the brighter fill grows from the attacker and touches the rim on the
 * impact frame, and the rim flickers during the final moment. */
static void telegraph(Gfx *f, Vec2 p, float range, float angle, float half, float remaining,
                      SDL_Color area, SDL_Color rim) {
    float progress = 1 - clampf(remaining, 0, 1);
    SDL_Color dim = area, fill = area;
    dim.a = (Uint8)(area.a * .6f);
    fill.a = (Uint8)fminf(255, area.a * 1.5f);
    gfx_sector(f, p, range, angle, half, dim);
    if (progress > 0)
        gfx_sector(f, p, range * progress, angle, half, fill);
    bool flicker = remaining < .25f && (int)(remaining * 12) % 2 == 0;
    rim.a = flicker ? 255 : 150;
    if (half < PI)
        for (int side = -1; side <= 1; side += 2)
            gfx_line(f, p, (Vec2){p.x + cosf(angle + side * half) * range,
                                  p.y + sinf(angle + side * half) * range}, rim);
    for (int j = 0; j < 40; j++) {
        float a0 = angle - half + 2 * half * j / 40, a1 = angle - half + 2 * half * (j + 1) / 40;
        gfx_line(f, (Vec2){p.x + cosf(a0) * range, p.y + sinf(a0) * range},
                    (Vec2){p.x + cosf(a1) * range, p.y + sinf(a1) * range}, rim);
    }
}
void gfx_world(Gfx *f, const Game *g, float w, float h, Vec2 camera, bool has_target,
               Vec2 target) {
    gfx_rect(f, (SDL_FRect){0, 0, w, h}, (SDL_Color){16, 32, 30, 255});
    SDL_FRect map = {-camera.x, -camera.y, WORLD_W, WORLD_H};
    SDL_RenderCopyF(f->renderer, f->map.texture, NULL, &map);
    if (g->phase == PH_TITLE || g->phase == PH_PROLOGUE) {
        gfx_hero(f, g, screen_pos(HOME, camera));
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
            float bounce = g->drops[i].age < .32f ? sinf(g->drops[i].age / .32f * PI) * 18 : 0;
            shadow(f, p, 8, 1);
            Vec2 item = {p.x, p.y - 8 - bounce};
            gfx_sector(f, item, 6, 0, PI, (SDL_Color){189, 155, 94, 255});
            gfx_round_border(f, (SDL_FRect){item.x - 6, item.y - 6, 12, 12}, 4, GOLD);
            gfx_line(f, (Vec2){item.x - 4, item.y - 2}, (Vec2){item.x + 4, item.y + 2}, INK);
            gfx_line(f, (Vec2){item.x, item.y - 6}, (Vec2){item.x, item.y + 5}, INK);
            if (g->drops[i].attracting) {
                Vec2 tail = {g->drops[i].pos.x - g->hero.pos.x, g->drops[i].pos.y - g->hero.pos.y};
                float d = fmaxf(1, hypotf(tail.x, tail.y));
                for (int j = 1; j <= 3; j++) {
                    Vec2 dot = {p.x + tail.x / d * j * 7, item.y + tail.y / d * j * 7};
                    gfx_sector(f, dot, 2, 0, PI, (SDL_Color){227, 193, 131, (Uint8)(160 - j * 35)});
                }
            } else
                label(f, "약재", (Vec2){p.x, item.y - 24}, GOLD);
        }
    typedef struct {
        int kind, index;
        float y;
    } Actor;
    Actor actors[10];
    int count = 0, npcs = g->training.active ? 0 : g->quest == Q_MESSENGER ? 5 : 4;
    for (int i = 0; i < npcs; i++)
        actors[count++] = (Actor){0, i, NPCS[i].pos.y};
    if (game_boars_active(g))
        for (int i = 0; i < BOAR_COUNT; i++)
            if (g->boars[i].hp > 0 || g->boars[i].dying > 0) {
                actors[count++] = (Actor){1, i, g->boars[i].pos.y};
                if (g->boars[i].state == AI_WINDUP)
                    telegraph(f, screen_pos(g->boars[i].pos, camera), BOAR_STRIKE_RANGE, 0, PI,
                              g->boars[i].timer / BOAR_WINDUP_SECONDS, (SDL_Color){200, 90, 45, 72},
                              (SDL_Color){255, 170, 120, 255});
            }
    if (g->training.active || g->quest == Q_HUNT || g->quest == Q_COMBAT || g->boss.dying > 0) {
        actors[count++] = (Actor){2, 0, g->boss.pos.y};
        if (g->boss.state == AI_WINDUP)
            telegraph(f, screen_pos(g->boss.pos, camera), BOSS_STRIKE_RANGE, g->boss.angle,
                      BOSS_STRIKE_HALF, g->boss.timer / BOSS_WINDUP_SECONDS,
                      (SDL_Color){205, 55, 41, 110}, (SDL_Color){255, 176, 129, 255});
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
    for (int i = 0; i < g->effect_count; i++)
        if (g->effects[i].kind == FX_DUST)
            dust(f, &g->effects[i], screen_pos(g->effects[i].pos, camera));
    /* NPC cells in the 2172 x 724 master strip, scaled to the runtime copy. */
    const SDL_Rect cells[4] = {
        {128, 0, 448, 724}, {606, 0, 438, 724}, {1142, 0, 368, 724}, {1674, 0, 423, 724}};
    SDL_Rect crops[4];
    for (int i = 0; i < 4; i++) {
        float k = (float)f->npcs.h / 724;
        crops[i] = (SDL_Rect){(int)lroundf(cells[i].x * k), 0, (int)lroundf(cells[i].w * k), f->npcs.h};
    }
    for (int a = 0; a < count; a++) {
        int i = actors[a].index;
        Vec2 p;
        if (actors[a].kind == 0) {
            p = screen_pos(NPCS[i].pos, camera);
            shadow(f, p, 11, 1);
            if (i == MESSENGER)
                gfx_sprite(f, &f->messenger, p, 46, 46, false);
            else {
                SDL_FRect r = {p.x - 46 * (float)cells[i].w / 724 / 2, p.y - 46, 46 * (float)cells[i].w / 724,
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
            shadow(f, p, 17, e->hp > 0 ? 1 : clampf(e->dying / ENEMY_DEATH_SECONDS, 0, 1));
            gfx_enemy(f, &f->boar, (Vec2){p.x, p.y + 3}, 55, 48, e->pos.x < g->hero.pos.x, e,
                      (EnemyTiming){BOAR_WINDUP_SECONDS, 0, BOAR_RECOVER_SECONDS}, g->time,
                      g->phase == PH_PLAY);
            cover_actor(f, e->pos, camera, 55, 50);
            if (e->hp <= 0) continue;
            if (e->hp < e->max_hp) {
                gfx_rect(f, (SDL_FRect){p.x - 20, p.y - 55, 40, 4}, INK);
                gfx_rect(f, (SDL_FRect){p.x - 20, p.y - 55, 40 * (float)e->hp / (float)e->max_hp, 4}, RED);
            } else
                label(f, "변이한 멧돼지", (Vec2){p.x, p.y - 65}, (SDL_Color){219, 186, 162, 255});
        } else if (actors[a].kind == 2) {
            p = screen_pos(g->boss.pos, camera);
            shadow(f, p, 23, g->boss.hp > 0 || g->training.active ? 1 : clampf(g->boss.dying / ENEMY_DEATH_SECONDS, 0, 1));
            if (g->training.active) {
                SDL_Color straw = g->boss.hit_time > 0 ? PAPER : GOLD;
                float sway = g->training.hit_time > 0 ? sinf(g->training.hit_time * 55) * 3 : 0;
                gfx_round(f, (SDL_FRect){p.x - 4, p.y - 45, 8, 45}, 3, (SDL_Color){104, 74, 47, 255});
                gfx_round(f, (SDL_FRect){p.x - 26 + sway, p.y - 40, 52, 7}, 3, straw);
                gfx_round(f, (SDL_FRect){p.x - 13 + sway, p.y - 43, 26, 29}, 7, straw);
                gfx_disc(f, (Vec2){p.x + sway, p.y - 53}, 10, straw, INK);
                gfx_line(f, (Vec2){p.x - 12 + sway, p.y - 29}, (Vec2){p.x + 12 + sway, p.y - 29}, INK);
                label(f, "허수아비", (Vec2){p.x, p.y - 78}, PAPER);
                continue;
            }
            gfx_enemy(f, &f->boss, (Vec2){p.x, p.y + 7}, 83, 96, g->boss.pos.x < g->hero.pos.x, &g->boss,
                      (EnemyTiming){BOSS_WINDUP_SECONDS, BOSS_SWING_SECONDS, 0}, g->time,
                      g->phase == PH_PLAY);
            cover_actor(f, g->boss.pos, camera, 83, 98);
            if (g->boss.hp <= 0) continue;
            if (g->boss.state == AI_RECOVER)
                label(f, "빈틈", (Vec2){p.x, p.y - 98}, GOLD);
            else if (g->quest == Q_HUNT)
                label(f, "변이한 백정", (Vec2){p.x, p.y - 98}, PAPER);
        } else {
            p = screen_pos(g->hero.pos, camera);
            shadow(f, p, 10, 1);
            gfx_hero(f, g, p);
            /* A faint silhouette keeps the player readable behind tall scenery. */
            SDL_SetTextureAlphaMod(f->map.texture, 205);
            cover_actor(f, g->hero.pos, camera,
                        g->hero.skilling ? 180 : g->hero.attacking ? 96 : 56,
                        g->hero.skilling ? 110 : g->hero.attacking ? 64 : 48);
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
            /* Pop in large, settle, rise with ease-out and fade at the end.
             * A per-position nudge keeps simultaneous numbers apart. */
            char s[24];
            snprintf(s, sizeof s, "%s%d", e->damage ? "-" : "", e->amount);
            float total = e->damage ? .8f : .7f, t = clampf(1 - e->life / total, 0, 1);
            float rise = 1 - (1 - t) * (1 - t) * (1 - t);
            float pop = t < .18f ? 1 + .45f * (1 - t / .18f) * (1 - t / .18f) : 1;
            float nudge = (fmodf(fabsf(e->pos.x * 12.9898f + e->pos.y * 78.233f), 1) - .5f) * 14;
            float size = (e->amount >= 20 ? 20 : 16) * pop;
            if (!e->damage && e->amount >= 20) c = (SDL_Color){255, 226, 140, 255};
            c.a = (Uint8)(255 * clampf((1 - t) / .3f, 0, 1));
            float x = p.x + nudge, y = p.y - 6 - rise * 26 - size * .5f;
            gfx_center(f, s, x + 1, y + 1, size, (SDL_Color){20, 12, 10, (Uint8)(c.a * .8f)});
            gfx_center(f, s, x, y, size, c);
        } else if (e->kind == FX_DUST) {
            continue;
        } else if (e->kind == FX_SKILL) {
            /* A shock ring expanding from the strike point. */
            float t = clampf(1 - e->life / .30f, 0, 1), grow = 1 - (1 - t) * (1 - t);
            c.a = (Uint8)(220 * (1 - t));
            gfx_circle(f, (Vec2){p.x, p.y - 13}, 18 + 30 * grow, c);
            c.a = (Uint8)(120 * (1 - t));
            gfx_circle(f, (Vec2){p.x, p.y - 13}, 12 + 22 * grow, c);
        }
        else if (e->kind == FX_WIND)
            gfx_line(f, (Vec2){p.x, p.y - 17}, (Vec2){q.x, q.y - 22}, (SDL_Color){204, 238, 203, 255});
        else if (e->kind == FX_BOSS_SWING)
            gfx_sector(f, p, 65, e->angle, 1.1f, (SDL_Color){244, 178, 154, 100});
        else {
            float fade = clampf(e->life / .25f, 0, 1), radius = 5 + 10 * (1 - fade);
            Vec2 center = {q.x, q.y - 24};
            SDL_Color light = e->damage ? (SDL_Color){255, 190, 155, (Uint8)(255 * fade)}
                                       : (SDL_Color){255, 239, 195, (Uint8)(255 * fade)};
            for (int ray = 0; ray < 6; ray++) {
                float angle = e->angle + ray * PI / 3;
                gfx_line(f, (Vec2){center.x + cosf(angle) * 3, center.y + sinf(angle) * 3},
                            (Vec2){center.x + cosf(angle) * radius, center.y + sinf(angle) * radius}, light);
            }
            /* Chips fly off the struck side and fall under gravity. */
            float age = .25f - e->life;
            for (int chip = 0; chip < 6; chip++) {
                float a = e->angle + (scatter(e->target, chip) - .5f) * 2.2f;
                float speed = 110 + 80 * scatter(e->target, chip + 6);
                Vec2 at = {center.x + cosf(a) * speed * age,
                           center.y + sinf(a) * speed * age - 40 * age + 260 * age * age};
                gfx_rect(f, (SDL_FRect){at.x - 1.5f, at.y - 1.5f, 3, 3}, light);
            }
        }
    }
    Vec2 objective = game_objective(g);
    if (!g->training.active && g->phase == PH_PLAY && g->quest != Q_COMBAT && distance(objective, g->hero.pos) > 70) {
        float angle = atan2f(objective.y - g->hero.pos.y, objective.x - g->hero.pos.x);
        Vec2 p = screen_pos(g->hero.pos, camera);
        p.x += cosf(angle) * 42;
        p.y += sinf(angle) * 42 - 17;
        gfx_sector(f, p, 9, angle, .55f, GOLD);
    }
}
