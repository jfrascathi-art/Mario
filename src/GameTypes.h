#pragma once
#include "lvgl.h"

// ─────────────────────────────────────────────────────────────────────────────
// GameTypes.h  —  Types et constantes partagés entre main.cpp et Level*.h
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
#define CAVE_COL3 0x3E2723
#define CAVE_COL4 0x212121

#define MAX_PLATFORMS 32
#define MAX_GOOMBAS 12
#define GOOMBA_W 14.0f
#define GOOMBA_H 16.0f
#define GOOMBA_SPEED 1.2f

// ── Power-ups ─────────────────────────────────────────────────────────────────
// MAX_BLOCKS : nombre max de blocs ? par niveau
// MAX_ITEMS  : items qui tombent du bloc après qu'on le frappe
// MAX_FIREBALLS : boules de feu simultanées à l'écran
#define MAX_BLOCKS 16
#define MAX_ITEMS 4
#define MAX_FIREBALLS 4

// Durée de l'effet fleur de feu en frames (25fps × 5s = 125 frames)
#define FIRE_DURATION 125

// ── Structs plateforme et ennemi ──────────────────────────────────────────────
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

// ── Bloc ? ────────────────────────────────────────────────────────────────────
// Un bloc ? est un bloc en l'air que le joueur frappe par en-dessous.
// Quand il est frappé (hit=true), un item apparaît au-dessus.
// powerUpType : 1=champignon, 2=fleur de feu, 3=mini
struct Block
{
    float x, y;      // coin haut-gauche dans le monde
    int powerUpType; // type d'item qu'il contient (0=brique déco, 1/2/3=power-up)
    bool hit;        // true = déjà frappé (bloc vide, couleur grise)
    lv_obj_t *obj;   // rectangle LVGL du bloc
    lv_obj_t *mark;  // label "?" affiché par-dessus (nullptr si type 0)
};

// ── Item au sol (item qui tombe du bloc et attend d'être ramassé) ─────────────
// L'item apparaît au-dessus du bloc, tombe sur le sol, et attend le joueur.
// active=true tant qu'il n'a pas été ramassé.
struct Item
{
    float x, y, velY; // position + vitesse verticale (item tombe)
    int type;         // 1=champignon, 2=fleur de feu, 3=mini
    bool active;
    bool onGround;
    lv_obj_t *obj;
};

// ── Boule de feu ──────────────────────────────────────────────────────────────
// Tirée par le joueur avec joystick haut + bouton DOWN.
// Vole horizontalement (direction du joueur), détruites en touchant un Goomba
// ou en sortant de l'écran.
struct FireBall
{
    float x, y, velX;
    bool active;
    lv_obj_t *obj;
};