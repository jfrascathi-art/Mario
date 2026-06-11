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
    // Plage max utile ≈ ±67 counts
    const float MAX_RANGE = 65.0f; // remplace JOY_CENTER - JOY_DEADZONE
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
  PowerUp powerUp;
  int lives, score, characterId;
};
Player player;
int currentLevel = 0, activeSaveSlot = 0;

#define GRAVITY 0.5f
#define WALK_SPEED 3.0f
#define JUMP_VELOCITY -9.0f
#define GROUND_Y 200.0f

// ── ÉTAPE 5 AJOUT : structure d'une plateforme ───────────────────────────────
// Une plateforme = rectangle dans le monde (coordonnées monde, pas écran).
// x,y = coin haut-gauche | w,h = largeur/hauteur | color = couleur LVGL.
// Les coordonnées X peuvent dépasser 480px → le niveau défile avec la caméra.
struct Platform
{
  float x, y, w, h;
  uint32_t color;
};
#define MAX_PLATFORMS 16                       // nombre max de plateformes par niveau
Platform platforms[MAX_PLATFORMS];             // tableau des plateformes du niveau courant
int platformCount = 0;                         // combien sont actives dans ce niveau
static lv_obj_t *platObjs[MAX_PLATFORMS] = {}; // rectangle LVGL par plateforme
// ── FIN ÉTAPE 5 AJOUT : structure ────────────────────────────────────────────

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

// moteur de jeu (physique) — MODIFIÉ ÉTAPE 5 : ajout collision plateformes
void updateGame(InputState &in)
{
  player.velX = in.normalizedX() * WALK_SPEED;
  if (in.buttonJump && player.onGround)
  {
    player.velY = JUMP_VELOCITY;
    player.onGround = false;
  }
  player.velY += GRAVITY;
  player.x += player.velX;
  player.y += player.velY;

  // ── ÉTAPE 5 AJOUT : collision AABB avec les plateformes ──────────────────
  // AABB = Axis-Aligned Bounding Box.
  // On compare le rectangle du joueur avec chaque rectangle de plateforme.
  // Si les deux se chevauchent EN X et EN Y, et que le joueur DESCEND
  // (velY > 0), on le pose sur la surface de la plateforme.
  //
  // Pourquoi tester velY > 0 ?
  //   Sans ce test, en sautant PAR EN DESSOUS d'une plateforme le joueur
  //   serait bloqué contre le dessous au lieu de passer à travers.
  //
  // Le joueur : player.x = bord gauche, player.y = bas des pieds.
  // Taille hitbox : PW × PH pixels.
  const float PW = 16.0f; // largeur hitbox joueur
  const float PH = 26.0f; // hauteur hitbox joueur (13 rows × S=2)

  for (int i = 0; i < platformCount; i++)
  {
    Platform &p = platforms[i];

    // Bords du joueur
    float px1 = player.x;      // gauche
    float px2 = player.x + PW; // droite
    float py1 = player.y - PH; // haut (chapeau)
    float py2 = player.y;      // bas  (pieds)

    // Bords de la plateforme
    float plx1 = p.x;
    float plx2 = p.x + p.w;
    float ply1 = p.y;       // surface (haut) — là où on marche
    float ply2 = p.y + p.h; // dessous

    // Chevauchement horizontal ET vertical ?
    bool overlapX = (px2 > plx1) && (px1 < plx2);
    bool overlapY = (py2 > ply1) && (py1 < ply2);

    if (overlapX && overlapY && player.velY > 0)
    {
      // Atterrissage : on pose les pieds exactement sur le haut de la plateforme
      player.y = ply1;
      player.velY = 0.0f;
      player.onGround = true;
    }
  }
  // ── FIN ÉTAPE 5 AJOUT : collision ────────────────────────────────────────

  if (player.y >= GROUND_Y)
  {
    player.y = GROUND_Y;
    player.velY = 0.0f;
    player.onGround = true;
  }
  if (player.x < 0.0f)
    player.x = 0.0f;
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

// palette pastel menu (parce que c'est plus joli)
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

// variables globales menu
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
void renderGame(); // déclaration anticipée — défini plus bas, appelé depuis initGameObjects
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

#define SCREEN_W 480
#define SCREEN_H 272
#define PLAYER_W 16
#define PLAYER_H 24
#define GROUND_VISUAL_Y 200

// ── ÉTAPE 5 AJOUT : largeur du monde ─────────────────────────────────────────
// Le sol et l'herbe doivent être plus larges que l'écran pour ne pas
// avoir un bord visible quand la caméra avance.
// On les crée à WORLD_W px de large et on les repositionne comme les plateformes.
#define WORLD_W 2000 // largeur totale du niveau en pixels monde
// ── FIN ÉTAPE 5 AJOUT : largeur du monde ─────────────────────────────────────

#define SKY_COLOR 0xB3E5FC
#define GROUND_COLOR 0x4A7C3F
#define GRASS_COLOR 0x66BB6A

static const uint32_t playerColors[3] = {0xE53935, 0x43A047, 0x1E88E5};

static float cameraX = 0.0f;

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

// ── ÉTAPE 5 AJOUT : chargement des plateformes d'un niveau ───────────────────
// Appelée depuis initGameObjects() à chaque démarrage de niveau.
// - Détruit les anciens objets LVGL de plateformes.
// - Remplit le tableau platforms[] selon currentLevel.
// - Crée un rectangle LVGL par plateforme.
//
// Coordonnées "monde" : x peut dépasser 480px (le niveau défile).
// y = bord HAUT de la surface → le joueur atterrit à cette hauteur.
void loadLevelPlatforms(lv_obj_t *scr)
{
  // Détruit les rectangles LVGL du niveau précédent
  for (int i = 0; i < platformCount; i++)
  {
    if (platObjs[i])
    {
      lv_obj_del(platObjs[i]);
      platObjs[i] = nullptr;
    }
  }
  platformCount = 0;

  // Format : { x_monde, y_monde, largeur, hauteur, couleur }
  if (currentLevel == 0)
  {
    platforms[0] = {180, 160, 80, 12, 0x795548};
    platforms[1] = {320, 130, 60, 12, 0x795548};
    platforms[2] = {500, 150, 100, 12, 0x795548};
    platforms[3] = {680, 120, 80, 12, 0x795548};
    platformCount = 4;
  }
  else if (currentLevel == 1)
  {
    platforms[0] = {150, 150, 60, 12, 0x5D4037};
    platforms[1] = {280, 120, 60, 12, 0x5D4037};
    platforms[2] = {420, 100, 80, 12, 0x5D4037};
    platforms[3] = {580, 130, 60, 12, 0x5D4037};
    platforms[4] = {700, 90, 100, 12, 0x5D4037};
    platformCount = 5;
  }

  // Crée les rectangles LVGL — position initiale = position monde (cameraX = 0 ici)
  for (int i = 0; i < platformCount; i++)
  {
    platObjs[i] = makeRect(scr,
                           (int)(platforms[i].x - cameraX),
                           (int)(platforms[i].y),
                           (int)platforms[i].w,
                           (int)platforms[i].h,
                           platforms[i].color);
  }
}
// ── FIN ÉTAPE 5 AJOUT : chargement des plateformes ───────────────────────────

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

  // ── ÉTAPE 5 MODIFIÉ : sol et herbe créés à WORLD_W de large ──────────────
  // Avant : SCREEN_W (480px) → le sol s'arrêtait au bord de l'écran,
  //         donc quand la caméra avançait on voyait le fond derrière.
  // Maintenant : WORLD_W (2000px) → le sol couvre tout le niveau.
  // Dans renderGame() on repositionne objGround et objGrass selon cameraX,
  // exactement comme les plateformes.
  objGround = makeRect(scr, 0, GROUND_VISUAL_Y, WORLD_W, SCREEN_H - GROUND_VISUAL_Y, GROUND_COLOR);
  objGrass = makeRect(scr, 0, GROUND_VISUAL_Y, WORLD_W, 4, GRASS_COLOR);
  // ── FIN ÉTAPE 5 MODIFIÉ ──────────────────────────────────────────────────

  lblScore = makeHudLabel(scr, 8, 4, 160);
  lv_label_set_text(lblScore, "Score: 000000");
  lblLives = makeHudLabel(scr, 210, 4, 60);
  lv_label_set_text(lblLives, "x3");
  lblLevel = makeHudLabel(scr, 400, 4, 70);
  lv_label_set_text(lblLevel, "Niv.1");
  lblDebug = makeHudLabel(scr, 8, SCREEN_H - 16, 140);
  lv_label_set_text(lblDebug, "x:0 y:0");

  // ── ÉTAPE 5 AJOUT : charge les plateformes AVANT drawPlayer ──────────────
  // L'ordre de création LVGL = ordre z (profondeur).
  // Les plateformes doivent être SOUS le joueur → créées avant lui.
  loadLevelPlatforms(scr);
  // ── FIN ÉTAPE 5 AJOUT ────────────────────────────────────────────────────

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

  float targetX = player.x - (float)SCREEN_W / 3.0f;
  if (targetX < 0.0f)
    targetX = 0.0f;
  cameraX += (targetX - cameraX) * 0.15f;

  int screenPX = (int)(player.x - cameraX);

  const int S = 2;
  int spriteH = (player.characterId == CHAR_TOAD) ? 15 * S : 13 * S;
  int screenPY = (int)(player.y);
  int by = screenPY - spriteH;
  int bx = screenPX;

  int c = player.characterId;

  if (c == CHAR_MARIO || c == CHAR_LUIGI)
  {
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
    moveSprite(spLegL, bx, by, 2 * S, 8 * S);
    moveSprite(spLegR, bx, by, 7 * S, 8 * S);
    moveSprite(spLegMid, bx, by, 4 * S, 8 * S);
    moveSprite(spShoeL, bx, by, 2 * S, 11 * S);
    moveSprite(spShoeR, bx, by, 7 * S, 11 * S);
  }
  else
  {
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
    moveSprite(spLegL, bx, by, 2 * S, 10 * S);
    moveSprite(spLegR, bx, by, 7 * S, 10 * S);
    moveSprite(spLegMid, bx, by, 4 * S, 10 * S);
    moveSprite(spShoeL, bx, by, 2 * S, 13 * S);
    moveSprite(spShoeR, bx, by, 7 * S, 13 * S);
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

  //Défilement du sol et de l'herbe
  //On les décale de -cameraX pour qu'ils semblent défiler avec le niveau.
  //Quand le joueur avance, cameraX augmente et le sol part vers la gauche.
  if (objGround)
    lv_obj_set_pos(objGround, (int)(-cameraX), GROUND_VISUAL_Y);
  if (objGrass)
    lv_obj_set_pos(objGrass, (int)(-cameraX), GROUND_VISUAL_Y);

  //Défilement des plateformes
  // Position écran = position monde - cameraX.
  for (int i = 0; i < platformCount; i++)
  {
    if (platObjs[i])
    {
      int sx = (int)(platforms[i].x - cameraX); // X écran
      int sy = (int)(platforms[i].y);           // Y écran (Y monde = Y écran)
      lv_obj_set_pos(platObjs[i], sx, sy);
    }
  }
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

  // Remet les pointeurs de plateformes à zéro
  // Évite les pointeurs "dangling" vers des objets LVGL détruits par lv_obj_clean() juste avant (quand on revient du menu).
  for (int i = 0; i < MAX_PLATFORMS; i++)
    platObjs[i] = nullptr;
  platformCount = 0;
  
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

    // Verrouille LVGL avant de toucher aux objets graphiques
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
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(16));
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