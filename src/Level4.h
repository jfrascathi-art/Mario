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

        // BUG FIX : cette grotte (largeur 180) finissait à x=1280, exactement
        // au bord du trou suivant (1280-1430) — aucune marge pour se relever
        // et sauter, même souci que la grotte 12 de Level3. Largeur réduite
        // à 148 (finit à x=1248) : 32px de sol dégagé avant le trou.
        addCave(1100, 148, CAVE_COL); // 11
        // BUG FIX : même souci, cette grotte (largeur 170) finissait à
        // x=1680, pile au bord du trou suivant (1680-1840). Largeur réduite
        // à 138 (finit à x=1648) : 32px de marge avant le trou.
        addCave(1510, 138, CAVE_COL); // 12
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
        // BUG FIX #2 : 16px d'écart avec la plateforme 16, c'est trop peu —
        // la largeur du joueur (PLAYER_W=16px dans main.cpp) tient exactement
        // dans cet espace : le saut entre les deux n'a presque rien à
        // "sauter", ça ressemble à un pas plutôt qu'à un vrai saut voulu.
        // Décalée à x=2596 : 32px d'écart avec la plateforme 16 (un saut
        // significatif, comme partout ailleurs dans les niveaux), et il
        // reste encore 10px avant la grotte 14 (x=2670) — sans risque
        // puisqu'une grotte n'est qu'un plafond bas, pas un trou : nul
        // besoin de marge "saut" pour y entrer, contrairement à un trou.
        addPlatform(2596, 145); // 17

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

        // ── Arène du boss final ───────────────────────────────────────────────
        // Inspirée du pont de Bowser dans le vrai Super Mario Bros (1985) : un
        // nouveau segment de sol séparé du segment 5 par un trou, sur lequel
        // patrouille le boss avant le drapeau final.
        //
        // CALCUL DU TROU (x=3200 à x=3248, soit 48px) : même leçon que les BUG
        // FIX des grottes 11/12/13 plus haut dans ce fichier — ne JAMAIS coller
        // un bord de trou à la limite exacte de la portée de saut du joueur, et
        // rester dans une plage confortable. Portée maximale d'un saut (mêmes
        // constantes que main.cpp) :
        //   JUMP_VELOCITY = -8 px/frame, GRAVITY = 0.5 px/frame²
        //   temps jusqu'au sommet du saut = 8 / 0.5 = 16 frames
        //   temps total en l'air (montée + descente symétriques) ≈ 32 frames
        //   distance horizontale = 32 frames × WALK_SPEED (2 px/frame) = 64 px
        // 48px reste donc confortablement dans cette portée de 64px (25% de
        // marge), au lieu de coller exactement à la limite comme le faisaient
        // les grottes avant leur correction.
        addGround(3248, 452); // 27 — le "pont" du boss, de x=3248 à x=3700

        // Une fleur de feu juste avant le trou : dernière chance d'arriver armé
        // au combat pour qui n'a pas encore de power-up. Pas obligatoire : on
        // peut foncer sans s'arrêter, tenter le combat à mains nues (coups de
        // tête sautés), ou même éviter complètement le boss en courant et
        // sautant par-dessous lui jusqu'au drapeau, comme dans le vrai jeu.
        addBlock(3150, 152, 2); // fleur de feu

        // Boss : patrouille entre x=3290 et x=3630, donc toujours à au moins
        // 42px du bord du trou (x=3248) et avec de la marge avant le drapeau
        // final (x=3650, voir plus bas) — jamais collé à un bord, même
        // raisonnement que pour les grottes/trous plus haut. Démarre côté
        // drapeau (x=3580) pour qu'on le voie apparaître dès l'arrivée sur le
        // pont, sans bloquer le passage : on peut toujours courir dessous lui.
        addBoss(3580, 3290, 3630);

        // Drapeau final déplacé sur le pont, après la zone de patrouille du
        // boss : patrolRight(3630) + BOSS_W(28) = 3658 au maximum, donc même la
        // zone de déclenchement du drapeau (flagX-8 à flagX+FLAG_POLE_W+8, soit
        // 3642 à 3664) garde une frontière nette avec le corps du boss. Avant
        // l'ajout du boss, le drapeau était à x=3100, juste après l'escalier ;
        // désormais cet escalier ne fait plus que mener à la traversée du pont,
        // plus directement à la victoire.
        flagX = 3650.0f;

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

        addGoomba(150, GROUND_Y, 10, 245);
        addGoomba(430, GROUND_Y, 375, 565);
        addGoomba(2100, GROUND_Y, 1845, 2290);
        // BUG FIX : mêmes index erronés que dans Level3 (0 et 2 = segments de
        // sol, pas plateformes). startX (105 et 380) correspond en réalité
        // aux plateformes 6 et 8 (x=100 et x=375).
        addGoombaOnPlatform(6, 105);
        addGoombaOnPlatform(8, 380);

        createGoombaSprites(scr);
        createBossSprites(scr);
    }
};