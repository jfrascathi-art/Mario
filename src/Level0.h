#pragma once
#include "Level.h"

// Niveau 0 : Découverte
// Blocs placés à y=152 (48px au-dessus du sol à y=200) : le joueur les
// atteint dès le début de son saut (le saut monte ~85px), donc c'est
// confortable même pour un premier niveau.
// Rangées de 3-4 blocs comme dans le vrai Mario : briques (type 0,
// décoratives) encadrant un bloc "?" (type 1/2/3) contenant le power-up.

class Level0 : public Level
{
public:
    void load(lv_obj_t *scr) override
    {
        platformCount = 0;
        goombaCount = 0;
        blockCount = 0;
        pipeCount = 0;

        // ── Sol ───────────────────────────────────────────────────────────────
        addGround(0, 500);     // 0
        addGround(610, 490);   // 1
        addGround(1220, 1900); // 2

        // ── Plateformes flottantes ────────────────────────────────────────────
        addPlatform(200, 165);  // 3
        addPlatform(350, 140);  // 4
        addPlatform(700, 160);  // 5
        addPlatform(900, 135);  // 6
        addPlatform(1350, 155); // 7
        addPlatform(1600, 130); // 8
        addPlatform(1900, 155); // 9
        addPlatform(2200, 140); // 10

        // ── Tubes et escalier ────────────────────────────────────────────────
        // Tube-obstacle (juste un obstacle à enjamber, pas de symbole "▼") :
        // posé dans le grand espace dégagé entre la plateforme 9 (1900-1964)
        // et la plateforme 10 (2200-2264).
        addPipe(2050, 32);
        // Tube WARP : posé après la plateforme 10, avant l'escalier final.
        // Descendre dedans (joystick bas) téléporte 350px plus loin, juste
        // devant l'escalier — un petit raccourci comme dans le vrai jeu.
        addPipe(2350, 48, true, 2700);
        // Escalier final façon "Hard Blocks" de la fin du niveau 1-1 : 5
        // marches de 16px, de 16 à 80px de haut.
        // BUG FIX : se terminait à flagX-10, soit DANS la zone de
        // déclenchement du drapeau qui commence dès flagX-8 (voir
        // updateGame()) — il suffisait quasiment de poser le pied sur la
        // dernière marche pour finir le niveau, sans même la place de
        // sauter. Reculé pour finir à flagX-50 : il reste maintenant 42px
        // de marche réelle avant que le drapeau ne se déclenche.
        addStaircase(2720, 5);

        // ── Blocs style Mario ────────────────────────────────────────────────
        // Rangée 1 (x=80..128) : brique - champignon - brique - brique
        addBlock(80, 152, 0);
        addBlock(96, 152, 1);
        addBlock(112, 152, 0);
        addBlock(128, 152, 0);

        // Bloc "?" isolé un peu plus haut (x=650) : fleur de feu, juste avant
        // le premier trou pour aider à le franchir
        addBlock(650, 136, 2);

        // Rangée 2 après le trou (x=820..868) : brique - mini - brique
        addBlock(820, 152, 0);
        addBlock(836, 152, 3);
        addBlock(852, 152, 0);

        // Bloc "?" isolé (x=1500) : champignon
        addBlock(1500, 144, 1);

        // ── Drapeau ───────────────────────────────────────────────────────────
        // L'escalier se termine maintenant à x=2800, laissant 50px avant
        // le mât (au lieu de 10px avant le correctif ci-dessus).
        flagX = 2850.0f;

        // ── Goombas ───────────────────────────────────────────────────────────
        addGoomba(250, GROUND_Y, 10, 490);
        addGoomba(700, GROUND_Y, 615, 1090);
        addGoomba(1400, GROUND_Y, 1225, 1800);
        addGoombaOnPlatform(4, 355);
        addGoombaOnPlatform(5, 705);

        createGoombaSprites(scr);
    }
};