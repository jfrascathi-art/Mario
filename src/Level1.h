#pragma once
#include "Level.h"

class Level1 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;
        blockCount = 0;

        addGround(0, 350);     // 0
        addGround(470, 310);   // 1
        addGround(910, 270);   // 2
        addGround(1320, 280);  // 3
        addGround(1750, 1250); // 4

        addPlatform(160, 158);  // 5
        addPlatform(280, 130);  // 6
        addPlatform(510, 155);  // 7
        addPlatform(700, 130);  // 8
        addPlatform(820, 155);  // 9
        addPlatform(1000, 130); // 10
        addPlatform(1100, 155); // 11
        addPlatform(1400, 130); // 12
        addPlatform(1550, 155); // 13
        addPlatform(1900, 140); // 14
        addPlatform(2200, 125); // 15
        addPlatform(2500, 150); // 16

        // Rangée 1 (x=80..128) : brique - champignon - brique - brique
        addBlock(80, 152, 0);
        addBlock(96, 152, 1);
        addBlock(112, 152, 0);
        addBlock(128, 152, 0);

        // Bloc isolé : fleur de feu
        addBlock(620, 144, 2);

        // Rangée 2 : brique - mini - brique
        // BUG FIX : à x=1150 cette rangée finissait à cheval sur la plateforme 11
        // (x=1100-1160/1164) → le joueur ne pouvait jamais sauter assez haut
        // pour atteindre le dessous du bloc, bloqué par le dessous de la
        // plateforme avant. Décalée à x=1340 (zone dégagée avant la plateforme 12).
        addBlock(1340, 152, 0);
        addBlock(1356, 152, 3);
        addBlock(1372, 152, 0);

        // Bloc isolé : champignon
        addBlock(2050, 144, 1);

        flagX = 2900.0f;

        addGoomba(180, GROUND_Y, 10, 345);
        addGoomba(600, GROUND_Y, 475, 775);
        addGoomba(1850, GROUND_Y, 1755, 2090);
        addGoombaOnPlatform(9, 825);
        addGoombaOnPlatform(14, 1905);

        createGoombaSprites(scr);
    }
};