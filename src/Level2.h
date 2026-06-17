#pragma once
#include "Level.h"

class Level2 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;
        blockCount = 0;
        pipeCount = 0;

        addGround(0, 400);    // 0
        addGround(510, 370);  // 1
        addGround(990, 2110); // 2

        addPlatform(150, 158); // 3
        addPlatform(280, 130); // 4
        addPlatform(560, 155); // 5
        addPlatform(700, 130); // 6
        addPlatform(840, 155); // 7

        addCave(1100, 250, CAVE_COL); // 8
        addCave(1700, 300, CAVE_COL); // 9

        addPlatform(1400, 150); // 10
        addPlatform(1550, 130); // 11
        addPlatform(2100, 155); // 12
        addPlatform(2400, 130); // 13
        addPlatform(2700, 150); // 14

        // ── Tubes et escalier ────────────────────────────────────────────────
        // Tube-obstacle dans le segment de sol 2, juste avant la grotte 8.
        addPipe(1020, 32);
        // Tube WARP, dans l'espace dégagé entre les plateformes 12 et 13.
        // Descendre dedans saute par-dessus la plateforme 13 et atterrit
        // juste après, avant la plateforme 14.
        addPipe(2280, 48, true, 2550);
        // Escalier final.
        // BUG FIX : se terminait à flagX-10, dans la zone de déclenchement
        // du drapeau (flagX-8, voir updateGame()) — reculé pour finir à
        // flagX-50.
        addStaircase(2820, 5);

        // Rangée avant 1ère grotte
        addBlock(60, 152, 0);
        addBlock(76, 152, 1); // champignon
        addBlock(92, 152, 0);

        // Bloc isolé entre les deux grottes
        // BUG FIX (photo envoyée par l'utilisateur) : à x=1420 ce bloc était
        // posé EN PLEIN MILIEU de la plateforme 10 (x=1400-1464, y=150-162) :
        // le joueur cognait le dessous de la plateforme avant de pouvoir
        // atteindre le bloc, qui restait donc inaccessible. Décalé à x=1500,
        // dans l'espace dégagé entre les plateformes 10 et 11.
        addBlock(1500, 144, 2); // fleur de feu

        // Rangée après grotte 2
        addBlock(2050, 152, 0);
        addBlock(2066, 152, 3); // mini
        addBlock(2082, 152, 0);

        // L'escalier se termine maintenant à x=2900, 50px avant le mât.
        flagX = 2950.0f;

        addGoomba(250, GROUND_Y, 10, 395);
        addGoomba(650, GROUND_Y, 515, 875);
        addGoomba(1600, GROUND_Y, 1455, 1690);
        // BUG FIX : ces deux appels référençaient les plateformes 4 et 5
        // (x=280 et x=560) alors que le startX donné (565 et 705) correspond
        // en réalité aux plateformes 5 et 6 (x=560 et x=700). Résultat : le
        // Goomba se créait hors des bornes de patrouille calculées et se
        // "téléportait" dès la première frame pour se replacer sur la bonne
        // plateforme. Indices corrigés.
        addGoombaOnPlatform(5, 565);
        addGoombaOnPlatform(6, 705);

        createGoombaSprites(scr);
    }
};