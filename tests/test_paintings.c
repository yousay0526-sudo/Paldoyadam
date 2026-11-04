/* The large opaque paintings ship as JPEG copies of lossless masters in art/
 * (scripts/prepare_web_paintings.py). Each copy must decode with the game's
 * stb_image build, keep the master's size and stay visually equal to it, so a
 * repainted master cannot ship with a stale runtime copy. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static const char *const PAINTINGS[][2] = {
    {"art/village/village-remake-pass1-master.png", "assets/village.jpg"},
    {"art/maps/village.png", "assets/maps/village.jpg"},
    {"art/maps/solbaramjae.png", "assets/maps/solbaramjae.jpg"},
    {"art/maps/tamsikgul.png", "assets/maps/tamsikgul.jpg"},
    {"art/prologue/01-homeward.png", "assets/prologue/01-homeward.jpg"},
    {"art/prologue/02-hunger.png", "assets/prologue/02-hunger.jpg"},
    {"art/prologue/03-reflection.png", "assets/prologue/03-reflection.jpg"},
};

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : ".";
    for (size_t i = 0; i < sizeof PAINTINGS / sizeof *PAINTINGS; i++) {
        char master_path[2048], runtime_path[2048];
        snprintf(master_path, sizeof master_path, "%s/%s", root, PAINTINGS[i][0]);
        snprintf(runtime_path, sizeof runtime_path, "%s/%s", root, PAINTINGS[i][1]);
        int mw, mh, rw, rh, channels;
        unsigned char *master = stbi_load(master_path, &mw, &mh, &channels, 3);
        unsigned char *runtime = stbi_load(runtime_path, &rw, &rh, &channels, 3);
        if (!master || !runtime)
            fprintf(stderr, "cannot decode %s or %s\n", master_path, runtime_path);
        CHECK(master && runtime);
        CHECK(mw == rw && mh == rh);
        double squared = 0;
        size_t n = (size_t)mw * mh * 3;
        for (size_t k = 0; k < n; k++) {
            double d = (double)master[k] - runtime[k];
            squared += d * d;
        }
        double psnr = 10 * log10(255.0 * 255.0 / (squared / n));
        printf("%s: %dx%d, PSNR %.1f dB\n", PAINTINGS[i][1], rw, rh, psnr);
        /* Quality 95 at 4:4:4 measures 38-44 dB; a stale or wrong copy is far lower. */
        CHECK(psnr >= 36);
        stbi_image_free(master);
        stbi_image_free(runtime);
    }
    printf("painting checks passed: %d\n", checks);
    return 0;
}
