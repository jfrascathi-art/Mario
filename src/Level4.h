#pragma once
#include "Level.h"

class Level4 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;
        blockCount = 0;
        pipeCount = 0;

        addGround(0, 250);     // 0
        addGround(370, 200);   // 1
        addGround(700, 200);   // 2
        addGround(1040, 240);  // 3
        addGround(1430, 250);  // 4
        addGround(1840, 1360); // 5

        addPlatform(100, 155); // 6
        addPlatform(200, 125); // 7
        addPlatform(375, 155); // 8
        addPlatform(500, 130); // 9
        addPlatform(615, 155); // 10

        addCave(1100, 180, CAVE_COL); // 11
        addCave(1510, 170, CAVE_COL); // 12
        addCave(2000, 420, CAVE_COL); // 13
        addCave(2670, 280, CAVE_COL); // 14

        addPlatform(1860, 150); // 15
        addPlatform(2500, 130); // 16
        // BUG FIX (lié au nouvel escalier final) : cette plateforme était à
        // x=3000 (donc jusqu'à x=3144), pile sur la zone du nouvel escalier
        // (x=3010-3090, voir plus bas) et du mât (x=3100). Déplacée à
        // x=2580 : juste après la plateforme 16 (x=2500-2564, 16px d'écart)
        // et bien avant la grotte 14 (x=2670, 26px d'écart) — un emplacement
        // resté inoccupé jusqu'ici.
        addPlatform(2580, 145); // 17

        // ── Tubes et escalier ────────────────────────────────────────────────
        // Tube-obstacle dans le segment de sol 3, seul espace dégagé avant
        // que la grotte 11 n'occupe presque tout le reste du segment.
        addPipe(1044, 32);
        // Tube WARP, dans l'espace dégagé entre la fin de la grotte 13 et le
        // début de la plateforme 16. Descendre dedans saute par-dessus la
        // grotte 14 et atterrit juste après sa sortie.
        // BUG FIX : l'arrivée était à x=2980, qui tombe maintenant DANS le
        // nouvel escalier reculé (voir plus bas) — ramenée à x=2952, dans
        // le petit espace de 20px qui reste entre la sortie de la grotte 14
        // (x=2950) et le début de l'escalier (x=2970).
        addPipe(2428, 48, true, 2952);
        // Escalier final.
        // BUG FIX : se terminait à flagX-10, dans la zone de déclenchement
        // du drapeau (flagX-8, voir updateGame()) — reculé pour finir à
        // flagX-50.
        addStaircase(2970, 5);

        // Rangée dès le début - niveau expert, peu de blocs mais utiles
        addBlock(40, 152, 3); // mini, utile pour les grottes
        addBlock(56, 152, 1); // champignon

        // Bloc isolé (flotte au-dessus du trou 900-1040, à la manière d'un
        // "pont" comme dans le vrai Mario : on saute en plein vol pour
        // l'attraper avant de continuer sa course)
        addBlock(950, 144, 2); // fleur de feu

        // Rangée finale
        // BUG FIX (cohérent avec le même bug trouvé dans Level1/Level2/Level3) :
        // à x=1900 le premier bloc de cette rangée chevauchait la plateforme 15
        // (x=1860-1905/1924, y=150-162). Toute la rangée décalée à x=1940,
        // juste après la plateforme, avant que la grotte 13 ne commence (2000).
        addBlock(1940, 152, 0);
        addBlock(1956, 152, 2); // fleur de feu
        addBlock(1972, 152, 0);

        // L'escalier se termine maintenant à x=3050, 50px avant le mât.
        flagX = 3100.0f;

        addGoomba(150, GROUND_Y, 10, 245);
        addGoomba(430, GROUND_Y, 375, 565);
        addGoomba(2100, GROUND_Y, 1845, 2290);
        // BUG FIX : mêmes index erronés que dans Level3 (0 et 2 = segments de
        // sol, pas plateformes). startX (105 et 380) correspond en réalité
        // aux plateformes 6 et 8 (x=100 et x=375).
        addGoombaOnPlatform(6, 105);
        addGoombaOnPlatform(8, 380);

        createGoombaSprites(scr);
    }
};