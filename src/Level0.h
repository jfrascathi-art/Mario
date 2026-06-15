#pragma once
#include "Level.h"

// ─────────────────────────────────────────────────────────────────────────────
// Level0.h  —  Niveau 0 : Découverte
//
// Objectif pédagogique : apprendre à avancer et sauter.
//   • 2 trous (110px et 120px) — franchissables facilement
//   • Sol large entre les trous
//   • 8 plateformes flottantes (optionnelles)
//   • 5 Goombas (3 au sol, 2 sur plateformes)
//   • Goombas à vitesse normale (×1.0)
//   • Drapeau à x=2850
//
// Structure du sol :
//   [0 → 500]  trou 110px  [610 → 1100]  trou 120px  [1220 → fin]
// ─────────────────────────────────────────────────────────────────────────────

class Level0 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;

        // ── Sol (dalles avec trous) ───────────────────────────────────────────
        //   idx 0 : dalle avant le 1er trou
        addGround(0, 500); // 0
        //   trou 1 : 610 - 500 = 110px
        addGround(610, 490); // 1
        //   trou 2 : 1220 - 1100 = 120px
        addGround(1220, 1900); // 2   (jusqu'à x=3120)

        // ── Plateformes flottantes (sol à y=200, plateformes vers y=130~165) ─
        //   Toutes ont h=12 (fines) et couleur 0x795548 (marron clair).
        //   y plus petit = plus haut sur l'écran.
        addPlatform(200, 165, 70, 12, 0x795548);  // 3
        addPlatform(350, 140, 60, 12, 0x795548);  // 4
        addPlatform(700, 160, 80, 12, 0x795548);  // 5
        addPlatform(900, 135, 60, 12, 0x795548);  // 6
        addPlatform(1350, 155, 70, 12, 0x795548); // 7
        addPlatform(1600, 130, 60, 12, 0x795548); // 8
        addPlatform(1900, 155, 80, 12, 0x795548); // 9
        addPlatform(2200, 140, 60, 12, 0x795548); // 10

        // ── Drapeau ───────────────────────────────────────────────────────────
        flagX = 2850.0f;

        // ── Goombas ───────────────────────────────────────────────────────────
        // 3 au sol — patrol entre les bords de leur dalle
        addGoomba(250, GROUND_Y, 10, 490);
        addGoomba(700, GROUND_Y, 615, 1090);
        addGoomba(1400, GROUND_Y, 1225, 1800);

        // 2 sur plateformes : utilise l'index de la plateforme + startX
        //   plateforme idx=4 (x=350, w=60) — Goomba commence à x=355
        addGoombaOnPlatform(4, 355);
        //   plateforme idx=5 (x=700, w=80) — Goomba commence à x=705
        addGoombaOnPlatform(5, 705);

        // Crée les sprites LVGL pour tous les Goombas
        createGoombaSprites(scr);
    }
};
