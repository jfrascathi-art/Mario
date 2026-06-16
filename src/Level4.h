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

        addGround(0, 250);     // 0
        addGround(370, 200);   // 1
        addGround(700, 200);   // 2
        addGround(1040, 240);  // 3
        addGround(1430, 250);  // 4
        addGround(1840, 1360); // 5

        addPlatform(100, 155, 45, 12, 0x212121); // 6
        addPlatform(200, 125, 40, 12, 0x212121); // 7
        addPlatform(375, 155, 45, 12, 0x212121); // 8
        addPlatform(500, 130, 40, 12, 0x212121); // 9
        addPlatform(615, 155, 40, 12, 0x212121); // 10

        addCave(1100, 180, CAVE_COL); // 11
        addCave(1510, 170, CAVE_COL); // 12
        addCave(2000, 420, CAVE_COL); // 13
        addCave(2670, 280, CAVE_COL); // 14

        addPlatform(1860, 150, 45, 12, 0x212121); // 15
        addPlatform(2500, 130, 40, 12, 0x212121); // 16
        addPlatform(3000, 145, 50, 12, 0x212121); // 17

        // Rangée dès le début - niveau expert, peu de blocs mais utiles
        addBlock(40, 152, 3); // mini, utile pour les grottes
        addBlock(56, 152, 1); // champignon

        // Bloc isolé
        addBlock(950, 144, 2); // fleur de feu

        // Rangée finale
        addBlock(1900, 152, 0);
        addBlock(1916, 152, 2); // fleur de feu
        addBlock(1932, 152, 0);

        flagX = 3100.0f;

        addGoomba(150, GROUND_Y, 10, 245);
        addGoomba(430, GROUND_Y, 375, 565);
        addGoomba(2100, GROUND_Y, 1845, 2290);
        addGoombaOnPlatform(0, 105);
        addGoombaOnPlatform(2, 380);

        createGoombaSprites(scr);
    }
};