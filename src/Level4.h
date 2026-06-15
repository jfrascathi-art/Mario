#pragma once
#include "Level.h"

// ─────────────────────────────────────────────────────────────────────────────
// Level4.h  —  Niveau 4 : Expert
//
// Objectif pédagogique : enchaîner trous ET grottes sans temps de répit.
//   • 5 trous serrés (120→160px)
//   • 4 grottes longues (260, 240, 450, 300px)
//   • Certaines grottes arrivent IMMÉDIATEMENT après un trou
//     → le joueur doit sauter le trou puis s'accroupir dès l'atterrissage
//   • Goombas à vitesse ×2.2 (appliqué dans updateGame)
//   • Drapeau à x=3050
//
// Structure du sol :
//   [0→250] t1=120px [370→570] t2=130px [700→900] t3=140px [1040→1230]
//   t4=150px [1380→1580] t5=160px [1740→fin]
//   Grotte 1 : x=1090..1350  (juste après trou 3 — sauter puis accroupir)
//   Grotte 2 : x=1460..1700  (avant trou 5)
//   Grotte 3 : x=1900..2350  (450px — la plus longue)
//   Grotte 4 : x=2600..2900  (300px)
// ─────────────────────────────────────────────────────────────────────────────

class Level4 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;

        // ── Sol ───────────────────────────────────────────────────────────────
        addGround(0, 250);     // 0
        addGround(370, 200);   // 1  (trou 1 = 120px)
        addGround(700, 200);   // 2  (trou 2 = 130px)
        addGround(1040, 190);  // 3  (trou 3 = 140px)
        addGround(1380, 200);  // 4  (trou 4 = 150px)
        addGround(1740, 1460); // 5  (trou 5 = 160px)

        // ── Plateformes flottantes (très réduites) ────────────────────────────
        addPlatform(100, 155, 45, 12, 0x212121); // 6
        addPlatform(200, 125, 40, 12, 0x212121); // 7
        addPlatform(375, 155, 45, 12, 0x212121); // 8  aide trou 1
        addPlatform(500, 130, 40, 12, 0x212121); // 9
        addPlatform(615, 155, 40, 12, 0x212121); // 10 aide trou 2

        // ── Grottes ───────────────────────────────────────────────────────────
        // Grotte 1 (260px) : juste après le trou 3 → atterrissage + accroupissement immédiat
        addCave(1090, 260, CAVE_COL); // 11

        // Grotte 2 (240px) : avant le trou 5
        addCave(1460, 240, CAVE_COL); // 12

        // Grotte 3 (450px) : la plus longue du jeu
        addCave(1900, 450, CAVE_COL); // 13

        // Grotte 4 (300px)
        addCave(2600, 300, CAVE_COL); // 14

        // ── Quelques plateformes hors grottes ─────────────────────────────────
        addPlatform(1760, 150, 45, 12, 0x212121); // 15
        addPlatform(2450, 130, 40, 12, 0x212121); // 16
        addPlatform(2980, 145, 50, 12, 0x212121); // 17

        // ── Drapeau ───────────────────────────────────────────────────────────
        flagX = 3050.0f;

        // ── Goombas ───────────────────────────────────────────────────────────
        addGoomba(120, GROUND_Y, 10, 245);
        addGoomba(400, GROUND_Y, 375, 565);
        addGoomba(2050, GROUND_Y, 1945, 2290);

        // Sur plateforme idx=6 (x=100, w=45)
        addGoombaOnPlatform(6, 105);
        // Sur plateforme idx=8 (x=375, w=45)
        addGoombaOnPlatform(8, 380);

        createGoombaSprites(scr);
    }
};
