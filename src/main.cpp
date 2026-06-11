#include "lvgl.h"
#define JOY_X A0
#define JOY_Y A1
#define BUTTON_JUMP D6
#define BUTTON_DOWN D7
#define INPUTS_H
#define JOY_CENTER 512
#define JOY_DEADZONE 20
#define JOY_MAX 1023
enum AppScreen
{
  SCREEN_JOYSTICK_TEST,
  SCREEN_MENU,
  SCREEN_GAME
};
AppScreen currentScreen = SCREEN_JOYSTICK_TEST;
static void event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    LV_LOG_USER("Clicked");
  }
  else if (code == LV_EVENT_VALUE_CHANGED)
  {
    LV_LOG_USER("Toggled");
  }
}
void testLvgl()
{
  lv_obj_t *label;
  lv_obj_t *btn1 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);
  lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);
  label = lv_label_create(btn1);
  lv_label_set_text(label, "Button");
  lv_obj_center(label);
  lv_obj_t *btn2 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
  lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_height(btn2, LV_SIZE_CONTENT);
  label = lv_label_create(btn2);
  lv_label_set_text(label, "Toggle");
  lv_obj_center(label);
}
#ifdef ARDUINO
#include "lvglDrivers.h"
struct InputState
{
  int joyX;
  int joyY;
  bool buttonJump;
  bool buttonDown;
  bool isLeft() const { return joyX < JOY_CENTER - JOY_DEADZONE; }
  bool isRight() const { return joyX > JOY_CENTER + JOY_DEADZONE; }
  bool isUp() const { return joyY < JOY_CENTER - JOY_DEADZONE; }
  bool isDown() const { return joyY > JOY_CENTER + JOY_DEADZONE; }
  float normalizedX() const
  {
    const float MAX_RANGE = 65.0f;
    if (isLeft())
      return -(float)(JOY_CENTER - JOY_DEADZONE - joyX) / MAX_RANGE;
    if (isRight())
      return (float)(joyX - JOY_CENTER - JOY_DEADZONE) / MAX_RANGE;
    return 0.0f;
  }
  float normalizedY() const
  {
    if (isUp())
      return -(float)(JOY_CENTER - JOY_DEADZONE - joyY) / (float)(JOY_CENTER - JOY_DEADZONE);
    if (isDown())
      return (float)(joyY - JOY_CENTER - JOY_DEADZONE) / (float)(JOY_MAX - JOY_CENTER - JOY_DEADZONE);
    return 0.0f;
  }
};
void initInputs();
void updateInputs();
InputState inputs;
void initInputs()
{
  pinMode(BUTTON_JUMP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
}
void updateInputs()
{
  inputs.joyX = analogRead(JOY_X);
  inputs.joyY = 1023 - analogRead(JOY_Y); // sinon haut et bas inversés
  inputs.buttonJump = !digitalRead(BUTTON_JUMP);
  inputs.buttonDown = !digitalRead(BUTTON_DOWN);
}
InputState getInputs() { return inputs; }
InputState getInputs();

// ═══ variables globales écran de test ═══
static lv_obj_t *joyCursor = nullptr, *joyCircle = nullptr;
static lv_obj_t *labelRaw = nullptr, *labelNorm = nullptr;
static lv_obj_t *labelDir = nullptr, *labelButtons = nullptr;

// ═══ structures du jeu ═══
struct SaveSlot
{
  bool used;
  int characterId, currentLevel, score, lives;
  char name[12];
};
#define MAX_SAVES 3
SaveSlot saves[MAX_SAVES];
enum PowerUp
{
  POWERUP_NONE = 0,
  POWERUP_MUSHROOM = 1,
  POWERUP_FIRE = 2,
  POWERUP_MINI = 3
};
enum CharacterId
{
  CHAR_MARIO = 0,
  CHAR_LUIGI = 1,
  CHAR_TOAD = 2
};
struct Player
{
  float x, y, velX, velY;
  bool onGround, isAlive;
  // ── AJOUT accroupissement ─────────────────────────────────────────────────
  // crouching = true quand buttonDown est appuyé ET le joueur est au sol.
  // Utilisé dans updateGame() pour bloquer le saut,
  // et dans renderGame() pour modifier la pose visuelle des jambes.
  bool crouching;
  // ── FIN AJOUT ─────────────────────────────────────────────────────────────
  PowerUp powerUp;
  int lives, score, characterId;
};
Player player;
int currentLevel = 0, activeSaveSlot = 0;

#define GRAVITY 0.5f
#define WALK_SPEED 3.0f
#define JUMP_VELOCITY -7.0f // réduit pour un saut plus court
#define GROUND_Y 200.0f

// Ces constantes sont utilisées dans updateGame() ET dans le rendu.
// Elles doivent être définies AVANT updateGame().
#define SCREEN_W 480
#define SCREEN_H 272
#define PLAYER_W 16
#define PLAYER_H 24
#define GROUND_VISUAL_Y 200
#define WORLD_W 2000

// cameraX partagée entre updateGame() (respawn) et renderGame() (défilement)
static float cameraX = 0.0f;

// ── Déclarations anticipées ───────────────────────────────────────────────────
// triggerGameOver() est appelée dans updateGame() mais utilise des variables
// globales (sprites, showMenu) définies plus bas dans le fichier.
// On la déclare ici et on la définit après showMenu().
void triggerGameOver();
void showMenu();

// ── Structure plateforme ──────────────────────────────────────────────────────
struct Platform
{
  float x, y, w, h;
  uint32_t color;
};
#define MAX_PLATFORMS 24
Platform platforms[MAX_PLATFORMS];
int platformCount = 0;
static lv_obj_t *platObjs[MAX_PLATFORMS] = {};

// ── ÉTAPE 6 : Structure Goomba ────────────────────────────────────────────────
// Un Goomba = ennemi qui patrouille entre patrolLeft et patrolRight.
// Sprite : chapeau (rectangle étroit marron foncé) + corps (marron moyen)
//          + pied gauche + pied droit (deux petits rectangles marron clair).
// alive = false → mort (écrasé), on cache les rectangles LVGL.
//
// Dimensions du sprite (S=2) :
//   chapeau : 14px large × 4px haut   (7 cols × 2 rows)
//   corps   : 14px large × 8px haut   (7 cols × 4 rows)
//   pieds   :  6px large × 4px haut chacun (3 cols × 2 rows), séparés de 2px
//   hauteur totale : 16px  → GH = 16
//   largeur totale : 14px  → GW = 14
#define MAX_GOOMBAS 10 // max d'ennemis par niveau
#define GOOMBA_W 14.0f
#define GOOMBA_H 16.0f
#define GOOMBA_SPEED 1.2f // pixels par frame

struct Goomba
{
  float x, y;        // position monde (bas des pieds)
  float velX;        // vitesse courante (+= droite, -= gauche)
  bool alive;        // false = mort, sprites cachés
  float patrolLeft;  // limite gauche de patrouille
  float patrolRight; // limite droite de patrouille
  // rectangles LVGL du sprite
  lv_obj_t *objHat;   // chapeau (marron foncé, simulant le triangle)
  lv_obj_t *objBody;  // corps (marron moyen)
  lv_obj_t *objFeetL; // pied gauche (marron clair)
  lv_obj_t *objFeetR; // pied droit  (marron clair)
};

Goomba goombas[MAX_GOOMBAS]; // tableau des goombas du niveau
int goombaCount = 0;         // combien sont actifs ce niveau
// ── FIN ÉTAPE 6 : Structure ───────────────────────────────────────────────────

// sauvegardes
void initSaves()
{
  for (int i = 0; i < MAX_SAVES; i++)
  {
    saves[i].used = false;
    saves[i].characterId = CHAR_MARIO;
    saves[i].currentLevel = 0;
    saves[i].score = 0;
    saves[i].lives = 3;
    saves[i].name[0] = 'P';
    saves[i].name[1] = 'a';
    saves[i].name[2] = 'r';
    saves[i].name[3] = 't';
    saves[i].name[4] = 'i';
    saves[i].name[5] = 'e';
    saves[i].name[6] = ' ';
    saves[i].name[7] = '1' + i;
    saves[i].name[8] = '\0';
  }
}
void loadSave(int idx)
{
  activeSaveSlot = idx;
  currentLevel = saves[idx].currentLevel;
  player.score = saves[idx].score;
  player.lives = saves[idx].lives;
  player.characterId = saves[idx].characterId;
  player.powerUp = POWERUP_NONE;
  player.isAlive = true;
  player.x = 50.0f;
  player.y = GROUND_Y;
  player.velX = 0.0f;
  player.velY = 0.0f;
  player.onGround = true;
  player.crouching = false;
}
void newGame(int idx, int charId)
{
  activeSaveSlot = idx;
  saves[idx].used = true;
  saves[idx].characterId = charId;
  saves[idx].currentLevel = 0;
  saves[idx].score = 0;
  saves[idx].lives = 3;
  loadSave(idx);
}
void saveCurrentGame()
{
  saves[activeSaveSlot].used = true;
  saves[activeSaveSlot].currentLevel = currentLevel;
  saves[activeSaveSlot].score = player.score;
  saves[activeSaveSlot].lives = player.lives;
  saves[activeSaveSlot].characterId = player.characterId;
}

// moteur de jeu (physique)
void updateGame(InputState &in)
{
  // ── AJOUT accroupissement : met à jour player.crouching ──────────────────
  // On ne peut s'accroupir que si on est au sol.
  // Quand accroupi : vitesse X réduite à 50%, saut bloqué.
  player.crouching = in.buttonDown && player.onGround;
  // ── FIN AJOUT ─────────────────────────────────────────────────────────────

  // Vitesse horizontale (réduite de moitié si accroupi)
  float speedMult = player.crouching ? 0.5f : 1.0f;
  player.velX = in.normalizedX() * WALK_SPEED * speedMult;

  // Saut bloqué si accroupi
  if (in.buttonJump && player.onGround && !player.crouching)
  {
    player.velY = JUMP_VELOCITY;
    player.onGround = false;
  }
  player.velY += GRAVITY;
  player.x += player.velX;
  player.y += player.velY;

  // ── Collision AABB complète (bloque par le haut ET par le bas) ───────────
  // On détecte de quel côté le joueur entre dans la plateforme en comparant
  // les positions AVANT et APRÈS le mouvement.
  //
  // Pour chaque plateforme, si les hitboxes se chevauchent après le mouvement :
  //   - Si le joueur descendait ET ses pieds étaient au-dessus → atterrissage
  //   - Si le joueur montait ET sa tête était en dessous → plafond, rebond
  //
  // Pourquoi les deux sens ?
  //   Sans la détection "plafond", le joueur pouvait traverser une plateforme
  //   par le bas en sautant fort. Maintenant il est bloqué des deux côtés.
  const float PW = 16.0f;
  const float PH = 26.0f;
  float py2_avant = player.y - player.velY; // Y pieds AVANT ce frame
  float py1_avant = py2_avant - PH;         // Y chapeau AVANT ce frame

  for (int i = 0; i < platformCount; i++)
  {
    Platform &p = platforms[i];
    float px1 = player.x;
    float px2 = player.x + PW;
    float py2 = player.y;      // Y pieds APRÈS
    float py1 = player.y - PH; // Y chapeau APRÈS

    float plx1 = p.x;
    float plx2 = p.x + p.w;
    float ply1 = p.y;       // haut de la plateforme
    float ply2 = p.y + p.h; // bas  de la plateforme

    bool overlapX = (px2 > plx1) && (px1 < plx2);

    if (!overlapX)
      continue;

    // ── Atterrissage : pieds traversent le haut de la plateforme ─────────
    // Avant : pieds au-dessus ou au niveau du haut (py2_avant <= ply1)
    // Après : pieds en dessous (py2 >= ply1)
    // ET le joueur descend (velY > 0)
    if (py2_avant <= ply1 && py2 >= ply1 && player.velY > 0)
    {
      player.y = ply1;
      player.velY = 0.0f;
      player.onGround = true;
    }
    // ── Plafond : tête traverse le bas de la plateforme ──────────────────
    // Avant : chapeau en dessous ou au niveau du bas (py1_avant >= ply2)
    // Après : chapeau au-dessus (py1 <= ply2)
    // ET le joueur monte (velY < 0)
    else if (py1_avant >= ply2 && py1 <= ply2 && player.velY < 0)
    {
      player.y = ply2 + PH; // repousse le joueur sous la plateforme
      player.velY = 0.0f;   // annule la montée
    }
  }

  // Chute hors sol → perd une vie, respawn
  if (player.y > SCREEN_H + 30)
  {
    player.lives--;
    // Vies ne peuvent pas descendre sous 0
    if (player.lives < 0)
      player.lives = 0;

    if (player.lives == 0)
    {
      triggerGameOver();
      return;
    }

    player.x = 50.0f;
    player.y = GROUND_Y;
    player.velX = 0.0f;
    player.velY = 0.0f;
    player.onGround = true;
    player.crouching = false;
    cameraX = 0.0f;
  }

  if (player.x < 0.0f)
    player.x = 0.0f;
  if (player.x > WORLD_W - 16.0f)
    player.x = WORLD_W - 16.0f;

  // ── ÉTAPE 6 : mise à jour des Goombas ────────────────────────────────────
  // Chaque frame, on déplace les Goombas vivants et on teste deux collisions :
  //   1. Joueur saute DESSUS  → Goomba mort, +100 pts, petit rebond du joueur
  //   2. Joueur touche SUR LE CÔTÉ → joueur perd une vie et respawn
  //
  // Hitbox Goomba : bord gauche = g.x, bord droit = g.x + GW
  //                bord haut   = g.y - GH, bord bas = g.y
  //
  // "Sauter dessus" : on compare la position Y des pieds du joueur AVANT
  // et APRÈS le frame. Si avant les pieds étaient au-dessus du haut du Goomba
  // et après ils sont en dessous, c'est un écrasement par le dessus.
  //
  // "Toucher sur le côté" : simple chevauchement des deux hitboxes,
  // SAUF si le joueur est déjà en train d'atterrir dessus (cas ci-dessus).

  for (int i = 0; i < goombaCount; i++)
  {
    Goomba &g = goombas[i];
    if (!g.alive)
      continue;

    // Déplacement du Goomba
    g.x += g.velX;

    // Demi-tour aux limites de patrouille
    if (g.x <= g.patrolLeft)
    {
      g.x = g.patrolLeft;
      g.velX = GOOMBA_SPEED;
    }
    if (g.x + GOOMBA_W >= g.patrolRight)
    {
      g.x = g.patrolRight - GOOMBA_W;
      g.velX = -GOOMBA_SPEED;
    }

    // Hitboxes
    float gx1 = g.x;
    float gx2 = g.x + GOOMBA_W;
    float gy1 = g.y - GOOMBA_H; // haut du Goomba
    float gy2 = g.y;            // bas  du Goomba

    float px1 = player.x;
    float px2 = player.x + PW;
    float py1 = player.y - PH;
    float py2_now = player.y;
    // py2_avant est déjà calculé plus haut (position pieds avant ce frame)

    bool overlapX = (px2 > gx1) && (px1 < gx2);
    bool overlapY = (py2_now > gy1) && (py1 < gy2);

    if (overlapX && overlapY)
    {
      // Le joueur arrive-t-il par le dessus ?
      // Ses pieds étaient au-dessus du haut du Goomba et sont maintenant dedans.
      bool stomped = (py2_avant <= gy1) && (py2_now >= gy1) && (player.velY > 0);

      if (stomped)
      {
        // ── Écrasement : Goomba mort ──────────────────────────────────────
        g.alive = false;
        // Cache les sprites LVGL du Goomba (on les déplace hors écran)
        if (g.objHat)
          lv_obj_add_flag(g.objHat, LV_OBJ_FLAG_HIDDEN);
        if (g.objBody)
          lv_obj_add_flag(g.objBody, LV_OBJ_FLAG_HIDDEN);
        if (g.objFeetL)
          lv_obj_add_flag(g.objFeetL, LV_OBJ_FLAG_HIDDEN);
        if (g.objFeetR)
          lv_obj_add_flag(g.objFeetR, LV_OBJ_FLAG_HIDDEN);
        // Petit rebond du joueur vers le haut
        player.velY = JUMP_VELOCITY * 0.5f;
        player.onGround = false;
        // Points
        player.score += 100;
      }
      else
      {
        // ── Contact latéral : joueur blessé ──────────────────────────────
        player.lives--;
        if (player.lives < 0)
          player.lives = 0;

        if (player.lives == 0)
        {
          triggerGameOver();
          return;
        }

        player.x = 50.0f;
        player.y = GROUND_Y;
        player.velX = 0.0f;
        player.velY = 0.0f;
        player.onGround = true;
        cameraX = 0.0f;
      }
    }
  }
  // ── FIN ÉTAPE 6 : mise à jour Goombas ────────────────────────────────────
}

// écran test joystick
#define JOY_CIRCLE_R 90
#define JOY_CURSOR_R 8
void showMenu();
static void joyTestOkCb(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  joyCursor = nullptr;
  joyCircle = nullptr;
  labelRaw = nullptr;
  labelNorm = nullptr;
  labelDir = nullptr;
  labelButtons = nullptr;
  lv_obj_clean(lv_scr_act());
  currentScreen = SCREEN_MENU;
  showMenu();
}
void showJoystickTest()
{
  lv_obj_t *scr = lv_scr_act();
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Test joystick");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
  joyCircle = lv_obj_create(scr);
  lv_obj_set_size(joyCircle, JOY_CIRCLE_R * 2, JOY_CIRCLE_R * 2);
  lv_obj_set_style_radius(joyCircle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(joyCircle, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_bg_opa(joyCircle, LV_OPA_40, 0);
  lv_obj_set_style_border_color(joyCircle, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(joyCircle, 2, 0);
  lv_obj_set_style_pad_all(joyCircle, 0, 0);
  lv_obj_align(joyCircle, LV_ALIGN_LEFT_MID, 10, 10);
  int dzPx = (int)(((float)JOY_DEADZONE / ((float)JOY_MAX / 2.0f)) * JOY_CIRCLE_R);
  lv_obj_t *dzCircle = lv_obj_create(scr);
  lv_obj_set_size(dzCircle, dzPx * 2, dzPx * 2);
  lv_obj_set_style_radius(dzCircle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dzCircle, lv_color_hex(0x999999), 0);
  lv_obj_set_style_bg_opa(dzCircle, LV_OPA_50, 0);
  lv_obj_set_style_border_width(dzCircle, 1, 0);
  lv_obj_set_style_border_color(dzCircle, lv_color_hex(0x555555), 0);
  lv_obj_set_style_pad_all(dzCircle, 0, 0);
  lv_obj_align_to(dzCircle, joyCircle, LV_ALIGN_CENTER, 0, 0);
  joyCursor = lv_obj_create(scr);
  lv_obj_set_size(joyCursor, JOY_CURSOR_R * 2, JOY_CURSOR_R * 2);
  lv_obj_set_style_radius(joyCursor, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(joyCursor, lv_color_hex(0x1D9E75), 0);
  lv_obj_set_style_border_width(joyCursor, 0, 0);
  lv_obj_set_style_pad_all(joyCursor, 0, 0);
  lv_obj_align_to(joyCursor, joyCircle, LV_ALIGN_CENTER, 0, 0);
  labelRaw = lv_label_create(scr);
  lv_label_set_text(labelRaw, "X:  512\nY:  512");
  lv_obj_align(labelRaw, LV_ALIGN_LEFT_MID, 220, -60);
  labelNorm = lv_label_create(scr);
  lv_label_set_text(labelNorm, "nX: 0.00\nnY: 0.00");
  lv_obj_align(labelNorm, LV_ALIGN_LEFT_MID, 220, 0);
  labelDir = lv_label_create(scr);
  lv_label_set_text(labelDir, "Zone morte");
  lv_obj_align(labelDir, LV_ALIGN_LEFT_MID, 220, 60);
  labelButtons = lv_label_create(scr);
  lv_label_set_text(labelButtons, "JUMP:0  DOWN:0");
  lv_obj_align(labelButtons, LV_ALIGN_LEFT_MID, 220, 90);
  lv_obj_t *btnOk = lv_button_create(scr);
  lv_obj_align(btnOk, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_add_event_cb(btnOk, joyTestOkCb, LV_EVENT_ALL, NULL);
  lv_obj_t *lblOk = lv_label_create(btnOk);
  lv_label_set_text(lblOk, "OK -> menu");
  lv_obj_center(lblOk);
}
void updateJoystickTest(InputState &in)
{
  if (!joyCursor || !joyCircle)
    return;
  int ox = (int)(in.normalizedX() * JOY_CIRCLE_R);
  int oy = (int)(in.normalizedY() * JOY_CIRCLE_R);
  lv_obj_align_to(joyCursor, joyCircle, LV_ALIGN_CENTER, ox, oy);
  lv_obj_set_style_bg_color(joyCursor,
                            lv_color_hex(ox == 0 && oy == 0 ? 0x999999 : 0x1D9E75), 0);
  char buf[48];
  snprintf(buf, sizeof(buf), "X:  %4d\nY:  %4d", in.joyX, in.joyY);
  lv_label_set_text(labelRaw, buf);
  snprintf(buf, sizeof(buf), "nX: %+.2f\nnY: %+.2f", in.normalizedX(), in.normalizedY());
  lv_label_set_text(labelNorm, buf);
  if (ox == 0 && oy == 0)
  {
    lv_label_set_text(labelDir, "Zone morte");
  }
  else
  {
    char dir[32] = "";
    if (in.isLeft())
      strcat(dir, "GAUCHE ");
    if (in.isRight())
      strcat(dir, "DROITE ");
    if (in.isUp())
      strcat(dir, "HAUT ");
    if (in.isDown())
      strcat(dir, "BAS ");
    lv_label_set_text(labelDir, dir);
  }
  snprintf(buf, sizeof(buf), "JUMP:%d  DOWN:%d", (int)in.buttonJump, (int)in.buttonDown);
  lv_label_set_text(labelButtons, buf);
}

// palette pastel menu
#define COL_BG 0xD6F0E8
#define COL_SLOT_IDLE 0xC8EAE0
#define COL_SLOT_SEL 0x5DCAA5
#define COL_SLOT_BORDER 0x9FE1CB
#define COL_TEXT_DARK 0x085041
#define COL_TEXT_MID 0x0F6E56
#define COL_MARIO_IDLE 0xFFD6D6
#define COL_MARIO_SEL 0xF09595
#define COL_LUIGI_IDLE 0xD6F0DD
#define COL_LUIGI_SEL 0x97C459
#define COL_TOAD_IDLE 0xD6E8FF
#define COL_TOAD_SEL 0x85B7EB
#define COL_START_OFF 0xB4B2A9
#define COL_START_ON 0x1D9E75

static int menuSelectedSlot = -1, menuSelectedChar = CHAR_MARIO;
static bool menuIsNewGame = false;
static lv_obj_t *btnStart = nullptr, *charPanel = nullptr;
static lv_obj_t *slotBtns[MAX_SAVES] = {nullptr, nullptr, nullptr};
static lv_obj_t *charBtns[3] = {nullptr, nullptr, nullptr};
static const char *charNames[3] = {"Mario", "Luigi", "Toad"};
static const uint32_t charColIdle[3] = {COL_MARIO_IDLE, COL_LUIGI_IDLE, COL_TOAD_IDLE};
static const uint32_t charColSel[3] = {COL_MARIO_SEL, COL_LUIGI_SEL, COL_TOAD_SEL};
static void styleSlotBtn(lv_obj_t *btn, bool sel)
{
  lv_obj_set_style_bg_color(btn, lv_color_hex(sel ? COL_SLOT_SEL : COL_SLOT_IDLE), 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(sel ? COL_TEXT_DARK : COL_SLOT_BORDER), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
}
void showGame();
void renderGame();
static void slotBtnCb(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  menuSelectedSlot = (int)(intptr_t)lv_event_get_user_data(e);
  menuIsNewGame = !saves[menuSelectedSlot].used;
  for (int i = 0; i < MAX_SAVES; i++)
    if (slotBtns[i])
      styleSlotBtn(slotBtns[i], i == menuSelectedSlot);
  if (menuIsNewGame)
    lv_obj_clear_flag(charPanel, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(charPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(btnStart, lv_color_hex(COL_START_ON), 0);
  lv_obj_add_flag(btnStart, LV_OBJ_FLAG_CLICKABLE);
}
static void charBtnCb(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  menuSelectedChar = (int)(intptr_t)lv_event_get_user_data(e);
  for (int i = 0; i < 3; i++)
    if (charBtns[i])
      lv_obj_set_style_bg_color(charBtns[i],
                                lv_color_hex(i == menuSelectedChar ? charColSel[i] : charColIdle[i]), 0);
}
static void startBtnCb(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  if (menuSelectedSlot < 0)
    return;
  btnStart = nullptr;
  charPanel = nullptr;
  for (int i = 0; i < MAX_SAVES; i++)
    slotBtns[i] = nullptr;
  for (int i = 0; i < 3; i++)
    charBtns[i] = nullptr;
  if (menuIsNewGame)
    newGame(menuSelectedSlot, menuSelectedChar);
  else
    loadSave(menuSelectedSlot);
  lv_obj_clean(lv_scr_act());
  currentScreen = SCREEN_GAME;
  showGame();
}

void showMenu()
{
  menuSelectedSlot = -1;
  menuSelectedChar = CHAR_MARIO;
  menuIsNewGame = false;
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Super Mario STM32");
  lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT_DARK), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_t *sep = lv_obj_create(scr);
  lv_obj_set_size(sep, 440, 1);
  lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_style_bg_color(sep, lv_color_hex(COL_SLOT_BORDER), 0);
  lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  for (int i = 0; i < MAX_SAVES; i++)
  {
    slotBtns[i] = lv_button_create(scr);
    lv_obj_set_size(slotBtns[i], 140, 56);
    lv_obj_set_pos(slotBtns[i], 12 + i * 148, 38);
    lv_obj_set_style_radius(slotBtns[i], 10, 0);
    lv_obj_set_style_pad_all(slotBtns[i], 6, 0);
    styleSlotBtn(slotBtns[i], false);
    lv_obj_add_event_cb(slotBtns[i], slotBtnCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *lblNum = lv_label_create(slotBtns[i]);
    char numBuf[12];
    snprintf(numBuf, sizeof(numBuf), "Slot %d", i + 1);
    lv_label_set_text(lblNum, numBuf);
    lv_obj_set_style_text_color(lblNum, lv_color_hex(COL_TEXT_MID), 0);
    lv_obj_align(lblNum, LV_ALIGN_TOP_LEFT, 4, 2);
    lv_obj_t *lblMain = lv_label_create(slotBtns[i]);
    lv_obj_set_style_text_color(lblMain, lv_color_hex(COL_TEXT_DARK), 0);
    if (!saves[i].used)
    {
      lv_label_set_text(lblMain, "Nouvelle partie");
      lv_obj_align(lblMain, LV_ALIGN_CENTER, 0, 6);
    }
    else
    {
      static const char *cN[3] = {"Mario", "Luigi", "Toad"};
      char buf[40];
      snprintf(buf, sizeof(buf), "%s\nNiv.%d  %d pts",
               cN[saves[i].characterId], saves[i].currentLevel + 1, saves[i].score);
      lv_label_set_text(lblMain, buf);
      lv_obj_align(lblMain, LV_ALIGN_CENTER, 0, 6);
    }
  }
  charPanel = lv_obj_create(scr);
  lv_obj_set_size(charPanel, 456, 52);
  lv_obj_set_pos(charPanel, 12, 104);
  lv_obj_set_style_bg_color(charPanel, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(charPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(charPanel, 0, 0);
  lv_obj_set_style_pad_all(charPanel, 0, 0);
  lv_obj_clear_flag(charPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(charPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *charTitle = lv_label_create(charPanel);
  lv_label_set_text(charTitle, "Personnage :");
  lv_obj_set_style_text_color(charTitle, lv_color_hex(COL_TEXT_MID), 0);
  lv_obj_align(charTitle, LV_ALIGN_LEFT_MID, 0, 0);
  for (int i = 0; i < 3; i++)
  {
    charBtns[i] = lv_button_create(charPanel);
    lv_obj_set_size(charBtns[i], 100, 40);
    lv_obj_set_pos(charBtns[i], 110 + i * 108, 6);
    lv_obj_set_style_radius(charBtns[i], 8, 0);
    lv_obj_set_style_bg_color(charBtns[i], lv_color_hex(charColIdle[i]), 0);
    lv_obj_set_style_border_color(charBtns[i], lv_color_hex(charColSel[i]), 0);
    lv_obj_set_style_border_width(charBtns[i], 2, 0);
    lv_obj_add_event_cb(charBtns[i], charBtnCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *lbl = lv_label_create(charBtns[i]);
    lv_label_set_text(lbl, charNames[i]);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT_DARK), 0);
    lv_obj_center(lbl);
  }
  lv_obj_set_style_bg_color(charBtns[CHAR_MARIO], lv_color_hex(COL_MARIO_SEL), 0);
  btnStart = lv_button_create(scr);
  lv_obj_set_size(btnStart, 180, 44);
  lv_obj_set_pos(btnStart, (480 - 180) / 2, 214);
  lv_obj_set_style_radius(btnStart, 22, 0);
  lv_obj_set_style_bg_color(btnStart, lv_color_hex(COL_START_OFF), 0);
  lv_obj_clear_flag(btnStart, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btnStart, startBtnCb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *startLbl = lv_label_create(btnStart);
  lv_label_set_text(startLbl, "START");
  lv_obj_set_style_text_color(startLbl, lv_color_hex(0xF1EFE8), 0);
  lv_obj_center(startLbl);
}

#define SKY_COLOR 0xB3E5FC
#define GROUND_COLOR 0x4A7C3F
#define GRASS_COLOR 0x66BB6A

// Couleurs du sprite Goomba
#define GOOMBA_HAT_COL 0x4E342E  // marron foncé  (chapeau)
#define GOOMBA_BODY_COL 0x795548 // marron moyen  (corps)
#define GOOMBA_FEET_COL 0xA1887F // marron clair  (pieds)

static const uint32_t playerColors[3] = {0xE53935, 0x43A047, 0x1E88E5};

static lv_obj_t *objSky = nullptr;
static lv_obj_t *objGround = nullptr;
static lv_obj_t *objGrass = nullptr;
static lv_obj_t *objPlayer = nullptr;
static lv_obj_t *objHead = nullptr;
static lv_obj_t *lblScore = nullptr;
static lv_obj_t *lblLives = nullptr;
static lv_obj_t *lblLevel = nullptr;
static lv_obj_t *lblDebug = nullptr;

static lv_obj_t *spHat = nullptr;
static lv_obj_t *spHatTop = nullptr;
static lv_obj_t *spHatBrim = nullptr;
static lv_obj_t *spHair[2] = {nullptr, nullptr};
static lv_obj_t *spSpot3 = nullptr;
static lv_obj_t *spEye[2] = {nullptr, nullptr};
static lv_obj_t *spMustache = nullptr;
static lv_obj_t *spShirt = nullptr;
static lv_obj_t *spArm[2] = {nullptr, nullptr};
static lv_obj_t *spLegL = nullptr;
static lv_obj_t *spLegR = nullptr;
static lv_obj_t *spLegMid = nullptr;
static lv_obj_t *spShoeL = nullptr;
static lv_obj_t *spShoeR = nullptr;

// Helper : rectangle sans bordure ni padding
static lv_obj_t *makeRect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

static lv_obj_t *makeHudLabel(lv_obj_t *parent, int x, int y, int w)
{
  lv_obj_t *lbl = lv_label_create(parent);
  lv_obj_set_pos(lbl, x, y);
  lv_obj_set_width(lbl, w);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x212121), 0);
  lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
  return lbl;
}

static void moveSprite(lv_obj_t *obj, int bx, int by, int dx, int dy)
{
  if (obj)
    lv_obj_set_pos(obj, bx + dx, by + dy);
}

// ── triggerGameOver : remet tous les pointeurs à zéro et retourne au menu ────
// Placée ICI car elle a besoin de toutes les variables de sprites déclarées
// juste au-dessus (objGround, spHat, etc.).
// La déclaration anticipée en haut du fichier permet à updateGame() de l'appeler.
void triggerGameOver()
{
  objGround = nullptr;
  objGrass = nullptr;
  objPlayer = nullptr;
  objHead = nullptr;
  lblScore = nullptr;
  lblLives = nullptr;
  lblLevel = nullptr;
  lblDebug = nullptr;
  spHat = nullptr;
  spHatTop = nullptr;
  spHatBrim = nullptr;
  spHair[0] = nullptr;
  spHair[1] = nullptr;
  spSpot3 = nullptr;
  spEye[0] = nullptr;
  spEye[1] = nullptr;
  spMustache = nullptr;
  spShirt = nullptr;
  spArm[0] = nullptr;
  spArm[1] = nullptr;
  spLegL = nullptr;
  spLegR = nullptr;
  spLegMid = nullptr;
  spShoeL = nullptr;
  spShoeR = nullptr;
  for (int i = 0; i < MAX_PLATFORMS; i++)
    platObjs[i] = nullptr;
  platformCount = 0;
  for (int j = 0; j < MAX_GOOMBAS; j++)
  {
    goombas[j].alive = false;
    goombas[j].objHat = nullptr;
    goombas[j].objBody = nullptr;
    goombas[j].objFeetL = nullptr;
    goombas[j].objFeetR = nullptr;
  }
  goombaCount = 0;
  saves[activeSaveSlot].lives = 3;
  saves[activeSaveSlot].score = 0;
  saves[activeSaveSlot].currentLevel = 0;
  lv_obj_clean(lv_scr_act());
  currentScreen = SCREEN_MENU;
  showMenu();
}
// ── FIN triggerGameOver ───────────────────────────────────────────────────────

// ── Chargement des plateformes + dalles de sol ────────────────────────────────
void loadLevelPlatforms(lv_obj_t *scr)
{
  for (int i = 0; i < platformCount; i++)
    if (platObjs[i])
    {
      lv_obj_del(platObjs[i]);
      platObjs[i] = nullptr;
    }
  platformCount = 0;

  if (currentLevel == 0)
  {
    // Dalles de sol avec trous
    platforms[0] = {0, GROUND_Y, 350, SCREEN_H, 0x4A7C3F};
    // Trou 1 : 130px — pas de plateforme au-dessus
    platforms[1] = {480, GROUND_Y, 300, SCREEN_H, 0x4A7C3F};
    // Trou 2 : 120px — une plateforme intermédiaire pour aider
    platforms[2] = {900, GROUND_Y, 400, SCREEN_H, 0x4A7C3F};
    // Trou 3 : 150px — pas de plateforme
    platforms[3] = {1200, GROUND_Y, 800, SCREEN_H, 0x4A7C3F};
    // Plateformes flottantes (uniquement au-dessus du trou 2)
    platforms[4] = {150, 160, 70, 12, 0x795548};
    platforms[5] = {260, 130, 60, 12, 0x795548};
    platforms[6] = {630, 155, 80, 12, 0x795548}; // trou 2 aide
    platforms[7] = {750, 125, 60, 12, 0x795548}; // trou 2 aide
    platforms[8] = {1020, 155, 70, 12, 0x795548};
    platformCount = 9;
  }
  else if (currentLevel == 1)
  {
    platforms[0] = {0, GROUND_Y, 300, SCREEN_H, 0x4A7C3F};
    platforms[1] = {450, GROUND_Y, 250, SCREEN_H, 0x4A7C3F};
    platforms[2] = {900, GROUND_Y, 300, SCREEN_H, 0x4A7C3F};
    platforms[3] = {1350, GROUND_Y, 650, SCREEN_H, 0x4A7C3F};
    platforms[4] = {150, 150, 60, 12, 0x5D4037};
    platforms[5] = {280, 120, 60, 12, 0x5D4037};
    platforms[6] = {370, 150, 60, 12, 0x5D4037};
    platforms[7] = {700, 130, 60, 12, 0x5D4037};
    platforms[8] = {800, 100, 80, 12, 0x5D4037};
    platformCount = 9;
  }

  for (int i = 0; i < platformCount; i++)
    platObjs[i] = makeRect(scr,
                           (int)(platforms[i].x - cameraX), (int)(platforms[i].y),
                           (int)platforms[i].w, (int)platforms[i].h, platforms[i].color);
}

// ── ÉTAPE 6 : Création des sprites Goomba ────────────────────────────────────
// Appelée UNE SEULE FOIS depuis initGameObjects(), après loadLevelPlatforms().
// Pour chaque Goomba du tableau goombas[], on crée ses 4 rectangles LVGL.
//
// Disposition du sprite (S=2, donc 1 cellule = 2px) :
//   chapeau  : col0..6 row0..1  → 14×4px  (marron foncé, étroit = "pointe" du triangle)
//   corps    : col0..6 row2..5  → 14×8px  (marron moyen)
//   pied G   : col0..2 row6..7  →  6×4px  (marron clair)
//   pied D   : col4..6 row6..7  →  6×4px  (marron clair)
//   espace entre les pieds : col3 = 2px de vide
static void createGoombaSprites(lv_obj_t *scr)
{
  const int S = 2;
  for (int i = 0; i < goombaCount; i++)
  {
    Goomba &g = goombas[i];
    // Position écran initiale (sera recalculée dans renderGame)
    int sx = (int)(g.x - cameraX);
    int sy = (int)(g.y); // bas des pieds

    // Chapeau : 7*S large, 2*S haut → simule la pointe du triangle
    g.objHat = makeRect(scr, sx, sy - (int)GOOMBA_H, 7 * S, 2 * S, GOOMBA_HAT_COL);
    // Corps   : 7*S large, 4*S haut
    g.objBody = makeRect(scr, sx, sy - (int)GOOMBA_H + 2 * S, 7 * S, 4 * S, GOOMBA_BODY_COL);
    // Pied G  : 3*S large, 2*S haut
    g.objFeetL = makeRect(scr, sx, sy - 2 * S, 3 * S, 2 * S, GOOMBA_FEET_COL);
    // Pied D  : 3*S large, 2*S haut, décalé de 4*S (3 pieds + 1 espace)
    g.objFeetR = makeRect(scr, sx + 4 * S, sy - 2 * S, 3 * S, 2 * S, GOOMBA_FEET_COL);
  }
}
// ── FIN ÉTAPE 6 : création sprites ───────────────────────────────────────────

// ── ÉTAPE 6 : Chargement des Goombas d'un niveau ─────────────────────────────
// Définit quels Goombas existent, où ils patrouillent.
// Sol : max 3 Goombas, répartis sur les dalles de sol.
// Plateformes flottantes : max 1 Goomba par plateforme (pas toutes).
//
// patrolLeft / patrolRight = limites de la zone de patrouille en X monde.
// Pour les Goombas au sol : largeur de la dalle de sol.
// Pour les Goombas sur plateforme : bords de la plateforme.
// velX initial = -GOOMBA_SPEED (part vers la gauche au démarrage).
void loadLevelGoombas(lv_obj_t *scr)
{
  goombaCount = 0;

  if (currentLevel == 0)
  {
    // ── Goombas au sol (max 3) ────────────────────────────────────────────
    // Dalle 0 : x=0..350  → 1 Goomba
    goombas[0] = {200, GROUND_Y, -GOOMBA_SPEED, true, 10, 340, nullptr, nullptr, nullptr, nullptr};
    // Dalle 1 : x=480..780 → 1 Goomba
    goombas[1] = {560, GROUND_Y, -GOOMBA_SPEED, true, 490, 770, nullptr, nullptr, nullptr, nullptr};
    // Dalle 2 : x=900..1300 → 1 Goomba
    goombas[2] = {1000, GROUND_Y, -GOOMBA_SPEED, true, 910, 1290, nullptr, nullptr, nullptr, nullptr};

    // ── Goombas sur plateformes (1 par plateforme, pas toutes) ───────────
    // platforms[6] = plateforme à x=630, w=80 → 1 Goomba
    goombas[3] = {635, platforms[6].y, -GOOMBA_SPEED, true,
                  platforms[6].x + 2, platforms[6].x + platforms[6].w - GOOMBA_W - 2,
                  nullptr, nullptr, nullptr, nullptr};
    // platforms[8] = plateforme à x=1020, w=70 → 1 Goomba
    goombas[4] = {1025, platforms[8].y, -GOOMBA_SPEED, true,
                  platforms[8].x + 2, platforms[8].x + platforms[8].w - GOOMBA_W - 2,
                  nullptr, nullptr, nullptr, nullptr};
    goombaCount = 5;
  }
  else if (currentLevel == 1)
  {
    // ── Sol ──────────────────────────────────────────────────────────────
    goombas[0] = {100, GROUND_Y, -GOOMBA_SPEED, true, 10, 290, nullptr, nullptr, nullptr, nullptr};
    goombas[1] = {500, GROUND_Y, -GOOMBA_SPEED, true, 460, 690, nullptr, nullptr, nullptr, nullptr};
    goombas[2] = {1000, GROUND_Y, -GOOMBA_SPEED, true, 910, 1190, nullptr, nullptr, nullptr, nullptr};
    // ── Plateformes (platforms[7] = x=700, w=60) ─────────────────────────
    goombas[3] = {705, platforms[7].y, -GOOMBA_SPEED, true,
                  platforms[7].x + 2, platforms[7].x + platforms[7].w - GOOMBA_W - 2,
                  nullptr, nullptr, nullptr, nullptr};
    goombaCount = 4;
  }

  // Crée les sprites LVGL de chaque Goomba
  createGoombaSprites(scr);
}
// ── FIN ÉTAPE 6 : chargement Goombas ─────────────────────────────────────────

static void drawPlayer(lv_obj_t *scr)
{
  const int S = 2;
  int c = player.characterId;
  uint32_t colHat = (c == CHAR_LUIGI) ? 0x388E3C : 0xE53935;
  uint32_t colPants = 0x1565C0;
  uint32_t colShoes = 0x5D4037;
  uint32_t colSkin = 0xFFCDD2;
  uint32_t colHair = (c == CHAR_TOAD) ? 0xFFFFFF : 0x5D4037;
  uint32_t colEyes = 0x212121;
  uint32_t colNose = 0xFF8A80;
  uint32_t colBrim = 0xEEEEEE;

  if (c == CHAR_MARIO || c == CHAR_LUIGI)
  {
    spHatTop = makeRect(scr, 0, 0, 6 * S, 2 * S, colHat);
    spHat = makeRect(scr, 0, 0, 8 * S, 2 * S, colHat);
    spHatBrim = nullptr;
    objHead = makeRect(scr, 0, 0, 6 * S, 3 * S, colSkin);
    spHair[0] = makeRect(scr, 0, 0, 2 * S, 1 * S, colHair);
    spHair[1] = makeRect(scr, 0, 0, 2 * S, 1 * S, colHair);
    spEye[0] = makeRect(scr, 0, 0, 1 * S, 1 * S, colEyes);
    spEye[1] = makeRect(scr, 0, 0, 1 * S, 1 * S, colEyes);
    spMustache = makeRect(scr, 0, 0, 6 * S, 1 * S, colHair);
    spShirt = makeRect(scr, 0, 0, 8 * S, 3 * S, colHat);
    spArm[0] = makeRect(scr, 0, 0, 1 * S, 2 * S, colSkin);
    spArm[1] = makeRect(scr, 0, 0, 1 * S, 2 * S, colSkin);
    spLegL = makeRect(scr, 0, 0, 3 * S, 3 * S, colPants);
    spLegR = makeRect(scr, 0, 0, 3 * S, 3 * S, colPants);
    spLegMid = makeRect(scr, 0, 0, 4 * S, 2 * S, colPants);
    spShoeL = makeRect(scr, 0, 0, 3 * S, 2 * S, colShoes);
    spShoeR = makeRect(scr, 0, 0, 3 * S, 2 * S, colShoes);
    spSpot3 = nullptr;
  }
  else
  {
    spHatTop = makeRect(scr, 0, 0, 12 * S, 1 * S, colHat);
    spHat = makeRect(scr, 0, 0, 12 * S, 3 * S, colHat);
    spHatBrim = makeRect(scr, 0, 0, 10 * S, 1 * S, colBrim);
    spHair[0] = makeRect(scr, 0, 0, 3 * S, 2 * S, colHair);
    spHair[1] = makeRect(scr, 0, 0, 2 * S, 2 * S, colHair);
    spSpot3 = makeRect(scr, 0, 0, 3 * S, 2 * S, colHair);
    objHead = makeRect(scr, 0, 0, 8 * S, 3 * S, colSkin);
    spEye[0] = makeRect(scr, 0, 0, 2 * S, 1 * S, colEyes);
    spEye[1] = makeRect(scr, 0, 0, 2 * S, 1 * S, colEyes);
    spMustache = makeRect(scr, 0, 0, 2 * S, 1 * S, colNose);
    spShirt = makeRect(scr, 0, 0, 8 * S, 3 * S, colPants);
    spArm[0] = makeRect(scr, 0, 0, 1 * S, 2 * S, colSkin);
    spArm[1] = makeRect(scr, 0, 0, 1 * S, 2 * S, colSkin);
    spLegL = makeRect(scr, 0, 0, 3 * S, 3 * S, 0xFFFFFF);
    spLegR = makeRect(scr, 0, 0, 3 * S, 3 * S, 0xFFFFFF);
    spLegMid = makeRect(scr, 0, 0, 4 * S, 2 * S, 0xFFFFFF);
    spShoeL = makeRect(scr, 0, 0, 3 * S, 2 * S, colShoes);
    spShoeR = makeRect(scr, 0, 0, 3 * S, 2 * S, colShoes);
  }
  objPlayer = spShirt;
}

void initGameObjects(lv_obj_t *scr)
{
  lv_obj_set_style_bg_color(scr, lv_color_hex(SKY_COLOR), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);

  objGround = nullptr;
  objGrass = makeRect(scr, 0, GROUND_VISUAL_Y, WORLD_W, 4, GRASS_COLOR);

  lblScore = makeHudLabel(scr, 8, 4, 160);
  lv_label_set_text(lblScore, "Score: 000000");
  lblLives = makeHudLabel(scr, 210, 4, 60);
  lv_label_set_text(lblLives, "x3");
  lblLevel = makeHudLabel(scr, 400, 4, 70);
  lv_label_set_text(lblLevel, "Niv.1");
  lblDebug = makeHudLabel(scr, 8, SCREEN_H - 16, 140);
  lv_label_set_text(lblDebug, "x:0 y:0");

  // Plateformes + sol (créés avant le joueur et les Goombas pour le z-order)
  loadLevelPlatforms(scr);

  // ── ÉTAPE 6 : crée les Goombas après les plateformes ─────────────────────
  // Ordre z LVGL : créé après les plateformes = dessiné par-dessus le sol,
  // mais avant le joueur = le joueur sera au premier plan.
  loadLevelGoombas(scr);
  // ── FIN ÉTAPE 6 ──────────────────────────────────────────────────────────

  drawPlayer(scr);
  if (player.y < GROUND_Y)
    player.y = GROUND_Y;
  if (player.x < 50.0f)
    player.x = 50.0f;
  renderGame();
}

void renderGame()
{
  if (!objPlayer)
    return;

  // Caméra lerp
  float targetX = player.x - (float)SCREEN_W / 3.0f;
  if (targetX < 0.0f)
    targetX = 0.0f;
  cameraX += (targetX - cameraX) * 0.15f;

  int screenPX = (int)(player.x - cameraX);
  const int S = 2;
  int spriteH = (player.characterId == CHAR_TOAD) ? 15 * S : 13 * S;
  int screenPY = (int)(player.y);

  // ── AJOUT accroupissement : on abaisse le coin haut du sprite ────────────
  // Quand accroupi, on veut que les pieds restent au sol (screenPY inchangé)
  // et que le haut du corps descende.
  // Solution : on augmente by de legOffset → le sprite descend de legOffset px,
  // donc la tête descend mais les pieds (qui sont à screenPY) restent en place.
  // Dans moveSprite les jambes/chaussures sont remontées du même legOffset,
  // ce qui annule leur descente → elles restent à la même hauteur écran.
  // Résultat : corps+tête descendent, jambes restent au sol → accroupissement.
  int legOffset = player.crouching ? 2 * S : 0;
  int by = screenPY - spriteH + legOffset; // haut du sprite abaissé si accroupi
  // ── FIN AJOUT ─────────────────────────────────────────────────────────────

  int bx = screenPX;
  int c = player.characterId;

  if (c == CHAR_MARIO || c == CHAR_LUIGI)
  {
    // ── AJOUT accroupissement ─────────────────────────────────────────────
    // Quand accroupi, on remonte les jambes et chaussures de 4px (2*S)
    // pour qu'elles restent collées au corps (jambes "écrasées").
    // Le reste du sprite (chapeau, tête, corps) ne bouge pas.
    // Effet visuel : le personnage semble s'accroupir sur place.
    int legOffset = player.crouching ? 2 * S : 0; // décalage vertical des jambes
    // ── FIN AJOUT ─────────────────────────────────────────────────────────

    moveSprite(spHatTop, bx, by, 3 * S, 0 * S);
    moveSprite(spHat, bx, by, 2 * S, 1 * S);
    moveSprite(objHead, bx, by, 3 * S, 2 * S);
    moveSprite(spHair[0], bx, by, 3 * S, 2 * S);
    moveSprite(spHair[1], bx, by, 7 * S, 2 * S);
    moveSprite(spEye[0], bx, by, 4 * S, 3 * S);
    moveSprite(spEye[1], bx, by, 7 * S, 3 * S);
    moveSprite(spMustache, bx, by, 3 * S, 4 * S);
    moveSprite(spShirt, bx, by, 2 * S, 5 * S);
    moveSprite(spArm[0], bx, by, 1 * S, 5 * S);
    moveSprite(spArm[1], bx, by, 10 * S, 5 * S);
    // Jambes et chaussures remontées de legOffset quand accroupi
    moveSprite(spLegL, bx, by, 2 * S, 8 * S - legOffset);
    moveSprite(spLegR, bx, by, 7 * S, 8 * S - legOffset);
    moveSprite(spLegMid, bx, by, 4 * S, 8 * S - legOffset);
    moveSprite(spShoeL, bx, by, 2 * S, 11 * S - legOffset);
    moveSprite(spShoeR, bx, by, 7 * S, 11 * S - legOffset);
  }
  else
  {
    // ── AJOUT accroupissement Toad ────────────────────────────────────────
    // Même principe que Mario/Luigi : jambes et chaussures remontées de 2*S.
    int legOffset = player.crouching ? 2 * S : 0;
    // ── FIN AJOUT ─────────────────────────────────────────────────────────

    moveSprite(spHatTop, bx, by, 0 * S, 0 * S);
    moveSprite(spHat, bx, by, 0 * S, 1 * S);
    moveSprite(spHatBrim, bx, by, 1 * S, 4 * S);
    moveSprite(spHair[0], bx, by, 1 * S, 1 * S);
    moveSprite(spHair[1], bx, by, 5 * S, 1 * S);
    moveSprite(spSpot3, bx, by, 8 * S, 1 * S);
    moveSprite(objHead, bx, by, 2 * S, 4 * S);
    moveSprite(spEye[0], bx, by, 3 * S, 5 * S);
    moveSprite(spEye[1], bx, by, 7 * S, 5 * S);
    moveSprite(spMustache, bx, by, 5 * S, 6 * S);
    moveSprite(spShirt, bx, by, 2 * S, 7 * S);
    moveSprite(spArm[0], bx, by, 1 * S, 7 * S);
    moveSprite(spArm[1], bx, by, 10 * S, 7 * S);
    moveSprite(spLegL, bx, by, 2 * S, 10 * S - legOffset);
    moveSprite(spLegR, bx, by, 7 * S, 10 * S - legOffset);
    moveSprite(spLegMid, bx, by, 4 * S, 10 * S - legOffset);
    moveSprite(spShoeL, bx, by, 2 * S, 13 * S - legOffset);
    moveSprite(spShoeR, bx, by, 7 * S, 13 * S - legOffset);
  }

  // HUD
  char buf[48];
  snprintf(buf, sizeof(buf), "Score: %06d", player.score);
  lv_label_set_text(lblScore, buf);
  snprintf(buf, sizeof(buf), "x%d", player.lives);
  lv_label_set_text(lblLives, buf);
  snprintf(buf, sizeof(buf), "Niv.%d", currentLevel + 1);
  lv_label_set_text(lblLevel, buf);
  snprintf(buf, sizeof(buf), "x:%.0f y:%.0f", player.x, player.y);
  lv_label_set_text(lblDebug, buf);

  // Défilement herbe décorative
  if (objGrass)
    lv_obj_set_pos(objGrass, (int)(-cameraX), GROUND_VISUAL_Y);

  // Défilement plateformes + dalles de sol
  for (int i = 0; i < platformCount; i++)
    if (platObjs[i])
      lv_obj_set_pos(platObjs[i],
                     (int)(platforms[i].x - cameraX), (int)(platforms[i].y));

  // ── ÉTAPE 6 : défilement des sprites Goomba ──────────────────────────────
  // Même logique que les plateformes : position écran = position monde - cameraX.
  // g.y est le bas des pieds, donc on remonte de GOOMBA_H pour le chapeau.
  // On ne repositionne que les Goombas vivants (les morts sont cachés via HIDDEN).
  const int GS = 2; // même S que le reste
  for (int i = 0; i < goombaCount; i++)
  {
    Goomba &g = goombas[i];
    if (!g.alive)
      continue;

    int gsx = (int)(g.x - cameraX); // X écran bord gauche du Goomba
    int gsy = (int)(g.y);           // Y écran bas des pieds

    // Chapeau : col0, row0  → décalage (0, -GOOMBA_H)
    if (g.objHat)
      lv_obj_set_pos(g.objHat, gsx, gsy - (int)GOOMBA_H);
    // Corps   : col0, row2  → décalage (0, -GOOMBA_H + 2*GS)
    if (g.objBody)
      lv_obj_set_pos(g.objBody, gsx, gsy - (int)GOOMBA_H + 2 * GS);
    // Pied G  : col0, row6  → décalage (0, -2*GS)
    if (g.objFeetL)
      lv_obj_set_pos(g.objFeetL, gsx, gsy - 2 * GS);
    // Pied D  : col4, row6  → décalage (4*GS, -2*GS)
    if (g.objFeetR)
      lv_obj_set_pos(g.objFeetR, gsx + 4 * GS, gsy - 2 * GS);
  }
  // ── FIN ÉTAPE 6 : défilement Goombas ─────────────────────────────────────
}

void showGame()
{
  lv_obj_t *scr = lv_scr_act();
  cameraX = 0.0f;

  objSky = nullptr;
  objGround = nullptr;
  objGrass = nullptr;
  objPlayer = nullptr;
  objHead = nullptr;
  lblScore = nullptr;
  lblLives = nullptr;
  lblLevel = nullptr;
  lblDebug = nullptr;

  spHat = nullptr;
  spHatTop = nullptr;
  spHatBrim = nullptr;
  spHair[0] = nullptr;
  spHair[1] = nullptr;
  spSpot3 = nullptr;
  spEye[0] = nullptr;
  spEye[1] = nullptr;
  spMustache = nullptr;
  spShirt = nullptr;
  spArm[0] = nullptr;
  spArm[1] = nullptr;
  spLegL = nullptr;
  spLegR = nullptr;
  spLegMid = nullptr;
  spShoeL = nullptr;
  spShoeR = nullptr;

  for (int i = 0; i < MAX_PLATFORMS; i++)
    platObjs[i] = nullptr;
  platformCount = 0;

  // ── ÉTAPE 6 : remet les Goombas à zéro ───────────────────────────────────
  // Évite les pointeurs dangling vers des objets LVGL détruits par lv_obj_clean().
  for (int i = 0; i < MAX_GOOMBAS; i++)
  {
    goombas[i].alive = false;
    goombas[i].objHat = nullptr;
    goombas[i].objBody = nullptr;
    goombas[i].objFeetL = nullptr;
    goombas[i].objFeetR = nullptr;
  }
  goombaCount = 0;
  // ── FIN ÉTAPE 6 ──────────────────────────────────────────────────────────

  initGameObjects(scr);
}

void mySetup()
{
  initInputs();
  initSaves();
  showJoystickTest();
}
void loop()
{
  // Inactif (pour mise en veille du processeur)
}
void myTask(void *pvParameters)
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();
  while (1)
  {
    updateInputs();
    InputState inputs = getInputs();

    lvglLock();

    if (currentScreen == SCREEN_JOYSTICK_TEST)
      updateJoystickTest(inputs);
    else if (currentScreen == SCREEN_GAME)
    {
      updateGame(inputs);
      renderGame();
    }

    lvglUnlock();

    Serial.print("X: ");
    Serial.print(inputs.joyX);
    Serial.print(" Y: ");
    Serial.print(inputs.joyY);
    Serial.print(" Jump: ");
    Serial.print(inputs.buttonJump);
    Serial.print(" Down: ");
    Serial.println(inputs.buttonDown);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(33)); // 30fps
  }
}
#else
#include "lvgl.h"
#include "app_hal.h"
#include <cstdio>
int main(void)
{
  printf("LVGL Simulator\n");
  fflush(stdout);
  lv_init();
  hal_setup();
  testLvgl();
  hal_loop();
  return 0;
}
#endif