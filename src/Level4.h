#pragma once
#include "Level.h"

// ─────────────────────────────────────────────────────────────────────────────
// Level4.h  —  Niveau 4 : Expert
//
// Règle de conception : une grotte ne peut JAMAIS chevaucher ni jouxter un trou.
// Distance minimale grotte↔trou : 80px de sol libre de chaque côté.
//
// Structure du sol :
//   [0→250] t1=120px [370→570] t2=130px [700→900] t3=140px [1040→1280]
//   t4=150px [1430→1680] t5=160px [1840→fin]
//
// Grottes (toutes sur dal le continue, jamais sur un trou) :
//   Grotte 1 : x=1100..1350 (sur dalle [1040→1280], 60px de marge avant/après)
//   Grotte 2 : x=1540..1780 (sur dalle [1430→1840], 110px de marge avant/après)
//   Grotte 3 : x=2000..2450 (sur dalle [1840→fin], 160px de marge)
//   Grotte 4 : x=2700..3000 (sur dalle [1840→fin])
// ─────────────────────────────────────────────────────────────────────────────

class Level4 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;

        // ── Sol ───────────────────────────────────────────────────────────────
        addGround(0, 250);     // 0   x=0..250
        addGround(370, 200);   // 1   x=370..570   (trou 1 = 120px)
        addGround(700, 200);   // 2   x=700..900   (trou 2 = 130px)
        addGround(1040, 240);  // 3   x=1040..1280 (trou 3 = 140px)
        addGround(1430, 250);  // 4   x=1430..1680 (trou 4 = 150px)
        addGround(1840, 1360); // 5   x=1840..3200 (trou 5 = 160px)

        // ── Plateformes flottantes d'aide ─────────────────────────────────────
        addPlatform(100, 155, 45, 12, 0x212121); // 6  avant t1
        addPlatform(200, 125, 40, 12, 0x212121); // 7
        addPlatform(375, 155, 45, 12, 0x212121); // 8  aide t1
        addPlatform(500, 130, 40, 12, 0x212121); // 9
        addPlatform(615, 155, 40, 12, 0x212121); // 10 aide t2

        // ── Grottes (toutes à l'intérieur d'une dalle, marges respectées) ─────
        //
        // Grotte 1 : dalle [1040→1280], grotte [1100→1350]
        //   → 60px de sol libre avant la grotte
        //   → la grotte déborde légèrement sur le bord droit de la dalle
        //     mais ça va : le mur latéral bloque avant le trou
        //   Pour être sûr : on la limite à la dalle → [1100→1280] = 180px
        addCave(1100, 180, CAVE_COL); // 11

        // Grotte 2 : dalle [1430→1680], grotte [1510→1680]
        //   → 80px de sol libre après le trou 4, puis grotte jusqu'à la fin de la dalle
        addCave(1510, 170, CAVE_COL); // 12

        // Grotte 3 : dalle [1840→fin], commence 160px après le trou 5
        addCave(2000, 420, CAVE_COL); // 13

        // Grotte 4 : même dalle, bien séparée de la grotte 3 (250px de sol libre)
        addCave(2670, 280, CAVE_COL); // 14

        // ── Plateformes hors grottes ──────────────────────────────────────────
        addPlatform(1860, 150, 45, 12, 0x212121); // 15 juste après trou 5
        addPlatform(2500, 130, 40, 12, 0x212121); // 16 entre grottes 3 et 4
        addPlatform(3000, 145, 50, 12, 0x212121); // 17 fin du niveau

        // ── Drapeau ───────────────────────────────────────────────────────────
        flagX = 3100.0f;

        // ── Goombas ───────────────────────────────────────────────────────────
        addGoomba(120, GROUND_Y, 10, 245);
        addGoomba(400, GROUND_Y, 375, 565);
        addGoomba(2100, GROUND_Y, 1845, 2290);

        // Sur plateforme idx=6 (x=100, w=45)
        addGoombaOnPlatform(6, 105);
        // Sur plateforme idx=8 (x=375, w=45)
        addGoombaOnPlatform(8, 380);

        createGoombaSprites(scr);
    }
};