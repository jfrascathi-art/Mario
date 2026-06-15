#pragma once
#include "lvgl.h"
#include "GameTypes.h"

// Variables globales définies dans main.cpp
extern Platform platforms[MAX_PLATFORMS];
extern int platformCount;
extern Goomba goombas[MAX_GOOMBAS];
extern int goombaCount;
extern float flagX;
extern float cameraX;

// Fonctions définies dans main.cpp (plus static !)
extern lv_obj_t *makeRect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color);
extern void createGoombaSprites(lv_obj_t *scr);

class Level
{
public:
    virtual void load(lv_obj_t *scr) = 0;
    virtual ~Level() {}

protected:
    void addPlatform(float x, float y, float w, float h, uint32_t color)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, y, w, h, color, false};
    }
    void addCave(float x, float w, uint32_t color)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, (float)CAVE_Y, w, (float)CAVE_H, color, true};
    }
    void addGround(float x, float w, uint32_t color = 0x4A7C3F)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, GROUND_Y, w, (float)(SCREEN_H - (int)GROUND_Y), color, false};
    }
    void addGoomba(float startX, float startY, float patrolLeft, float patrolRight)
    {
        if (goombaCount >= MAX_GOOMBAS)
            return;
        goombas[goombaCount++] = {
            startX, startY, -GOOMBA_SPEED,
            true, patrolLeft, patrolRight,
            nullptr, nullptr, nullptr, nullptr};
    }
    void addGoombaOnPlatform(int pidx, float startX)
    {
        if (pidx < 0 || pidx >= platformCount)
            return;
        Platform &p = platforms[pidx];
        addGoomba(startX, p.y, p.x + 2, p.x + p.w - GOOMBA_W - 2);
    }
};