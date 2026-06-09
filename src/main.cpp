#include "lvgl.h"
#define JOY_X A0
#define JOY_Y A1
#define BUTTON_JUMP D6
#define BUTTON_DOWN D7
#define INPUTS_H
#define JOY_CENTER   512
#define JOY_DEADZONE  80
#define JOY_MAX      1023
enum AppScreen {
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
  bool isLeft()  const { return joyX < JOY_CENTER - JOY_DEADZONE; }
  bool isRight() const { return joyX > JOY_CENTER + JOY_DEADZONE; }
  bool isUp()    const { return joyY < JOY_CENTER - JOY_DEADZONE; }
  bool isDown()  const { return joyY > JOY_CENTER + JOY_DEADZONE; }
  float normalizedX() const {
    if (isLeft())
      return -(float)(JOY_CENTER - JOY_DEADZONE - joyX) / (float)(JOY_CENTER - JOY_DEADZONE);
    if (isRight())
      return  (float)(joyX - JOY_CENTER - JOY_DEADZONE) / (float)(JOY_MAX - JOY_CENTER - JOY_DEADZONE);
    return 0.0f;
  }
  float normalizedY() const {
    if (isUp())
      return -(float)(JOY_CENTER - JOY_DEADZONE - joyY) / (float)(JOY_CENTER - JOY_DEADZONE);
    if (isDown())
      return  (float)(joyY - JOY_CENTER - JOY_DEADZONE) / (float)(JOY_MAX - JOY_CENTER - JOY_DEADZONE);
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
}
void updateInputs()
{
  inputs.joyX = analogRead(JOY_X);
  inputs.joyY = analogRead(JOY_Y);
  inputs.buttonJump = !digitalRead(BUTTON_JUMP);
  inputs.buttonDown = !digitalRead(BUTTON_DOWN);
}
InputState getInputs()
{
  return inputs;
}
InputState getInputs();

// ═══ variables globales écran de test ═══
static lv_obj_t* joyCursor    = nullptr;
static lv_obj_t* joyCircle    = nullptr;
static lv_obj_t* labelRaw     = nullptr;
static lv_obj_t* labelNorm    = nullptr;
static lv_obj_t* labelDir     = nullptr;
static lv_obj_t* labelButtons = nullptr;

// ═══ structures du jeu ═══
struct SaveSlot {
  bool  used;
  int   characterId;
  int   currentLevel;
  int   score;
  int   lives;
  char  name[12];
};
#define MAX_SAVES 3
SaveSlot saves[MAX_SAVES];

enum PowerUp     { POWERUP_NONE=0, POWERUP_MUSHROOM=1, POWERUP_FIRE=2, POWERUP_MINI=3 };
enum CharacterId { CHAR_MARIO=0, CHAR_LUIGI=1, CHAR_TOAD=2 };

struct Player {
  float   x, y, velX, velY;
  bool    onGround, isAlive;
  PowerUp powerUp;
  int     lives, score, characterId;
};
Player player;
int currentLevel   = 0;
int activeSaveSlot = 0;

#define GRAVITY        0.5f
#define WALK_SPEED     3.0f
#define JUMP_VELOCITY -9.0f
#define GROUND_Y     220.0f

// ═══ sauvegardes ═══
void initSaves() {
  for (int i = 0; i < MAX_SAVES; i++) {
    saves[i].used=false; saves[i].characterId=CHAR_MARIO;
    saves[i].currentLevel=0; saves[i].score=0; saves[i].lives=3;
    saves[i].name[0]='P'; saves[i].name[1]='a'; saves[i].name[2]='r';
    saves[i].name[3]='t'; saves[i].name[4]='i'; saves[i].name[5]='e';
    saves[i].name[6]=' '; saves[i].name[7]='1'+i; saves[i].name[8]='\0';
  }
}
void loadSave(int idx) {
  activeSaveSlot=idx; currentLevel=saves[idx].currentLevel;
  player.score=saves[idx].score; player.lives=saves[idx].lives;
  player.characterId=saves[idx].characterId; player.powerUp=POWERUP_NONE;
  player.isAlive=true; player.x=50.0f; player.y=GROUND_Y;
  player.velX=0.0f; player.velY=0.0f; player.onGround=true;
}
void newGame(int idx, int charId) {
  activeSaveSlot=idx; saves[idx].used=true;
  saves[idx].characterId=charId; saves[idx].currentLevel=0;
  saves[idx].score=0; saves[idx].lives=3;
  loadSave(idx);
}
void saveCurrentGame() {
  saves[activeSaveSlot].used=true;
  saves[activeSaveSlot].currentLevel=currentLevel;
  saves[activeSaveSlot].score=player.score;
  saves[activeSaveSlot].lives=player.lives;
  saves[activeSaveSlot].characterId=player.characterId;
}

// ═══ moteur de jeu ═══
void updateGame(InputState& in) {
  player.velX = in.normalizedX() * WALK_SPEED;
  if (in.buttonJump && player.onGround) {
    player.velY=JUMP_VELOCITY; player.onGround=false;
  }
  player.velY += GRAVITY;
  player.x    += player.velX;
  player.y    += player.velY;
  if (player.y >= GROUND_Y) { player.y=GROUND_Y; player.velY=0.0f; player.onGround=true; }
  if (player.x < 0.0f) player.x=0.0f;
}

// ═══ écran test joystick ═══
#define JOY_CIRCLE_R  90
#define JOY_CURSOR_R   8

void showMenu(); // déclaration anticipée

static void joyTestOkCb(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    joyCursor=nullptr; joyCircle=nullptr;
    labelRaw=nullptr; labelNorm=nullptr;
    labelDir=nullptr; labelButtons=nullptr;
    lv_obj_clean(lv_scr_act());
    currentScreen = SCREEN_MENU;
    showMenu();
  }
}
void showJoystickTest() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Test joystick");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
  joyCircle = lv_obj_create(scr);
  lv_obj_set_size(joyCircle, JOY_CIRCLE_R*2, JOY_CIRCLE_R*2);
  lv_obj_set_style_radius(joyCircle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(joyCircle, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_bg_opa(joyCircle, LV_OPA_40, 0);
  lv_obj_set_style_border_color(joyCircle, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(joyCircle, 2, 0);
  lv_obj_set_style_pad_all(joyCircle, 0, 0);
  lv_obj_align(joyCircle, LV_ALIGN_LEFT_MID, 10, 10);
  int dzPx = (int)(((float)JOY_DEADZONE/((float)JOY_MAX/2.0f))*JOY_CIRCLE_R);
  lv_obj_t* dzCircle = lv_obj_create(scr);
  lv_obj_set_size(dzCircle, dzPx*2, dzPx*2);
  lv_obj_set_style_radius(dzCircle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dzCircle, lv_color_hex(0x999999), 0);
  lv_obj_set_style_bg_opa(dzCircle, LV_OPA_50, 0);
  lv_obj_set_style_border_width(dzCircle, 1, 0);
  lv_obj_set_style_border_color(dzCircle, lv_color_hex(0x555555), 0);
  lv_obj_set_style_pad_all(dzCircle, 0, 0);
  lv_obj_align_to(dzCircle, joyCircle, LV_ALIGN_CENTER, 0, 0);
  joyCursor = lv_obj_create(scr);
  lv_obj_set_size(joyCursor, JOY_CURSOR_R*2, JOY_CURSOR_R*2);
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
  lv_obj_t* btnOk = lv_button_create(scr);
  lv_obj_align(btnOk, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_add_event_cb(btnOk, joyTestOkCb, LV_EVENT_ALL, NULL);
  lv_obj_t* lblOk = lv_label_create(btnOk);
  lv_label_set_text(lblOk, "OK -> menu");
  lv_obj_center(lblOk);
}
void updateJoystickTest(InputState& in) {
  if (joyCursor==nullptr || joyCircle==nullptr) return;
  int offsetX=(int)(in.normalizedX()*JOY_CIRCLE_R);
  int offsetY=(int)(in.normalizedY()*JOY_CIRCLE_R);
  lv_obj_align_to(joyCursor, joyCircle, LV_ALIGN_CENTER, offsetX, offsetY);
  if (offsetX==0 && offsetY==0)
    lv_obj_set_style_bg_color(joyCursor, lv_color_hex(0x999999), 0);
  else
    lv_obj_set_style_bg_color(joyCursor, lv_color_hex(0x1D9E75), 0);
  char buf[48];
  snprintf(buf, sizeof(buf), "X:  %4d\nY:  %4d", in.joyX, in.joyY);
  lv_label_set_text(labelRaw, buf);
  snprintf(buf, sizeof(buf), "nX: %+.2f\nnY: %+.2f", in.normalizedX(), in.normalizedY());
  lv_label_set_text(labelNorm, buf);
  if (offsetX==0 && offsetY==0) {
    lv_label_set_text(labelDir, "Zone morte");
  } else {
    char dir[32]="";
    if (in.isLeft())  strcat(dir,"GAUCHE ");
    if (in.isRight()) strcat(dir,"DROITE ");
    if (in.isUp())    strcat(dir,"HAUT ");
    if (in.isDown())  strcat(dir,"BAS ");
    lv_label_set_text(labelDir, dir);
  }
  snprintf(buf, sizeof(buf), "JUMP:%d  DOWN:%d", (int)in.buttonJump, (int)in.buttonDown);
  lv_label_set_text(labelButtons, buf);
}

// ═══ AJOUT : palette de couleurs pastel vert/bleu ═══
// Toutes les couleurs du menu sont définies ici en un seul endroit.
// Si tu veux ajuster une teinte, tu ne changes qu'une ligne.

// Fond de l'écran : vert pastel très clair (comme une prairie)
#define COL_BG          0xD6F0E8
// Fond des slots non sélectionnés : vert pastel doux
#define COL_SLOT_IDLE   0xC8EAE0
// Fond du slot sélectionné : vert menthe moyen
#define COL_SLOT_SEL    0x5DCAA5
// Bordure des slots : vert menthe
#define COL_SLOT_BORDER 0x9FE1CB
// Texte titre des slots (foncé pour contraste sur fond clair)
#define COL_TEXT_DARK   0x085041
// Texte secondaire des slots
#define COL_TEXT_MID    0x0F6E56

// Personnages — chacun a sa teinte pastel propre
// Mario : rose pastel / rouge pastel
#define COL_MARIO_IDLE  0xFFD6D6
#define COL_MARIO_SEL   0xF09595
#define COL_MARIO_BRD   0xF09595
// Luigi : vert pastel clair
#define COL_LUIGI_IDLE  0xD6F0DD
#define COL_LUIGI_SEL   0x97C459
#define COL_LUIGI_BRD   0x97C459
// Toad : bleu pastel
#define COL_TOAD_IDLE   0xD6E8FF
#define COL_TOAD_SEL    0x85B7EB
#define COL_TOAD_BRD    0x85B7EB

// START inactif : gris pastel
#define COL_START_OFF   0xB4B2A9
// START actif : vert émeraude
#define COL_START_ON    0x1D9E75

// ═══ AJOUT : variables globales du menu redesigné ═══
static int  menuSelectedSlot = -1;
static int  menuSelectedChar = CHAR_MARIO;
static bool menuIsNewGame    = false;

static lv_obj_t* btnStart            = nullptr;
static lv_obj_t* charPanel           = nullptr;
static lv_obj_t* slotBtns[MAX_SAVES] = {nullptr, nullptr, nullptr};
static lv_obj_t* charBtns[3]         = {nullptr, nullptr, nullptr};

// Noms affichés sur les boutons personnage
static const char* charNames[3] = {"Mario", "Luigi", "Toad"};

// Couleurs idle/sel de chaque personnage (pour les callbacks)
static const uint32_t charColIdle[3] = {COL_MARIO_IDLE, COL_LUIGI_IDLE, COL_TOAD_IDLE};
static const uint32_t charColSel[3]  = {COL_MARIO_SEL,  COL_LUIGI_SEL,  COL_TOAD_SEL};

// ═══ AJOUT : helper — applique le style pastel à un bouton slot ═══
// Factorise les appels répétitifs lv_obj_set_style_*
// "sel" = true → couleur sélectionnée, false → couleur idle
static void styleSlotBtn(lv_obj_t* btn, bool sel) {
  uint32_t bgCol  = sel ? COL_SLOT_SEL  : COL_SLOT_IDLE;
  uint32_t brdCol = sel ? COL_TEXT_DARK : COL_SLOT_BORDER;
  lv_obj_set_style_bg_color(btn,     lv_color_hex(bgCol),  0);
  lv_obj_set_style_border_color(btn, lv_color_hex(brdCol), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
}

// ═══ AJOUT : callbacks menu ═══

static void slotBtnCb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  menuSelectedSlot = (int)(intptr_t)lv_event_get_user_data(e);
  menuIsNewGame    = !saves[menuSelectedSlot].used;

  // Surligne le slot choisi, remet les autres en idle
  for (int i = 0; i < MAX_SAVES; i++) {
    if (slotBtns[i]) styleSlotBtn(slotBtns[i], i == menuSelectedSlot);
  }

  // Affiche ou cache le panneau personnage selon si c'est une nouvelle partie
  if (menuIsNewGame)
    lv_obj_clear_flag(charPanel, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(charPanel, LV_OBJ_FLAG_HIDDEN);

  // Active le START en vert
  lv_obj_set_style_bg_color(btnStart, lv_color_hex(COL_START_ON), 0);
  lv_obj_add_flag(btnStart, LV_OBJ_FLAG_CLICKABLE);
}

static void charBtnCb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  menuSelectedChar = (int)(intptr_t)lv_event_get_user_data(e);
  for (int i = 0; i < 3; i++) {
    if (charBtns[i])
      lv_obj_set_style_bg_color(charBtns[i],
        lv_color_hex(i == menuSelectedChar ? charColSel[i] : charColIdle[i]), 0);
  }
}

static void startBtnCb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (menuSelectedSlot < 0) return;

  // Nettoie les pointeurs
  btnStart=nullptr; charPanel=nullptr;
  for (int i=0;i<MAX_SAVES;i++) slotBtns[i]=nullptr;
  for (int i=0;i<3;i++) charBtns[i]=nullptr;

  if (menuIsNewGame)
    newGame(menuSelectedSlot, menuSelectedChar);
  else
    loadSave(menuSelectedSlot);

  lv_obj_clean(lv_scr_act());
  currentScreen = SCREEN_GAME;
}

// ═══ AJOUT : construction du menu pastel ═══
// Layout pour écran 480 x 272 px en PAYSAGE :
//
//  ┌──────────────────────────────────────────────┐
//  │         Super Mario STM32  (titre)           │  y=8, h=20
//  ├──────────────────────────────────────────────┤
//  │  [Slot 1]    [Slot 2]    [Slot 3]            │  y=34, h=56
//  ├──────────────────────────────────────────────┤
//  │  [Mario]   [Luigi]   [Toad]   (si nouveau)   │  y=100, h=52
//  ├──────────────────────────────────────────────┤
//  │                [  START  ]                   │  y=164, h=44
//  └──────────────────────────────────────────────┘
//
// Les 3 slots font chacun (480 - 2*12 - 2*8) / 3 = 140 px de large
// Les 3 perso font chacun (480 - 2*12 - 2*8) / 3 = 140 px de large

void showMenu() {
  menuSelectedSlot = -1;
  menuSelectedChar = CHAR_MARIO;
  menuIsNewGame    = false;

  lv_obj_t* scr = lv_scr_act();

  // --- Fond de l'écran : vert pastel très clair ---
  // lv_obj_set_style_bg_color sur l'écran racine change tout le fond.
  lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // --- Titre ---
  // Centré en haut, police légèrement plus grande, couleur vert foncé
  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Super Mario STM32");
  lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT_DARK), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  // --- Ligne de séparation sous le titre ---
  // Un rectangle fin de 1px de haut, pleine largeur
  lv_obj_t* sep = lv_obj_create(scr);
  lv_obj_set_size(sep, 440, 1);
  lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_style_bg_color(sep, lv_color_hex(COL_SLOT_BORDER), 0);
  lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(sep, 0, 0);

  // --- 3 boutons slot ---
  // Positionnés côte à côte. Chaque slot : x = 12 + i*(140+8), y=38, w=140, h=56
  // Valeurs : marge gauche=12, largeur=140, gap=8
  for (int i = 0; i < MAX_SAVES; i++) {
    slotBtns[i] = lv_button_create(scr);
    lv_obj_set_size(slotBtns[i], 140, 56);
    // Position absolue : décalage depuis le coin haut-gauche de l'écran
    lv_obj_set_pos(slotBtns[i], 12 + i * 148, 38);
    lv_obj_set_style_radius(slotBtns[i], 10, 0);
    lv_obj_set_style_pad_all(slotBtns[i], 6, 0);
    styleSlotBtn(slotBtns[i], false); // style idle par défaut

    lv_obj_add_event_cb(slotBtns[i], slotBtnCb, LV_EVENT_CLICKED,
                        (void*)(intptr_t)i);

    // Numéro du slot en petit en haut
    lv_obj_t* lblNum = lv_label_create(slotBtns[i]);
    char numBuf[12];
    snprintf(numBuf, sizeof(numBuf), "Slot %d", i+1);
    lv_label_set_text(lblNum, numBuf);
    lv_obj_set_style_text_color(lblNum, lv_color_hex(COL_TEXT_MID), 0);
    lv_obj_align(lblNum, LV_ALIGN_TOP_LEFT, 4, 2);

    // Texte principal du slot
    lv_obj_t* lblMain = lv_label_create(slotBtns[i]);
    lv_obj_set_style_text_color(lblMain, lv_color_hex(COL_TEXT_DARK), 0);

    if (!saves[i].used) {
      lv_label_set_text(lblMain, "Nouvelle partie");
      lv_obj_align(lblMain, LV_ALIGN_CENTER, 0, 6);
    } else {
      // Affiche le nom du perso + niveau + score sur 2 lignes
      static const char* cNames[3] = {"Mario","Luigi","Toad"};
      char buf[40];
      snprintf(buf, sizeof(buf), "%s\nNiv.%d  %d pts",
               cNames[saves[i].characterId],
               saves[i].currentLevel + 1,
               saves[i].score);
      lv_label_set_text(lblMain, buf);
      lv_obj_align(lblMain, LV_ALIGN_CENTER, 0, 6);
    }
  }

  // --- Panneau personnage (caché par défaut) ---
  // Apparaît sous les slots quand on choisit une nouvelle partie.
  // Même logique de positionnement : y=104, h=52
  charPanel = lv_obj_create(scr);
  lv_obj_set_size(charPanel, 456, 52);
  lv_obj_set_pos(charPanel, 12, 104);
  lv_obj_set_style_bg_color(charPanel, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(charPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(charPanel, 0, 0);
  lv_obj_set_style_pad_all(charPanel, 0, 0);
  lv_obj_clear_flag(charPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(charPanel, LV_OBJ_FLAG_HIDDEN); // caché au départ

  // Sous-titre "Personnage :"
  lv_obj_t* charTitle = lv_label_create(charPanel);
  lv_label_set_text(charTitle, "Personnage :");
  lv_obj_set_style_text_color(charTitle, lv_color_hex(COL_TEXT_MID), 0);
  lv_obj_align(charTitle, LV_ALIGN_LEFT_MID, 0, 0);

  // 3 boutons personnage côte à côte dans le panneau
  // Larges de 100px chacun, espacés de 8px, après le label (offset x=110)
  for (int i = 0; i < 3; i++) {
    charBtns[i] = lv_button_create(charPanel);
    lv_obj_set_size(charBtns[i], 100, 40);
    lv_obj_set_pos(charBtns[i], 110 + i * 108, 6);
    lv_obj_set_style_radius(charBtns[i], 8, 0);

    // Couleur idle propre à chaque personnage
    lv_obj_set_style_bg_color(charBtns[i], lv_color_hex(charColIdle[i]), 0);
    // Bordure assortie
    lv_obj_set_style_border_color(charBtns[i], lv_color_hex(charColSel[i]), 0);
    lv_obj_set_style_border_width(charBtns[i], 2, 0);

    lv_obj_add_event_cb(charBtns[i], charBtnCb, LV_EVENT_CLICKED,
                        (void*)(intptr_t)i);

    lv_obj_t* lbl = lv_label_create(charBtns[i]);
    lv_label_set_text(lbl, charNames[i]);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT_DARK), 0);
    lv_obj_center(lbl);
  }
  // Mario sélectionné par défaut → sa couleur sel dès le départ
  lv_obj_set_style_bg_color(charBtns[CHAR_MARIO], lv_color_hex(COL_MARIO_SEL), 0);

  // --- Bouton START ---
  // Centré horizontalement, y=168 (laisse 40px de marge en bas pour l'écran 272px)
  // Coins arrondis en pilule (radius = moitié de la hauteur = 22)
  btnStart = lv_button_create(scr);
  lv_obj_set_size(btnStart, 180, 44);
  lv_obj_set_pos(btnStart, (480 - 180) / 2, 214); // x centré = (480-180)/2 = 150
  lv_obj_set_style_radius(btnStart, 22, 0);        // pilule arrondie
  lv_obj_set_style_bg_color(btnStart, lv_color_hex(COL_START_OFF), 0);
  lv_obj_clear_flag(btnStart, LV_OBJ_FLAG_CLICKABLE); // désactivé au départ
  lv_obj_add_event_cb(btnStart, startBtnCb, LV_EVENT_CLICKED, NULL);

  lv_obj_t* startLbl = lv_label_create(btnStart);
  lv_label_set_text(startLbl, "START");
  lv_obj_set_style_text_color(startLbl, lv_color_hex(0xF1EFE8), 0);
  lv_obj_center(startLbl);
}

// à décommenter pour tester la démo
// #include "demos/lv_demos.h"

void mySetup()
{
  // à décommenter pour tester la démo
  // lv_demo_widgets();
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

    if (currentScreen == SCREEN_JOYSTICK_TEST)
      updateJoystickTest(inputs);
    // SCREEN_MENU : LVGL gère seul via callbacks tactiles
    // SCREEN_GAME : moteur de rendu ajouté à l'étape suivante

    Serial.print("X: ");
    Serial.print(inputs.joyX);
    Serial.print(" Y: ");
    Serial.print(inputs.joyY);
    Serial.print(" Jump: ");
    Serial.print(inputs.buttonJump);
    Serial.print(" Down: ");
    Serial.println(inputs.buttonDown);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
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