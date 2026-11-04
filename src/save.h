#ifndef PALDO_SAVE_H
#define PALDO_SAVE_H
#include "game.h"
#include <stdint.h>

#define SAVE_VERSION 4u
/* v3 = v2 (224 bytes) + hunting round + health and respawn time left for
 * each ordinary boar. */
#define SAVE_V3_SIZE (224u + 4u + 8u * HUNT_COUNT)
/* v4 appends bag quantities, loot RNG, and fixed independent ground lanes.
 * Each pile is quantity, x, y, rest age and magnet acceleration time. */
#define SAVE_V4_SIZE (SAVE_V3_SIZE + 4u * (MATERIAL_COUNT + 1u) + 20u * MATERIAL_DROP_MAX)
#define SAVE_CAPACITY 1024
typedef enum {
    SAVE_OK, SAVE_MISSING, SAVE_CORRUPT, SAVE_VERSION_NEW,
    SAVE_UNAVAILABLE, SAVE_FULL, SAVE_IO, SAVE_INVALID
} SaveResult;
/* Explicit little-endian fields; never a dump of Game or another C struct. */
SaveResult save_encode(const Game *game, unsigned char *bytes, size_t *size);
/* Validate into a temporary Game before touching the destination. */
SaveResult save_decode(Game *game, const unsigned char *bytes, size_t size);
/* Same accept/reject result as save_decode, without repairing positions
 * (no route searches). For deciding whether stored bytes are loadable. */
SaveResult save_check(const unsigned char *bytes, size_t size);
const char *save_result_text(SaveResult result);
#endif
