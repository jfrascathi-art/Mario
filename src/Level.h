#pragma once
#include "lvgl.h"
#include "GameTypes.h"

// ── Variables globales définies dans main.cpp ─────────────────────────────────
extern Platform platforms[MAX_PLATFORMS];
extern int platformCount;
extern Goomba goombas[MAX_GOOMBAS];
extern int goombaCount;
extern Block blocks[MAX_BLOCKS];
extern int blockCount;
extern Pipe pipes[MAX_PIPES];
extern int pipeCount;
extern float flagX;
extern float cameraX;
// Boss final : un seul boss possible par niveau (pas un tableau comme les
// Goombas), et bossActive distingue "ce niveau a un boss" de "il est mort" :
// bossActive ne redevient jamais false tant qu'on n'a pas quitté le niveau
// (même boss.alive==false après sa défaite), pour que le drapeau final sache
// encore qu'il doit vérifier le bonus de score (voir updateGame()).
extern Boss boss;
extern bool bossActive;
extern Hammer hammers[MAX_HAMMERS];
extern int hammerCount;

// ── Fonctions définies dans main.cpp ─────────────────────────────────────────
extern lv_obj_t *makeRect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color);
extern void createGoombaSprites(lv_obj_t *scr);
extern void createBossSprites(lv_obj_t *scr);

// ── Classe abstraite Level ────────────────────────────────────────────────────
class Level
{
public:
    virtual void load(lv_obj_t *scr) = 0;
    virtual ~Level() {}

protected:
    // ── Helpers sol / plateformes / grottes ───────────────────────────────────
    // Version "libre" (largeur/hauteur/couleur au choix) : gardée pour usage
    // interne et flexibilité future. solid=false : collision dessus/dessous
    // uniquement, comme une plateforme flottante classique.
    void addPlatform(float x, float y, float w, float h, uint32_t color)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, y, w, h, color, false};
    }
    // Version "uniforme" : c'est CELLE-CI que tous les Level*.h utilisent
    // pour les plateformes flottantes (taille/couleur fixées une fois pour
    // toutes dans GameTypes.h).
    void addPlatform(float x, float y)
    {
        addPlatform(x, y, PLATFORM_W, PLATFORM_H, PLATFORM_COLOR);
    }

    // addSolidPlatform : comme addPlatform, mais avec collision PLEINE (haut +
    // bas + LES DEUX côtés) — impossible à traverser latéralement. C'est le
    // même comportement que les murs de grotte ; addCave, addStaircase et
    // addPipe s'appuient tous les trois dessus, donc AUCUN nouveau code de
    // collision n'a été nécessaire dans main.cpp pour les escaliers et les
    // tubes : on réutilise exactement celui déjà écrit (et testé) pour les
    // grottes.
    void addSolidPlatform(float x, float y, float w, float h, uint32_t color)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, y, w, h, color, true};
    }

    void addCave(float x, float w, uint32_t color)
    {
        addSolidPlatform(x, (float)CAVE_Y, w, (float)CAVE_H, color);
    }
    void addGround(float x, float w, uint32_t color = GROUND_DIRT_COLOR)
    {
        if (platformCount >= MAX_PLATFORMS)
            return;
        platforms[platformCount++] = {x, GROUND_Y, w, (float)(SCREEN_H - (int)GROUND_Y), color, false};
    }

    // ── Escalier ──────────────────────────────────────────────────────────────
    // Construit un escalier ASCENDANT de `steps` marches partant du sol,
    // comme celui qui précède le drapeau dans le vrai Super Mario Bros
    // ("Hard Blocks" empilés en pyramide, à peu près à la hauteur du mât).
    //
    // POURQUOI une seule Platform par marche, et pas plusieurs blocs 16×16
    // empilés ? Parce qu'un rectangle plein de hauteur (i+1)×stepH produit
    // EXACTEMENT la même boîte de collision qu'une pile de (i+1) blocs de
    // stepH chacun (le joueur ne voit aucune différence, et on n'attaque
    // jamais une marche par en-dessous comme un bloc ?) — donc autant
    // utiliser 1 seule Platform par marche : moins de mémoire, même résultat.
    //
    // xStart : position x de la marche la PLUS BASSE (la première qu'on
    //          rencontre en avançant vers le drapeau)
    // steps  : nombre de marches (5 marches = 16,32,48,64,80px de haut)
    void addStaircase(float xStart, int steps, float stepW = 16.0f, float stepH = 16.0f,
                      uint32_t color = STAIR_BLOCK_COLOR)
    {
        for (int i = 0; i < steps; i++)
        {
            float h = (float)(i + 1) * stepH;
            addSolidPlatform(xStart + (float)i * stepW, GROUND_Y - h, stepW, h, color);
        }
    }

    // ── Tube ──────────────────────────────────────────────────────────────────
    // Ajoute un tube vert debout sur le sol : un CHAPEAU (plus large, fine
    // bande du haut) posé sur un CORPS (jusqu'au sol). Les deux sont des
    // Platform "solid" → collision déjà gérée, rien à ajouter dans
    // updateGame() pour qu'on ne puisse pas traverser le tube par le côté ou
    // marcher au travers depuis le dessus.
    //
    // x       : bord gauche du CORPS (le chapeau dépasse de PIPE_OVERHANG de
    //           chaque côté, voir GameTypes.h)
    // height  : hauteur TOTALE du tube, du sol jusqu'au sommet du chapeau.
    //           Multiple de 16 recommandé (16/32/48/64...) pour rester sur
    //           la même grille que les blocs ? et les marches d'escalier.
    // warp    : true → le joueur peut "descendre" dedans en poussant le
    //           joystick vers le BAS alors qu'il est debout sur le chapeau
    //           (voir le bloc "TUBES" dans updateGame(), main.cpp)
    // exitX   : position d'arrivée dans le MÊME niveau si warp == true.
    //           Comme le moteur ne gère qu'un seul niveau "ouvert" à la fois
    //           (pas de zone souterraine séparée), on simplifie le warp pipe
    //           du vrai jeu (Warp Zone de 1-2 par ex.) en un téléport plus
    //           loin dans le niveau courant : même esprit (un raccourci cool
    //           récompensant l'exploration), sans avoir à gérer une seconde
    //           carte.
    void addPipe(float x, float height, bool warp = false, float exitX = 0.0f)
    {
        float topY = GROUND_Y - height;
        float capX = x - PIPE_OVERHANG;
        float capW = PIPE_BODY_W + 2.0f * PIPE_OVERHANG;
        addSolidPlatform(capX, topY, capW, PIPE_CAP_H, PIPE_CAP_COLOR);
        addSolidPlatform(x, topY + PIPE_CAP_H, PIPE_BODY_W, height - PIPE_CAP_H, PIPE_BODY_COLOR);
        if (pipeCount < MAX_PIPES)
            pipes[pipeCount++] = {capX, capW, topY, warp, exitX, nullptr};
    }

    // ── Helper Goomba ─────────────────────────────────────────────────────────
    void addGoomba(float startX, float startY, float patrolLeft, float patrolRight)
    {
        if (goombaCount >= MAX_GOOMBAS)
            return;
        goombas[goombaCount++] = {
            startX, startY, -GOOMBA_SPEED,
            true, patrolLeft, patrolRight,
            nullptr, nullptr, nullptr, nullptr};
    }
    void addGoombaOnPlatform(int pidx, float startX)
    {
        if (pidx < 0 || pidx >= platformCount)
            return;
        Platform &p = platforms[pidx];
        addGoomba(startX, p.y, p.x + 2, p.x + p.w - GOOMBA_W - 2);
    }

    // ── Helper Bloc ? ─────────────────────────────────────────────────────────
    // x, y     : coin haut-gauche du bloc dans le monde
    // puType   : 1=champignon  2=fleur de feu  3=mini champignon
    void addBlock(float x, float y, int puType)
    {
        if (blockCount >= MAX_BLOCKS)
            return;
        blocks[blockCount++] = {x, y, puType, false, nullptr, nullptr};
    }

    // ── Helper Boss ───────────────────────────────────────────────────────────
    // x            : position de départ du boss (pieds), dans le monde
    // patrolLeft/Right : bornes de patrouille (même logique que addGoomba)
    // Met bossActive à true : c'est ce qui dit à updateGame()/renderGame()
    // "il y a un combat de boss sur ce niveau, active toute la logique
    // associée (marteaux, collisions, bonus de score au drapeau)". Aucun
    // autre Level*.h n'appelle cette fonction, donc bossActive reste à
    // false (sa valeur par défaut, réinitialisée à chaque niveau dans
    // showGame()/nextLevel()) pour tous les niveaux sauf celui qui l'utilise.
    void addBoss(float x, float patrolLeft, float patrolRight)
    {
        boss.x = x;
        boss.y = GROUND_Y;
        boss.velX = -BOSS_SPEED;
        boss.hp = BOSS_MAX_HP;
        boss.alive = true;
        boss.hitCooldown = 0;
        boss.throwTimer = BOSS_HAMMER_INTERVAL_MAX;
        boss.patrolLeft = patrolLeft;
        boss.patrolRight = patrolRight;
        boss.objBody = boss.objBelly = boss.objHead = boss.objEyeL = boss.objEyeR = boss.objBrow = boss.objHornL = boss.objHornR = nullptr;
        bossActive = true;
    }
};