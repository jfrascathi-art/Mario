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
        pipeCount = 0;

        addGround(0, 300);     // 0
        addGround(420, 260);   // 1
        addGround(810, 240);   // 2
        addGround(1190, 260);  // 3
        addGround(1600, 1500); // 4

        addPlatform(130, 155);  // 5
        addPlatform(240, 125);  // 6
        addPlatform(435, 155);  // 7
        addPlatform(580, 130);  // 8
        addPlatform(700, 155);  // 9
        addPlatform(870, 130);  // 10
        addPlatform(1010, 155); // 11

        // BUG FIX (photo envoyée par l'utilisateur) : cette grotte était à
        // x=1300, largeur 260 (donc jusqu'à x=1560). Le sol s'arrête à x=1450
        // (trou de 1450 à 1600) : les derniers 110px du mur de grotte
        // surplombaient donc le vide, sans aucun sol dessous. Le joueur qui
        // se baissait/rétrécissait pour passer sous la grotte tombait dans le
        // trou au lieu d'arriver sur la terre ferme. Repositionnée à x=1250.
        // BUG FIX #2 : avec une largeur de 200, la grotte finissait à x=1450,
        // c'est-à-dire EXACTEMENT là où commence le trou (aucune marge). Le
        // joueur sortait donc accroupi de la grotte à la toute dernière
        // frame avant le vide : pas une seule frame pour se relever et
        // prendre l'élan nécessaire au saut. Largeur réduite à 168 (finit à
        // x=1418) : il reste maintenant 32px de sol dégagé (la place de se
        // relever et de marcher un peu) avant le bord du trou à x=1450.
        addCave(1250, 168, CAVE_COL); // 12
        addCave(1750, 350, CAVE_COL); // 13
        addCave(2400, 300, CAVE_COL); // 14

        addPlatform(1650, 145); // 15
        addPlatform(2200, 130); // 16
        addPlatform(2750, 140); // 17
        // SUPPRIMÉE : addPlatform(2900, 120); // 18
        // Cette plateforme décorative (x=2900-3020) chevauchait pile la zone
        // du nouvel escalier final (x=2890-2970, voir plus bas). Comme elle
        // n'est référencée par aucun bloc ni Goomba, la retirer est plus
        // simple et plus sûr que la déplacer (et libère justement la zone où
        // est posé le tube WARP ci-dessous).

        // ── Tubes et escalier ────────────────────────────────────────────────
        // Tube-obstacle dans le segment de sol 2, entre les plateformes 10 et 11.
        addPipe(955, 32);
        // Tube WARP, dans l'espace dégagé entre la fin de la patrouille du
        // Goomba (x=2290) et le début de la grotte 14 (x=2400). Descendre
        // dedans saute par-dessus toute la grotte et atterrit juste après
        // sa sortie (x=2720).
        addPipe(2330, 48, true, 2720);
        // Escalier final (zone libérée par la suppression de l'ancienne
        // plateforme 18 ci-dessus).
        // BUG FIX : se terminait à flagX-10, dans la zone de déclenchement
        // du drapeau (flagX-8, voir updateGame()) — reculé pour finir à
        // flagX-50.
        addStaircase(2850, 5);

        // Rangée dès le début
        addBlock(60, 152, 1); // champignon
        addBlock(76, 152, 0);
        addBlock(92, 152, 3); // mini, utile sous les grottes

        // Bloc isolé avant 2ème trou
        addBlock(660, 144, 2); // fleur de feu

        // Rangée brique-champignon-brique, entre la 1ère grotte (repositionnée
        // ci-dessus) et la 2ème.
        // BUG FIX (photo envoyée par l'utilisateur, "deux plateformes l'une
        // dans l'autre") : à x=1660 cette rangée chevauchait la plateforme 15
        // (x=1620-1670/1684, y=145-157) : le premier bloc de la rangée était
        // en fait à moitié fondu dans la plateforme. Rangée décalée à
        // x=1195 (juste après l'atterrissage du trou précédent, avant que la
        // grotte 12 ne commence) ; plateforme 15 décalée à x=1650 (dans le
        // segment de sol qui suit le trou, avant la grotte 13).
        addBlock(1195, 144, 0);
        addBlock(1211, 144, 1); // champignon
        addBlock(1227, 144, 0);

        // BUG FIX : ce bloc isolé était à x=2550, EN PLEIN MILIEU du mur de
        // la grotte 14 (x=2400-2700, solide de y=0 à y=178) : totalement
        // enfermé dans la pierre, impossible à atteindre par le joueur quel
        // que soit le côté d'approche. Déplacé à x=2715, juste après la
        // sortie de la grotte.
        addBlock(2715, 144, 2);

        // L'escalier se termine maintenant à x=2930, 50px avant le mât.
        flagX = 2980.0f;

        addGoomba(180, GROUND_Y, 10, 295);
        addGoomba(480, GROUND_Y, 425, 665);
        addGoomba(2050, GROUND_Y, 1905, 2290);
        // BUG FIX : ces deux appels référençaient les index 0 et 2, qui sont
        // des segments de SOL (addGround), pas des plateformes flottantes.
        // Le code fonctionnait quand même (un Goomba patrouillait sur tout le
        // segment de sol), mais créait un second Goomba redondant qui se
        // chevauchait presque entièrement avec un Goomba déjà posé au sol
        // juste au-dessus. Le startX donné (135 et 440) correspond en réalité
        // aux plateformes 5 et 7 (x=130 et x=435) : indices corrigés pour que
        // ces Goombas patrouillent bien sur leur plateforme flottante prévue.
        addGoombaOnPlatform(5, 135);
        addGoombaOnPlatform(7, 440);

        createGoombaSprites(scr);
    }
};