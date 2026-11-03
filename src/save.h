#ifndef PALDO_SAVE_H
#define PALDO_SAVE_H
#include "game.h"
#include <stdint.h>

#define SAVE_VERSION 1u
#define SAVE_CAPACITY 512
typedef enum {
    SAVE_OK, SAVE_MISSING, SAVE_CORRUPT, SAVE_VERSION_NEW,
    SAVE_UNAVAILABLE, SAVE_FULL, SAVE_IO, SAVE_INVALID
} SaveResult;
/* Explicit little-endian fields; never a dump of Game or another C struct. */
SaveResult save_encode(const Game *game, unsigned char *bytes, size_t *size);
/* Validate into a temporary Game before touching the destination. */
SaveResult save_decode(Game *game, const unsigned char *bytes, size_t size);
const char *save_result_text(SaveResult result);
#endif
