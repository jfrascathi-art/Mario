#pragma once
#include "lvgl.h"

// ─────────────────────────────────────────────────────────────────────────────
// GameTypes.h  —  Types et constantes partagés entre main.cpp et Level*.h
//
// Ce fichier est inclus par main.cpp ET par Level.h.
// Les structs Platform et Goomba ne sont définies QU'ICI, une seule fois.
// ─────────────────────────────────────────────────────────────────────────────

// ── Constantes monde ──────────────────────────────────────────────────────────
#define GROUND_Y 200.0f
#define SCREEN_W 480
#define SCREEN_H 272
#define GROUND_VISUAL_Y 200
#define WORLD_W 3200

#define CAVE_Y 0
#define CAVE_H ((int)(GROUND_Y - 22))

#define CAVE_COL 0x4E342E

#define MAX_PLATFORMS 32
#define MAX_GOOMBAS 12
#define GOOMBA_W 14.0f
#define GOOMBA_H 16.0f
#define GOOMBA_SPEED 1.2f

// ── Structs ───────────────────────────────────────────────────────────────────
struct Platform
{
    float x, y, w, h;
    uint32_t color;
    bool isCave;
};

struct Goomba
{
    float x, y, velX;
    bool alive;
    float patrolLeft, patrolRight;
    lv_obj_t *objHat, *objBody, *objFeetL, *objFeetR;
};
