#pragma once
#include "lvgl.h"
#include "GameTypes.h"

// ── Variables globales définies dans main.cpp ─────────────────────────────────
extern Platform platforms[MAX_PLATFORMS];
extern int platformCount;
extern Goomba goombas[MAX_GOOMBAS];
extern int goombaCount;
extern Block blocks[MAX_BLOCKS];
extern int blockCount;
extern float flagX;
extern float cameraX;

// ── Fonctions définies dans main.cpp ─────────────────────────────────────────
extern lv_obj_t *makeRect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color);
extern void createGoombaSprites(lv_obj_t *scr);

// ── Classe abstraite Level ────────────────────────────────────────────────────
class Level
{
public:
    virtual void load(lv_obj_t *scr) = 0;
    virtual ~Level() {}

protected:
    // ── Helpers sol / plateformes / grottes ───────────────────────────────────
    // Version "libre" (largeur/hauteur/couleur au choix) : gardée pour usage
    // interne (addCave s'appuie dessus) et flexibilité future.
    void addPlatform(float x, float y, float w, float h, uint32_t color)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, y, w, h, color, false};
    }
    // Version "uniforme" : c'est CELLE-CI que tous les Level*.h utilisent
    // maintenant pour les plateformes flottantes. Taille et couleur fixées une
    // fois pour toutes (PLATFORM_W/PLATFORM_H/PLATFORM_COLOR dans GameTypes.h)
    // → impossible de créer accidentellement des plateformes de tailles ou
    // couleurs différentes d'un niveau à l'autre.
    void addPlatform(float x, float y)
    {
        addPlatform(x, y, PLATFORM_W, PLATFORM_H, PLATFORM_COLOR);
    }
    void addCave(float x, float w, uint32_t color)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, (float)CAVE_Y, w, (float)CAVE_H, color, true};
    }
    void addGround(float x, float w, uint32_t color = GROUND_DIRT_COLOR)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, GROUND_Y, w, (float)(SCREEN_H - (int)GROUND_Y), color, false};
    }

    // ── Helper Goomba ─────────────────────────────────────────────────────────
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

    // ── Helper Bloc ? ─────────────────────────────────────────────────────────
    // x, y     : coin haut-gauche du bloc dans le monde
    // puType   : 1=champignon  2=fleur de feu  3=mini champignon
    //
    // POURQUOI y est NÉGATIF par rapport aux plateformes ?
    // Les plateformes ont y=sol (200). Un bloc en l'air a y=160 (plus petit = plus haut).
    // On le place sous une plateforme flottante en général.
    //
    // Taille du bloc : 16×16 px (même largeur que le joueur).
    void addBlock(float x, float y, int puType)
    {
        if (blockCount >= MAX_BLOCKS)
            return;
        // Le sprite LVGL (et le label "?") sera créé dans initGameObjects()
        // après le chargement, comme pour les plateformes (obj/mark = nullptr ici).
        blocks[blockCount++] = {x, y, puType, false, nullptr, nullptr};
    }
};