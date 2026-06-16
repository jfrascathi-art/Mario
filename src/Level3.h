#pragma once
#include "Level.h"

class Level3 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;
        blockCount = 0;

        addGround(0, 300);     // 0
        addGround(420, 260);   // 1
        addGround(810, 240);   // 2
        addGround(1190, 260);  // 3
        addGround(1600, 1500); // 4

        addPlatform(130, 155, 55, 12, 0x4E342E);  // 5
        addPlatform(240, 125, 50, 12, 0x4E342E);  // 6
        addPlatform(435, 155, 55, 12, 0x4E342E);  // 7
        addPlatform(580, 130, 50, 12, 0x4E342E);  // 8
        addPlatform(700, 155, 50, 12, 0x4E342E);  // 9
        addPlatform(870, 130, 50, 12, 0x4E342E);  // 10
        addPlatform(1010, 155, 50, 12, 0x4E342E); // 11

        addCave(1300, 260, CAVE_COL); // 12
        addCave(1750, 350, CAVE_COL); // 13
        addCave(2400, 300, CAVE_COL); // 14

        addPlatform(1620, 145, 50, 12, 0x4E342E); // 15
        addPlatform(2200, 130, 50, 12, 0x4E342E); // 16
        addPlatform(2750, 140, 55, 12, 0x4E342E); // 17
        addPlatform(2900, 120, 50, 12, 0x4E342E); // 18

        // Rangée dès le début
        addBlock(60, 152, 1); // champignon
        addBlock(76, 152, 0);
        addBlock(92, 152, 3); // mini, utile sous les grottes

        // Bloc isolé avant 2ème trou
        addBlock(660, 144, 2); // fleur de feu

        // Rangée entre grottes 1 et 2
        addBlock(1660, 144, 0);
        addBlock(1676, 144, 1); // champignon
        addBlock(1692, 144, 0);

        // Bloc isolé
        addBlock(2550, 136, 2);

        flagX = 2980.0f;

        addGoomba(180, GROUND_Y, 10, 295);
        addGoomba(480, GROUND_Y, 425, 665);
        addGoomba(2050, GROUND_Y, 1905, 2290);
        addGoombaOnPlatform(0, 135);
        addGoombaOnPlatform(2, 440);

        createGoombaSprites(scr);
    }
};