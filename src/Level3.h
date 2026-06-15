#pragma once
#include "Level.h"

// ─────────────────────────────────────────────────────────────────────────────
// Level3.h  —  Niveau 3 : Difficile
//
// Objectif pédagogique : combiner sauts ET accroupissements sous pression.
//   • 4 trous (120, 130, 140, 150px)
//   • 3 grottes (260px, 350px, 300px) — certaines après un trou
//   • Peu de plateformes d'aide
//   • Goombas à vitesse ×2.2 (appliqué dans updateGame)
//   • Drapeau à x=2980
//
// Structure du sol :
//   [0→300] trou 120px [420→680] trou 130px [810→1050]
//   trou 140px [1190→1450] trou 150px [1600→fin]
//   Grotte 1 : x=1300..1560  (260px)
//   Grotte 2 : x=1750..2100  (350px)
//   Grotte 3 : x=2400..2700  (300px)
// ─────────────────────────────────────────────────────────────────────────────

class Level3 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;

        // ── Sol ───────────────────────────────────────────────────────────────
        addGround(0, 300);     // 0
        addGround(420, 260);   // 1  (trou 1 = 120px)
        addGround(810, 240);   // 2  (trou 2 = 130px)
        addGround(1190, 260);  // 3  (trou 3 = 140px)
        addGround(1600, 1500); // 4  (trou 4 = 150px)

        // ── Plateformes flottantes (peu d'aide, couleur sombre) ───────────────
        addPlatform(130, 155, 55, 12, 0x4E342E);  // 5
        addPlatform(240, 125, 50, 12, 0x4E342E);  // 6
        addPlatform(435, 155, 55, 12, 0x4E342E);  // 7  aide trou 1
        addPlatform(580, 130, 50, 12, 0x4E342E);  // 8
        addPlatform(700, 155, 50, 12, 0x4E342E);  // 9  aide trou 2
        addPlatform(870, 130, 50, 12, 0x4E342E);  // 10
        addPlatform(1010, 155, 50, 12, 0x4E342E); // 11 aide trou 3

        // ── Grottes ───────────────────────────────────────────────────────────
        addCave(1300, 260, CAVE_COL); // 12 Grotte 1
        addCave(1750, 350, CAVE_COL); // 13 Grotte 2 (plus longue)
        addCave(2400, 300, CAVE_COL); // 14 Grotte 3

        // ── Plateformes après les grottes ─────────────────────────────────────
        addPlatform(1620, 145, 50, 12, 0x4E342E); // 15
        addPlatform(2200, 130, 50, 12, 0x4E342E); // 16
        addPlatform(2750, 140, 55, 12, 0x4E342E); // 17
        addPlatform(2900, 120, 50, 12, 0x4E342E); // 18

        // ── Drapeau ───────────────────────────────────────────────────────────
        flagX = 2980.0f;

        // ── Goombas ───────────────────────────────────────────────────────────
        addGoomba(150, GROUND_Y, 10, 295);
        addGoomba(450, GROUND_Y, 425, 665);
        addGoomba(2050, GROUND_Y, 1905, 2290);

        // Sur plateforme idx=5  (x=130, w=55)
        addGoombaOnPlatform(5, 135);
        // Sur plateforme idx=7  (x=435, w=55)
        addGoombaOnPlatform(7, 440);

        createGoombaSprites(scr);
    }
};
