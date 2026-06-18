#pragma once
#include "lvgl.h"

// ─────────────────────────────────────────────────────────────────────────────
// GameTypes.h  —  Types et constantes partagés entre main.cpp et Level*.h
// ─────────────────────────────────────────────────────────────────────────────

// ── Constantes monde ──────────────────────────────────────────────────────────
#define GROUND_Y 200.0f
#define SCREEN_W 480
#define SCREEN_H 272
#define GROUND_VISUAL_Y 200
// WORLD_W : largeur totale du monde, en pixels. Agrandie de 3200 à 3760 pour
// loger l'arène du boss final ajoutée à la fin du niveau 5 (Level4.h) :
// pont + zone de patrouille + drapeau final, voir le détail des calculs
// dans Level4.h. Les niveaux 0 à 3 n'utilisent jamais plus de x≈3000, donc
// cet agrandissement ne change rien pour eux (juste un peu plus de marge
// avant la limite invisible où le joueur est bloqué, cf. updateGame()).
#define WORLD_W 3760

#define CAVE_Y 0
#define CAVE_H ((int)(GROUND_Y - 22))

#define CAVE_COL 0x4E342E
#define CAVE_COL3 0x3E2723
#define CAVE_COL4 0x212121

// ── Plateformes flottantes : taille et couleur UNIQUES pour tous les niveaux ──
#define PLATFORM_W 64.0f
#define PLATFORM_H 12.0f
#define PLATFORM_COLOR 0xA0703A

// ── Sol "vrai Mario" : bande d'herbe verte sur de la terre brune ──────────────
#define GROUND_DIRT_COLOR 0x8B5A2B
#define GROUND_GRASS_COLOR 0x4CAF50
#define GRASS_CAP_H 6

#define MAX_PLATFORMS 32
#define MAX_GOOMBAS 12
#define GOOMBA_W 14.0f
#define GOOMBA_H 16.0f
#define GOOMBA_SPEED 1.2f

// ── Power-ups ─────────────────────────────────────────────────────────────────
#define MAX_BLOCKS 16
#define MAX_ITEMS 4
#define MAX_FIREBALLS 4

// ── Durée de TOUS les power-ups, en frames ────────────────────────────────────
// Le jeu tourne dans myTask() avec vTaskDelayUntil(..., pdMS_TO_TICKS(40)) :
// une frame toutes les 40 ms → 1000/40 = 25 frames par seconde (25 fps).
// Pour obtenir 5 secondes de power-up, il faut donc :
//     durée_frames = durée_secondes × fps = 5 × 25 = 125
// Avant, seule la fleur de feu avait un timer (FIRE_DURATION). Le champignon
// et le mini-champignon ne s'arrêtaient JAMAIS tout seuls (uniquement en
// touchant un Goomba). POWERUP_DURATION est maintenant utilisé pour les
// TROIS power-ups, dans powerUpTimer (voir main.cpp).
#define POWERUP_DURATION 125

// ── Tubes (pipes) ─────────────────────────────────────────────────────────────
// Un tube est dessiné avec deux rectangles pleins :
//   - le CHAPEAU (cap)  : plus large que le corps, fine bande en haut (8px)
//   - le CORPS   (body) : la partie qui descend jusqu'au sol
// PIPE_OVERHANG = débord du chapeau de chaque côté par rapport au corps
// (juste un effet de style avec nos rectangles plats, pour qu'on reconnaisse
// un tube au premier coup d'œil — dans le vrai jeu le "rebord" est juste un
// motif dessiné dans la même largeur de 32px, mais on n'a pas de sprite ici).
#define MAX_PIPES 8
#define PIPE_BODY_W 24.0f
#define PIPE_CAP_H 8.0f
#define PIPE_OVERHANG 4.0f
#define PIPE_BODY_COLOR 0x3C9D40
#define PIPE_CAP_COLOR 0x2D6A30

// ── Escaliers ──────────────────────────────────────────────────────────────
#define STAIR_BLOCK_COLOR 0x787878

// ── Structs plateforme et ennemi ──────────────────────────────────────────────
struct Platform
{
    float x, y, w, h;
    uint32_t color;
    // solid = collision PLEINE (dessus + dessous + les DEUX côtés), comme un
    // mur. Avant, ce champ s'appelait "isCave" car seul addCave() s'en
    // servait. Il sert maintenant aussi aux marches d'escalier et aux tubes,
    // qui ont besoin exactement du même comportement : impossible de les
    // traverser par le côté, contrairement aux plateformes flottantes
    // classiques qui ne bloquent que par le dessus/dessous.
    bool solid;
};

struct Goomba
{
    float x, y, velX;
    bool alive;
    float patrolLeft, patrolRight;
    lv_obj_t *objHat, *objBody, *objFeetL, *objFeetR;
};

// ── Bloc ? ────────────────────────────────────────────────────────────────────
struct Block
{
    float x, y;
    int powerUpType;
    bool hit;
    lv_obj_t *obj;
    lv_obj_t *mark;
};

// ── Item au sol ───────────────────────────────────────────────────────────────
struct Item
{
    float x, y, velY;
    int type;
    bool active;
    bool onGround;
    lv_obj_t *obj;
};

// ── Boule de feu ──────────────────────────────────────────────────────────────
struct FireBall
{
    float x, y, velX;
    bool active;
    lv_obj_t *obj;
};

// ── Tube ──────────────────────────────────────────────────────────────────────
// Un Pipe ne porte PAS la collision lui-même : sa collision est déjà assurée
// par les deux Platform "solid" (chapeau + corps) créées par addPipe() dans
// Level.h. Cette struct sert uniquement à repérer "le joueur est-il debout
// SUR ce tube précis" pour déclencher la descente.
struct Pipe
{
    float x, w;     // emprise horizontale du CHAPEAU (zone où on peut se tenir)
    float topY;     // altitude (y) du dessus du chapeau
    bool warp;      // true = on peut descendre dedans (joystick bas)
    float exitX;    // position d'arrivée dans le niveau si warp == true
    lv_obj_t *mark; // petite flèche "▼" au-dessus des tubes descendables (nullptr sinon)
};

// ── Boss final (fin du niveau 5) ───────────────────────────────────────────
// Inspiré du combat final de Bowser dans le vrai Super Mario Bros (1985) :
//   - il patrouille de gauche à droite sur un pont, comme un Goomba géant ;
//   - il lance régulièrement un marteau qui suit une trajectoire PARABOLIQUE
//     (même physique que le saut du joueur : vitesse verticale initiale +
//     gravité qui s'accumule frame après frame, voir updateGame()) ;
//   - 5 boules de feu OU 5 coups de tête sautés le vainquent (référence
//     directe au "Repeating fireballs: 5000 points" du jeu original) ;
//   - comme dans le vrai jeu, on peut aussi l'ÉVITER complètement en
//     courant/sautant par-dessous lui pour rejoindre le drapeau final —
//     le combattre n'est donc pas obligatoire pour finir le jeu, juste plus
//     payant en score.
// boss.x / boss.y suivent exactement la même convention que player.x/y :
// (x,y) = coin haut-gauche en x, position des PIEDS en y.
struct Boss
{
    float x, y, velX;
    int hp; // points de vie restants (0 = vaincu)
    bool alive;
    int hitCooldown; // frames d'invincibilité après un coup reçu (anti double-compte)
    int throwTimer;  // frames restantes avant le prochain lancer de marteau
    float patrolLeft, patrolRight;
    lv_obj_t *objBody, *objHead, *objEye, *objSpike;
};

#define BOSS_W 28.0f
#define BOSS_H 34.0f
#define BOSS_MAX_HP 5
#define BOSS_SPEED 0.6f
// BOSS_HAMMER_INTERVAL_MAX/MIN : le boss lance un marteau toutes les
// "throwTimer" frames, et cet intervalle RACCOURCIT au fur et à mesure
// qu'on le blesse (5 points de vie → 5 intervalles, du plus lent au plus
// rapide) pour qu'il devienne plus agressif quand il est presque vaincu,
// comme beaucoup de boss de jeu vidéo. Calcul détaillé dans Level4.h /
// updateGame() : intervalle = MAX - (MAX_HP - hp) * 8, jamais sous MIN.
#define BOSS_HAMMER_INTERVAL_MAX 70 // ~2.8s à 25 fps, à pleine vie (hp=5)
// BOSS_HAMMER_INTERVAL_MIN : simple garde-fou défensif pour le clamp dans
// updateGame(). Avec MAX_HP=5 et un pas de 8 par point de vie perdu, le
// pire cas réel (hp=1, juste avant la mort) donne 70-(5-1)*8 = 38 frames
// (~1.52s) : le clamp à 30 ne se déclenche donc jamais en pratique avec ces
// réglages précis, mais protège automatiquement si MAX_HP ou le pas de 8
// venaient à changer plus tard.
#define BOSS_HAMMER_INTERVAL_MIN 30

// ── Marteau (projectile du boss) ────────────────────────────────────────────
// Trajectoire parabolique : velY commence négatif (vers le haut, comme un
// saut) puis GRAVITY s'additionne à chaque frame jusqu'à ce qu'il retombe
// au sol — exactement le même calcul que player.velY, réutilisé pour un
// projectile au lieu d'un personnage.
#define MAX_HAMMERS 3
struct Hammer
{
    float x, y, velX, velY;
    bool active;
    lv_obj_t *obj;
};