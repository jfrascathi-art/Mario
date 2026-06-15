#pragma once
#include "Level.h"

// ─────────────────────────────────────────────────────────────────────────────
// Level1.h  —  Niveau 1 : Sauter
//
// Objectif pédagogique : maîtriser les sauts sur des trous plus larges.
//   • 4 trous (120, 130, 140, 150px) — plus larges qu'au niveau 0
//   • Sol plus court entre les trous (le joueur est plus souvent en l'air)
//   • Plateformes d'aide positionnées au-dessus des trous
//   • 5 Goombas (3 au sol, 2 sur plateformes)
//   • Goombas à vitesse normale (×1.0)
//   • Drapeau à x=2900
//
// Structure du sol :
//   [0→350]  trou 120px  [470→780]  trou 130px  [910→1180]
//   trou 140px  [1320→1600]  trou 150px  [1750→fin]
// ─────────────────────────────────────────────────────────────────────────────

class Level1 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;

        // ── Sol ───────────────────────────────────────────────────────────────
        addGround(0, 350);     // 0
        addGround(470, 310);   // 1  (trou 1 = 120px)
        addGround(910, 270);   // 2  (trou 2 = 130px)
        addGround(1320, 280);  // 3  (trou 3 = 140px)
        addGround(1750, 1250); // 4  (trou 4 = 150px)

        // ── Plateformes flottantes ────────────────────────────────────────────
        // Couleur 0x5D4037 (marron moyen)
        // Certaines sont placées juste au-dessus des trous pour aider.
        addPlatform(160, 158, 65, 12, 0x5D4037);  // 5
        addPlatform(280, 130, 55, 12, 0x5D4037);  // 6
        addPlatform(510, 155, 70, 12, 0x5D4037);  // 7  aide trou 1
        addPlatform(700, 130, 60, 12, 0x5D4037);  // 8
        addPlatform(820, 155, 60, 12, 0x5D4037);  // 9  aide trou 2
        addPlatform(1000, 130, 60, 12, 0x5D4037); // 10
        addPlatform(1100, 155, 60, 12, 0x5D4037); // 11 aide trou 3
        addPlatform(1400, 130, 55, 12, 0x5D4037); // 12
        addPlatform(1550, 155, 60, 12, 0x5D4037); // 13 aide trou 4
        addPlatform(1900, 140, 70, 12, 0x5D4037); // 14
        addPlatform(2200, 125, 60, 12, 0x5D4037); // 15
        addPlatform(2500, 150, 80, 12, 0x5D4037); // 16

        // ── Drapeau ───────────────────────────────────────────────────────────
        flagX = 2900.0f;

        // ── Goombas ───────────────────────────────────────────────────────────
        addGoomba(180, GROUND_Y, 10, 345);
        addGoomba(600, GROUND_Y, 475, 775);
        addGoomba(1850, GROUND_Y, 1755, 2090);

        // Sur plateforme idx=9  (x=820, w=60)
        addGoombaOnPlatform(9, 825);
        // Sur plateforme idx=14 (x=1900, w=70)
        addGoombaOnPlatform(14, 1905);

        createGoombaSprites(scr);
    }
};
