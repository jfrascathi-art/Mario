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

        addGround(0, 400);    // 0
        addGround(510, 370);  // 1
        addGround(990, 2110); // 2

        addPlatform(150, 158, 70, 12, 0x8D6E63); // 3
        addPlatform(280, 130, 60, 12, 0x8D6E63); // 4
        addPlatform(560, 155, 70, 12, 0x8D6E63); // 5
        addPlatform(700, 130, 60, 12, 0x8D6E63); // 6
        addPlatform(840, 155, 60, 12, 0x8D6E63); // 7

        addCave(1100, 250, CAVE_COL); // 8
        addCave(1700, 300, CAVE_COL); // 9

        addPlatform(1400, 150, 70, 12, 0x8D6E63); // 10
        addPlatform(1550, 130, 60, 12, 0x8D6E63); // 11
        addPlatform(2100, 155, 80, 12, 0x8D6E63); // 12
        addPlatform(2400, 130, 60, 12, 0x8D6E63); // 13
        addPlatform(2700, 150, 70, 12, 0x8D6E63); // 14

        // Rangée avant 1ère grotte
        addBlock(60, 152, 0);
        addBlock(76, 152, 1); // champignon
        addBlock(92, 152, 0);

        // Bloc isolé entre les deux grottes
        addBlock(1420, 144, 2); // fleur de feu

        // Rangée après grotte 2
        addBlock(2050, 152, 0);
        addBlock(2066, 152, 3); // mini
        addBlock(2082, 152, 0);

        flagX = 2950.0f;

        addGoomba(250, GROUND_Y, 10, 395);
        addGoomba(650, GROUND_Y, 515, 875);
        addGoomba(1600, GROUND_Y, 1455, 1690);
        addGoombaOnPlatform(4, 565);
        addGoombaOnPlatform(5, 705);

        createGoombaSprites(scr);
    }
};