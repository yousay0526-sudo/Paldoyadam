#ifndef PALDO_EQUIPMENT_H
#define PALDO_EQUIPMENT_H
#include <SDL.h>
#include <stdbool.h>

/* South-view visual prototype only. No Game, inventory or combat state. */
typedef enum {
    EQ_HEAD, EQ_TUNIC, EQ_SASH, EQ_RIGHT_UPPER, EQ_RIGHT_FORE,
    EQ_LEFT_UPPER, EQ_LEFT_FORE, EQ_RIGHT_LEG, EQ_LEFT_LEG,
    EQ_ARMOR, EQ_RIGHT_ARMOR_LEG, EQ_LEFT_ARMOR_LEG, EQ_HELMET,
    EQ_PLAIN_SWORD, EQ_ORNATE_SWORD, EQ_PELVIS, EQUIPMENT_PART_COUNT
} EquipmentPart;
typedef enum { EQ_IDLE, EQ_WALK, EQ_SLASH } EquipmentMotion;
typedef struct { bool upper, lower, helmet; int weapon; } EquipmentLoadout;
typedef struct {
    EquipmentPart part;
    SDL_FRect box;
    SDL_FPoint pivot;
    double angle;
    bool hand_only;
} EquipmentLayer;
typedef struct {
    EquipmentLayer layers[20];
    int count;
    SDL_FPoint right_hand, weapon_grip;
    SDL_FRect bounds;
} EquipmentPose;
typedef struct { SDL_Texture *texture; } EquipmentArt;

bool equipment_load(EquipmentArt *art, SDL_Renderer *renderer, const char *path);
void equipment_free(EquipmentArt *art);
/* feet is a fixed world/display root. scale=1 uses a 128x192 design space.
 * Phase is normalized, independent of gameplay clocks. Never mirrors parts. */
EquipmentPose equipment_pose(EquipmentLoadout loadout, EquipmentMotion motion,
                             float phase, SDL_FPoint feet, float scale);
bool equipment_draw(const EquipmentArt *art, SDL_Renderer *renderer, const EquipmentPose *pose);
#endif
