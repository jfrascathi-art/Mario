#pragma once
#include "Level.h"

// ─────────────────────────────────────────────────────────────────────────────
// Level2.h  —  Niveau 2 : S'accroupir
//
// Objectif pédagogique : apprendre la mécanique d'accroupissement.
//   • Introduction des GROTTES — blocs qui descendent du haut de l'écran
//   • Pourquoi 22px d'espace libre ?
//       Joueur DEBOUT   : hauteur effective PH = 26px  → 26 > 22 → bloqué
//       Joueur ACCROUPI : hauteur effective ~22px        → 22 ≤ 22 → passe
//   • 2 trous + 2 grottes obligatoires
//   • Goombas à vitesse ×1.5 (appliqué dans updateGame)
//   • Drapeau à x=2950
//
// Structure du sol :
//   [0→400]  trou 110px  [510→880]  trou 120px  [990→fin]
//   Grotte 1 : x=1100..1350  (250px)
//   Grotte 2 : x=1700..2000  (300px)
// ─────────────────────────────────────────────────────────────────────────────

class Level2 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;

        // ── Sol ───────────────────────────────────────────────────────────────
        addGround(0, 400);    // 0
        addGround(510, 370);  // 1  (trou 1 = 110px)
        addGround(990, 2110); // 2  (trou 2 = 120px, couvre x=990→3100)

        // ── Plateformes flottantes (avant les grottes) ────────────────────────
        addPlatform(150, 158, 70, 12, 0x8D6E63); // 3
        addPlatform(280, 130, 60, 12, 0x8D6E63); // 4
        addPlatform(560, 155, 70, 12, 0x8D6E63); // 5  aide trou 1
        addPlatform(700, 130, 60, 12, 0x8D6E63); // 6
        addPlatform(840, 155, 60, 12, 0x8D6E63); // 7  aide trou 2

        // ── Grottes ───────────────────────────────────────────────────────────
        // addCave(x, largeur, couleur)
        // → crée un bloc y=0 → y=CAVE_H (=178px), isCave=true
        addCave(1100, 250, CAVE_COL); // 8  Grotte 1 (250px)
        addCave(1700, 300, CAVE_COL); // 9  Grotte 2 (300px)

        // ── Plateformes après les grottes ─────────────────────────────────────
        addPlatform(1400, 150, 70, 12, 0x8D6E63); // 10
        addPlatform(1550, 130, 60, 12, 0x8D6E63); // 11
        addPlatform(2100, 155, 80, 12, 0x8D6E63); // 12
        addPlatform(2400, 130, 60, 12, 0x8D6E63); // 13
        addPlatform(2700, 150, 70, 12, 0x8D6E63); // 14

        // ── Drapeau ───────────────────────────────────────────────────────────
        flagX = 2950.0f;

        // ── Goombas ───────────────────────────────────────────────────────────
        addGoomba(200, GROUND_Y, 10, 395);
        addGoomba(620, GROUND_Y, 515, 875);
        addGoomba(1600, GROUND_Y, 1455, 1690);

        // Sur plateforme idx=5 (x=560, w=70)
        addGoombaOnPlatform(5, 565);
        // Sur plateforme idx=6 (x=700, w=60)
        addGoombaOnPlatform(6, 705);

        createGoombaSprites(scr);
    }
};
