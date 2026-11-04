#include "save_store.h"
#include "world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIR(p) mkdir(p, 0700)
#endif
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL save:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
static unsigned char bytes[SAVE_CAPACITY];
static size_t size;
/* Every decode in this file also checks that the route-free acceptance test
 * used by save_store_write agrees with the full decode. */
static SaveResult decode_checked(Game *out, const unsigned char *b, size_t n) {
    SaveResult quick = save_check(b, n), full = save_decode(out, b, n);
    CHECK(quick == full);
    return full;
}
static Game roundtrip(const Game *g) {
    Game loaded;
    CHECK(save_encode(g, bytes, &size) == SAVE_OK);
    CHECK(size == SAVE_V4_SIZE);
    MapId before = world_selected();
    CHECK(decode_checked(&loaded, bytes, size) == SAVE_OK);
    CHECK(world_selected() == before);
    return loaded;
}
static void begin(Game *g) {
    game_reset(g); game_start(g);
    g->hero.pos = NPCS[ELDER].pos; CHECK(game_accept(g));
    g->map = MAP_PASS;
    for (int i = 0; i < TRACE_COUNT; i++) {
        g->hero.pos = TRACES[i].pos; CHECK(game_inspect(g, i));
    }
}
static void finish_herbs(Game *g) {
    for (int i = 0; i < BOAR_COUNT; i++) {
        g->hero.pos = g->boars[i].pos;
        CHECK(game_damage(g, &g->boars[i], 100, FX_HIT));
        *g = roundtrip(g); /* Killed, currency awarded, herb not collected. */
        CHECK(g->boars[i].hp == 0 && g->drops[i].exists && !g->drops[i].collected);
        CHECK(!game_damage(g, &g->boars[i], 100, FX_HIT));
        g->hero.pos = g->drops[i].pos;
        CHECK(game_collect(g, i));
        *g = roundtrip(g); CHECK(!game_collect(g, i));
    }
}
static void delivery(Game *g) {
    g->map = MAP_VILLAGE; g->hero.pos = NPCS[HERBALIST].pos;
    CHECK(game_deliver(g));
    *g = roundtrip(g);
    CHECK(g->herbs == 0 && g->coins == 137 && g->tonics == 4 && g->bandages == 2);
    CHECK(!game_deliver(g));
    g->hero.pos = NPCS[ELDER].pos; CHECK(game_report(g));
}
static void chapter(void) {
    Game g; game_reset(&g); game_start(&g);
    Game loaded = roundtrip(&g);
    CHECK(loaded.quest == Q_MEET && loaded.hero.hp == 100 && loaded.coins == 55);
    g.phase = PH_PROLOGUE; loaded = roundtrip(&g); CHECK(loaded.phase == PH_PROLOGUE);
    g.phase = PH_DIALOGUE; loaded = roundtrip(&g); CHECK(loaded.phase == PH_PLAY && loaded.quest == Q_MEET);
    begin(&g);
    for (int m = 0; m < 3; m++) {
        g.map = (MapId)m; g.hero.pos = m == 0 ? HOME : MAP_EXITS[m - 1].arrival;
        g.hero.hp = 31; g.hero.bleed = 3.5f; g.hero.bleed_tick = .4f;
        g.cooldown = 2.25f; g.item_cooldown = .5f; g.attack_cooldown = .5f;
        g.phase = PH_BAG; g.owned_sword = g.owned_armor = true;
        CHECK(game_equip(&g, ITEM_SWORD, true)); CHECK(game_equip(&g, ITEM_ARMOR, true));
        g.phase = PH_TECHNIQUES; CHECK(game_bind_swipe(&g, SWIPE_UP, TECH_SWEEP));
        g.phase = PH_PLAY; g.hero.attacking = true; g.queued_move = TECH_THRUST; g.effect_count = 1;
        loaded = roundtrip(&g);
        CHECK(loaded.map == m && distance(loaded.hero.pos, g.hero.pos) < .001f);
        CHECK(loaded.hero.hp == 31 && loaded.hero.bleed == 3.5f && loaded.hero.bleed_tick == .4f);
        CHECK(loaded.sword && loaded.armor && loaded.owned_sword && loaded.owned_armor);
        CHECK(loaded.swipe_bindings[0] == TECH_SWEEP && loaded.cooldown == 2.25f && loaded.item_cooldown == .5f);
        CHECK(!loaded.hero.attacking && loaded.queued_move == MOVE_NONE && loaded.effect_count == 0);
    }
    begin(&g); finish_herbs(&g);
    CHECK(g.quest == Q_DELIVER && g.herbs == 3 && g.coins == 82);
    g.phase = PH_DIALOGUE; loaded = roundtrip(&g);
    CHECK(loaded.phase == PH_PLAY && loaded.quest == Q_DELIVER && !loaded.supplies_delivered);
    g.phase = PH_PLAY; delivery(&g);
    g.map = MAP_CAVE; g.hero.pos = MAP_EXITS[1].arrival;
    CHECK(game_begin_boss(&g));
    g.hero.hp = 17; g.boss.hp = 25; g.tonics = 1;
    loaded = roundtrip(&g);
    CHECK(loaded.quest == Q_COMBAT && loaded.map == MAP_CAVE && loaded.hero.hp == 100 && loaded.boss.hp == 240);
    CHECK(loaded.tonics == 1 && loaded.coins == 137);
    CHECK(!loaded.hero.attacking && loaded.effect_count == 0 && loaded.hero.bleed == 0);
    g.phase = PH_DEAD; g.hero.hp = 0; loaded = roundtrip(&g);
    CHECK(loaded.map == MAP_VILLAGE && loaded.quest == Q_HUNT && loaded.hero.hp == 100);
    g = loaded; g.map = MAP_CAVE; CHECK(game_begin_boss(&g));
    CHECK(game_damage(&g, &g.boss, 1000, FX_HIT));
    g = roundtrip(&g); CHECK(g.quest == Q_RETURN && g.boss_rewarded && g.coins == 207);
    CHECK(!game_damage(&g, &g.boss, 1000, FX_HIT));
    g.phase = PH_DIALOGUE; loaded = roundtrip(&g); CHECK(loaded.quest == Q_RETURN && loaded.phase == PH_PLAY);
    g.phase = PH_AUGMENT; g = roundtrip(&g); CHECK(g.phase == PH_AUGMENT);
    CHECK(game_choose(&g, AUG_FLOW));
    g = roundtrip(&g); CHECK(g.quest == Q_MESSENGER && g.augment == AUG_FLOW && !game_choose(&g, AUG_STEEL));
    g.phase = PH_COMPLETE; g = roundtrip(&g); CHECK(g.phase == PH_COMPLETE);
    g.replay = true; game_retry(&g); g.hero.hp = 0; g.phase = PH_DEAD;
    g = roundtrip(&g); CHECK(g.map == MAP_CAVE && g.quest == Q_COMBAT && g.hero.hp == 100 && g.coins == 207);
    CHECK(game_damage(&g, &g.boss, 1000, FX_HIT));
    g.phase = PH_COMPLETE; g = roundtrip(&g); CHECK(g.coins == 207 && g.phase == PH_COMPLETE);
    begin(&g); g.hero.hp = 0; g.phase = PH_DEAD;
    g = roundtrip(&g); CHECK(g.map == MAP_VILLAGE && g.hero.hp == 100 && g.quest == Q_RECOVER);
}
static void sum_again(void) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < size - 4; i++) h = (h ^ bytes[i]) * 16777619u;
    for (int i = 0; i < 4; i++) bytes[size - 4 + i] = (unsigned char)(h >> (8 * i));
}
static void combat_state_and_v1(void) {
    Game g; begin(&g);
    CHECK(game_dodge(&g, (Vec2){1, 0}));
    g.perfect_bonus = .7f; g.perfect_slow = .2f; g.perfect_flash = .5f;
    g.chain_hits = 12; g.chain_last = 7; g.combo_count = 3; g.cancel_count = 9;
    Game loaded = roundtrip(&g);
    CHECK(loaded.dodge_charges == 1 && loaded.dodge_recharge == DODGE_RECHARGE_SECONDS);
    CHECK(!loaded.hero.dodging && loaded.hero.dodge_elapsed == 0 && loaded.hero.invul == 0);
    CHECK(loaded.perfect_bonus == 0 && loaded.perfect_slow == 0 && loaded.perfect_flash == 0);
    CHECK(loaded.chain_hits == 0 && loaded.combo_count == 0 && loaded.cancel_count == 0);
    loaded.hero.pos = MAP_EXITS[0].arrival;
    CHECK(game_dodge(&loaded, (Vec2){1, 0}));
    loaded = roundtrip(&loaded); CHECK(loaded.dodge_charges == 0 && !game_dodge(&loaded, (Vec2){1, 0}));
    g.dodge_charges = 3; CHECK(save_encode(&g, bytes, &size) == SAVE_INVALID);
    g.dodge_charges = 0; g.dodge_recharge = NAN;
    CHECK(save_encode(&g, bytes, &size) == SAVE_INVALID);
    g.dodge_recharge = .7f; CHECK(save_encode(&g, bytes, &size) == SAVE_OK);
    /* Exact old v1 layout: delete the two appended fields, then re-sign it. */
    size = 216; bytes[4] = 1; bytes[8] = 216; bytes[9] = 0; sum_again();
    CHECK(decode_checked(&loaded, bytes, size) == SAVE_OK);
    CHECK(loaded.quest == g.quest && loaded.coins == g.coins && loaded.hero.hp == g.hero.hp);
    CHECK(loaded.dodge_charges == DODGE_CHARGES && loaded.dodge_recharge == 0);
    CHECK(save_encode(&loaded, bytes, &size) == SAVE_OK && bytes[4] == SAVE_VERSION && size == SAVE_V4_SIZE);
    bytes[212] = 3; sum_again();
    CHECK(decode_checked(&loaded, bytes, size) == SAVE_CORRUPT);
}
static void invalid_input(void) {
    Game g; begin(&g); g.hero.pos = (Vec2){NAN, INFINITY};
    Game out = roundtrip(&g); CHECK(distance(out.hero.pos, MAP_EXITS[0].arrival) == 0);
    g.hero.pos = (Vec2){1140, 675}; out = roundtrip(&g);
    CHECK(distance(out.hero.pos, MAP_EXITS[0].arrival) == 0);
    g.hero.pos = g.boars[0].pos; CHECK(game_damage(&g, &g.boars[0], 99, FX_HIT));
    g.drops[0].pos = (Vec2){-500, NAN}; out = roundtrip(&g);
    CHECK(distance(out.drops[0].pos, out.boars[0].home) == 0);
    g = out; Game sentinel = g;
    for (size_t n = 0; n < size; n++) {
        CHECK(decode_checked(&out, bytes, n) != SAVE_OK); CHECK(!memcmp(&out, &sentinel, sizeof out));
    }
    for (size_t i = 0; i < size; i++) {
        unsigned char old = bytes[i]; bytes[i] ^= 128;
        CHECK(decode_checked(&out, bytes, size) != SAVE_OK); CHECK(!memcmp(&out, &sentinel, sizeof out)); bytes[i] = old;
    }
    bytes[4] = 99; CHECK(decode_checked(&out, bytes, size) == SAVE_VERSION_NEW);
    CHECK(!memcmp(&out, &sentinel, sizeof out));
    CHECK(save_encode(&g, bytes, &size) == SAVE_OK);
    bytes[48] = 255; bytes[49] = 255; bytes[50] = 255; bytes[51] = 127; sum_again();
    CHECK(decode_checked(&out, bytes, size) == SAVE_CORRUPT);
    CHECK(save_encode(&g, bytes, &size) == SAVE_OK);
    bytes[64] = 2; sum_again(); CHECK(decode_checked(&out, bytes, size) == SAVE_CORRUPT);
    CHECK(save_encode(&g, bytes, &size) == SAVE_OK);
    bytes[156] = 0; sum_again(); /* Dead boar without its drop: reject coherently. */
    CHECK(decode_checked(&out, bytes, size) == SAVE_CORRUPT);
    Game training, saved; training = g;
    CHECK(game_enter_training(&training, &saved));
    CHECK(save_encode(&training, bytes, &size) == SAVE_INVALID);
    CHECK(game_leave_training(&training, &saved));
    CHECK(save_encode(&training, bytes, &size) == SAVE_OK);
}
static void file_path(char *out, const char *dir, const char *name) { snprintf(out, 2400, "%s/%s", dir, name); }
static void storage(const char *dir) {
    MAKE_DIR(dir);
    char path[2400];
    const char *names[] = {"journey-autosave.sav", "journey-autosave.bak", "journey-autosave.tmp", "journey-autosave.bak.tmp"};
    for (int i = 0; i < 4; i++) { file_path(path, dir, names[i]); remove(path); }
    SaveStore s; save_store_init(&s, dir);
    Game g, out; game_reset(&g); game_start(&g); bool recovered;
    CHECK(save_store_load(&s, &out, &recovered) == SAVE_MISSING);
    CHECK(save_store_write(&s, &g) == SAVE_OK);
    g.coins = 44; CHECK(save_store_write(&s, &g) == SAVE_OK);
    CHECK(save_store_load(&s, &out, &recovered) == SAVE_OK && !recovered && out.coins == 44);
    file_path(path, dir, names[0]); FILE *f = fopen(path, "wb"); CHECK(f != NULL); fputs("broken", f); fclose(f);
    CHECK(save_store_load(&s, &out, &recovered) == SAVE_OK && recovered && out.coins == 55);
    g.coins = 33; CHECK(save_store_write(&s, &g) == SAVE_OK);
    CHECK(save_store_load(&s, &out, &recovered) == SAVE_OK && !recovered && out.coins == 33);
    /* The broken primary was not copied over the backup: it still holds 55. */
    {
        unsigned char kept[SAVE_CAPACITY]; size_t kept_size;
        file_path(path, dir, names[1]); f = fopen(path, "rb"); CHECK(f != NULL);
        kept_size = fread(kept, 1, sizeof kept, f); fclose(f);
        CHECK(decode_checked(&out, kept, kept_size) == SAVE_OK && out.coins == 55);
    }
    /* A directory at the temporary filename deterministically forces a write failure. */
    file_path(path, dir, names[2]); remove(path); CHECK(MAKE_DIR(path) == 0);
    g.coins = 22; CHECK(save_store_write(&s, &g) != SAVE_OK);
    CHECK(save_store_load(&s, &out, &recovered) == SAVE_OK && out.coins == 33);
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
    SaveStore absent; save_store_init(&absent, NULL);
    CHECK(save_store_write(&absent, &g) == SAVE_UNAVAILABLE);
    Game training, saved; training = g; CHECK(game_enter_training(&training, &saved));
    CHECK(save_store_write(&s, &training) == SAVE_INVALID);
    CHECK(save_store_load(&s, &out, &recovered) == SAVE_OK && out.coins == 33);
    CHECK(save_encode(&g, bytes, &size) == SAVE_OK); bytes[4] = 9;
    file_path(path, dir, names[0]); f = fopen(path, "wb"); CHECK(f != NULL); fwrite(bytes, 1, size, f); fclose(f);
    file_path(path, dir, names[1]); remove(path);
    CHECK(save_store_load(&s, &out, &recovered) == SAVE_VERSION_NEW);
    file_path(path, dir, names[0]); f = fopen(path, "wb"); CHECK(f != NULL); fputs("bad", f); fclose(f);
    CHECK(save_store_load(&s, &out, &recovered) == SAVE_CORRUPT);
    for (int i = 0; i < 4; i++) { file_path(path, dir, names[i]); remove(path); }
}
int main(int argc, char **argv) {
    chapter(); combat_state_and_v1(); invalid_input(); storage(argc > 1 ? argv[1] : "save-fixtures");
    printf("PASS: %d save/quest/recovery/storage checks\n", checks); return 0;
}
