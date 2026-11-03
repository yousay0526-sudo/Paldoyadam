#ifndef PALDO_SAVE_STORE_H
#define PALDO_SAVE_STORE_H
#include "save.h"
typedef struct { char directory[2048]; } SaveStore;
/* directory comes from SDL_GetPrefPath, never the executable/working directory. */
void save_store_init(SaveStore *store, const char *directory);
SaveResult save_store_load(const SaveStore *store, Game *game, bool *recovered);
SaveResult save_store_write(const SaveStore *store, const Game *game);
#endif
