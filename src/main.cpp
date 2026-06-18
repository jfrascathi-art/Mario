#include "lvgl.h"
#include "GameTypes.h"
#include "LevelManager.h"

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
  SCREEN_GAME,
  // Écran de victoire, affiché après le drapeau final SI le boss était déjà
  // vaincu (sinon nextLevel() s'en charge comme avant). Écran statique
  // (un seul bouton OK), donc pas besoin de l'ajouter au if/else de myTask()
  // — exactement comme SCREEN_MENU, qui n'y est pas non plus traité.
  SCREEN_VICTORY
};
AppScreen currentScreen = SCREEN_JOYSTICK_TEST;

static void event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
    LV_LOG_USER("Clicked");
  else if (code == LV_EVENT_VALUE_CHANGED)
    LV_LOG_USER("Toggled");
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
  int joyX, joyY;
  bool buttonJump, buttonDown;
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
  inputs.joyY = 1023 - analogRead(JOY_Y);
  inputs.buttonJump = !digitalRead(BUTTON_JUMP);
  inputs.buttonDown = !digitalRead(BUTTON_DOWN);
}

InputState getInputs() { return inputs; }
InputState getInputs();

static lv_obj_t *joyCursor = nullptr, *joyCircle = nullptr;
static lv_obj_t *labelRaw = nullptr, *labelNorm = nullptr;
static lv_obj_t *labelDir = nullptr, *labelButtons = nullptr;

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
  bool onGround, isAlive, crouching;
  PowerUp powerUp;
  int lives, score, characterId;
  // Frames d'invincibilité restantes après avoir touché un Goomba en perdant
  // le power-up. Empêche un second contact (rebond trop lent) de retirer
  // aussi une vie le coup suivant. Voir bug #2 dans updateGame().
  int hitCooldown;
};
Player player;
int currentLevel = 0, activeSaveSlot = 0;

#define GRAVITY 0.5f
#define WALK_SPEED 2.0f
#define JUMP_VELOCITY -8.0f
#define PLAYER_W 16
#define PLAYER_H 24

// ── Variables globales partagées avec Level*.h ────────────────────────────────
float cameraX = 0.0f;
Platform platforms[MAX_PLATFORMS];
int platformCount = 0;
Goomba goombas[MAX_GOOMBAS];
int goombaCount = 0;
float flagX = 0.0f;

// ── Power-ups : blocs ?, items, boules de feu ──────────────────────────────
Block blocks[MAX_BLOCKS];
int blockCount = 0;
Item items[MAX_ITEMS];
int itemCount = 0;
FireBall fireballs[MAX_FIREBALLS];
int fireballCount = 0;
// powerUpTimer : frames restantes du power-up ACTUELLEMENT actif, quel qu'il
// soit (avant, seule la fleur de feu avait un timer — voir POWERUP_DURATION
// dans GameTypes.h pour le calcul des 125 frames = 5s).
int powerUpTimer = 0;
bool fireCooldown = false;
// joyUpBufferFrames : mémorise pendant quelques frames qu'on a récemment
// poussé le joystick vers le HAUT, pour tolérer un léger décalage avec
// l'appui du bouton accroupi (voir le tir de boules de feu plus bas). Sans
// ça, "joystick haut" ET "bouton appuyé" doivent être vrais EXACTEMENT à la
// même frame de 40ms, ce qui est physiquement difficile à viser à la main.
int joyUpBufferFrames = 0;

// ── Tubes ────────────────────────────────────────────────────────────────────
Pipe pipes[MAX_PIPES];
int pipeCount = 0;
// pipeWarpTimer : tant que > 0, une transition "descente dans un tube" est en
// cours : on saute toute la physique normale (voir le tout début de
// updateGame()) pour jouer une petite animation scriptée, puis on téléporte
// le joueur à pipeWarpExitX.
int pipeWarpTimer = 0;
float pipeWarpExitX = 0.0f;
// Courte invincibilité après une téléportation, pour ne pas redéclencher
// instantanément un second warp si le point d'arrivée est lui-même au-dessus
// d'un tube (même idée que player.hitCooldown pour les Goombas).
int pipeWarpCooldown = 0;
#define PIPE_WARP_DURATION 15 // ~0.6s @25fps
#define PIPE_WARP_SINK_SPEED 1.0f

static lv_obj_t *platObjs[MAX_PLATFORMS] = {};
// Bande d'herbe verte posée SUR chaque segment de sol (un cap par segment,
// jamais sur les plateformes flottantes ni au-dessus des trous). Remplace
// l'ancienne unique bande "objGrass" qui couvrait TOUT le monde d'un bloc,
// y compris par-dessus les trous où il n'y a pourtant aucun sol.
static lv_obj_t *grassCapObjs[MAX_PLATFORMS] = {};
static lv_obj_t *objFlagPole = nullptr;
static lv_obj_t *objFlagTop = nullptr;
#define FLAG_POLE_W 6
#define FLAG_POLE_H 60
#define FLAG_TOP_W 20
#define FLAG_TOP_H 14

// ── Boss final ─────────────────────────────────────────────────────────────
// Un seul boss possible (pas un tableau comme goombas[]) : un seul niveau en
// a un. bossActive=false par défaut, et SEUL Level4::load() (via addBoss(),
// voir Level.h) le passe à true — donc tout le code ci-dessous gardé par
// "if (bossActive)" ne fait jamais rien pour les niveaux 0 à 3.
Boss boss;
bool bossActive = false;
Hammer hammers[MAX_HAMMERS];
int hammerCount = 0;

void triggerGameOver();
void showMenu();
void nextLevel();
void triggerVictory();
void showVictory(int finalScore);

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
  player.hitCooldown = 0;
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

// ── damageBoss : centralise la perte d'un point de vie du boss ─────────────
// Appelée par une boule de feu OU par un coup de tête sauté (les deux moyens
// valides de le vaincre, exactement comme Bowser dans le vrai jeu).
// boss.hitCooldown évite qu'un seul contact (qui dure plusieurs frames tant
// que les boîtes se chevauchent) ne soit compté plusieurs fois de suite —
// même idée que player.hitCooldown pour les Goombas, voir plus bas.
void damageBoss()
{
  if (boss.hitCooldown > 0)
    return;
  boss.hp--;
  boss.hitCooldown = 15; // ~0.6s avant qu'un nouveau coup ne puisse compter
  player.score += 300;
  if (boss.hp <= 0)
  {
    boss.alive = false;
    if (boss.objBody)
      lv_obj_add_flag(boss.objBody, LV_OBJ_FLAG_HIDDEN);
    if (boss.objBelly)
      lv_obj_add_flag(boss.objBelly, LV_OBJ_FLAG_HIDDEN);
    if (boss.objHead)
      lv_obj_add_flag(boss.objHead, LV_OBJ_FLAG_HIDDEN);
    if (boss.objEyeL)
      lv_obj_add_flag(boss.objEyeL, LV_OBJ_FLAG_HIDDEN);
    if (boss.objEyeR)
      lv_obj_add_flag(boss.objEyeR, LV_OBJ_FLAG_HIDDEN);
    if (boss.objBrow)
      lv_obj_add_flag(boss.objBrow, LV_OBJ_FLAG_HIDDEN);
    if (boss.objHornL)
      lv_obj_add_flag(boss.objHornL, LV_OBJ_FLAG_HIDDEN);
    if (boss.objHornR)
      lv_obj_add_flag(boss.objHornR, LV_OBJ_FLAG_HIDDEN);
  }
}

// ── hurtPlayer : factorise "perdre le power-up OU perdre une vie" ──────────
// Cette logique existait déjà, copiée en dur dans le bloc de collision avec
// les Goombas (plus bas dans updateGame(), code historique INTACT, pas
// touché pour ne rien risquer de casser). Plutôt que de la copier-coller une
// troisième fois pour le corps du boss ET pour les marteaux, elle est
// extraite ici une seule fois et réutilisée par les deux.
// dangerX : position x du danger (boss ou marteau), pour savoir de quel
// côté repousser le joueur s'il ne fait QUE perdre son power-up — même idée
// que `(player.x > goombas[i].x) ? 2.0f : -2.0f` dans le bloc Goomba.
// Retourne false si triggerGameOver() vient d'être déclenché (plus de vies) :
// l'appelant DOIT alors faire `if (!hurtPlayer(...)) return;`, sinon il
// continuerait à utiliser un état de jeu déjà réinitialisé par
// triggerGameOver() (même piège que le code Goomba existant évite avec son
// propre `return;` explicite juste après son appel à triggerGameOver()).
bool hurtPlayer(float dangerX)
{
  if (player.hitCooldown > 0)
    return true;
  if (player.powerUp != POWERUP_NONE)
  {
    player.powerUp = POWERUP_NONE;
    powerUpTimer = 0;
    player.velX = (player.x > dangerX) ? 2.0f : -2.0f;
    player.velY = JUMP_VELOCITY * 0.4f;
    player.onGround = false;
    player.hitCooldown = 40;
    return true;
  }
  player.lives--;
  if (player.lives < 0)
    player.lives = 0;
  if (player.lives == 0)
  {
    saves[activeSaveSlot].score = player.score;
    saves[activeSaveSlot].currentLevel = currentLevel;
    triggerGameOver();
    return false;
  }
  saves[activeSaveSlot].lives = player.lives;
  saves[activeSaveSlot].score = player.score;
  saves[activeSaveSlot].currentLevel = currentLevel;
  player.x = 50.0f;
  player.y = GROUND_Y;
  player.velX = 0.0f;
  player.velY = 0.0f;
  player.onGround = true;
  player.hitCooldown = 40;
  cameraX = 0.0f;
  return true;
}

void updateGame(InputState &in)
{
  // ── TRANSITION "DESCENTE DANS UN TUBE" ─────────────────────────────────────
  // Tant que pipeWarpTimer > 0, on est en train de jouer l'animation
  // scriptée : le joueur s'enfonce dans le tube, immobile, sans aucune des
  // règles normales (gravité, collisions, saut...). On isole complètement ce
  // bloc du reste de la physique avec un `return` immédiat : c'est le moyen
  // le plus sûr de ne RIEN casser dans le code déjà existant en dessous.
  if (pipeWarpTimer > 0)
  {
    pipeWarpTimer--;
    player.y += PIPE_WARP_SINK_SPEED; // s'enfonce lentement
    player.velX = 0.0f;
    player.velY = 0.0f;
    if (pipeWarpTimer == 0)
    {
      player.x = pipeWarpExitX;
      player.y = GROUND_Y;
      player.onGround = true;
      pipeWarpCooldown = PIPE_WARP_DURATION;
      // Recentre la caméra D'UN COUP sur la nouvelle position (pas de
      // panoramique progressif, sinon on voit défiler tout le niveau entre
      // les deux points — l'effet "téléportation" doit être net).
      float targetX = player.x - (float)SCREEN_W / 3.0f;
      if (targetX < 0.0f)
        targetX = 0.0f;
      cameraX = targetX;
    }
    return;
  }
  if (pipeWarpCooldown > 0)
    pipeWarpCooldown--;

  if (player.hitCooldown > 0)
    player.hitCooldown--;
  player.crouching = in.buttonDown && player.onGround;
  float speedMult = player.crouching ? 0.5f : 1.0f;
  player.velX = in.normalizedX() * WALK_SPEED * speedMult;
  if (in.buttonJump && player.onGround && !player.crouching)
  {
    player.velY = JUMP_VELOCITY;
    player.onGround = false;
  }
  player.velY += GRAVITY;
  player.x += player.velX;
  player.y += player.velY;

  const float PW = 16.0f, PH = 26.0f;
  float py2_avant = player.y - player.velY;
  // MINI : hauteur réduite à 14px → passe sous les grottes (CAVE_H=22)
  // sans s'accroupir. MUSHROOM : hauteur augmentée à 34px.
  float PH_eff;
  if (player.crouching)
    PH_eff = 22.0f;
  else if (player.powerUp == POWERUP_MINI)
    PH_eff = 14.0f;
  else if (player.powerUp == POWERUP_MUSHROOM)
    PH_eff = 34.0f;
  else
    PH_eff = PH;

  // Réinitialiser onGround chaque frame : il sera remis à true uniquement
  // si une collision verticale (sol/plateforme/bloc) est détectée plus bas.
  // Sans ce reset, le joueur reste "au sol" même dans le vide, ce qui
  // empêche les atterrissages corrects sur les plateformes suivantes.
  player.onGround = false;

  for (int i = 0; i < platformCount; i++)
  {
    Platform &p = platforms[i];
    float px1 = player.x, px2 = player.x + PW;
    float py2 = player.y, py1 = player.y - PH_eff;
    float plx1 = p.x, plx2 = p.x + p.w, ply1 = p.y, ply2 = p.y + p.h;
    bool overlapX = (px2 > plx1) && (px1 < plx2);
    if (overlapX)
    {
      float py2_av = player.y - player.velY, py1_av = py2_av - PH_eff;
      bool crossedTop = (py2_av <= ply1) && (py2 >= ply1);
      bool skinnedTop = (py2 > ply1) && (py2 <= ply1 + 6.0f) && (py2_av <= ply1 + 2.0f);
      if ((crossedTop || skinnedTop) && player.velY >= 0)
      {
        player.y = ply1;
        player.velY = 0.0f;
        player.onGround = true;
      }
      else if (py1_av >= ply2 && py1 <= ply2 && player.velY < 0)
      {
        player.y = ply2 + PH_eff;
        player.velY = 0.0f;
      }
    }
    if (p.solid)
    {
      float py2n = player.y, py1n = player.y - PH_eff;
      bool overlapY = (py2n > ply1) && (py1n < ply2);
      if (overlapY)
      {
        float px1a = player.x - player.velX, px2a = px1a + PW;
        if (px2a <= plx1 && px2 > plx1)
        {
          player.x = plx1 - PW;
          player.velX = 0.0f;
        }
        else if (px1a >= plx2 && px1 < plx2)
        {
          player.x = plx2;
          player.velX = 0.0f;
        }
      }
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  // BLOCS ? : collision physique solide
  // ══════════════════════════════════════════════════════════════════════════
  // Un bloc est un solide 16×16 px. Deux comportements :
  //   CAS A : le joueur ATTERRIT sur le dessus  → bloqué comme une plateforme
  //   CAS B : il FRAPPE le dessous en montant   → déclenche l'item (si !hit)
  for (int i = 0; i < blockCount; i++)
  {
    Block &bl = blocks[i];
    float bx1 = bl.x, bx2 = bl.x + 16;
    float by1 = bl.y, by2 = bl.y + 16;
    float px1 = player.x, px2 = player.x + PW;
    if (!(px2 > bx1 && px1 < bx2))
      continue; // pas de chevauchement horizontal

    float py2 = player.y;
    float py2_prev = player.y - player.velY; // pieds à la frame précédente

    // CAS A : atterrissage sur le dessus du bloc
    if (py2_prev <= by1 && py2 >= by1 && player.velY >= 0)
    {
      player.y = by1;
      player.velY = 0.0f;
      player.onGround = true;
    }
    // CAS B : frappe par en-dessous (uniquement si pas déjà frappé)
    else if (!bl.hit)
    {
      float py1 = player.y - PH_eff;      // tête à la frame courante
      float py1_prev = py2_prev - PH_eff; // tête à la frame précédente
      if (py1_prev >= by2 && py1 <= by2 && player.velY < 0)
      {
        bl.hit = true;
        if (bl.obj)
          lv_obj_set_style_bg_color(bl.obj, lv_color_hex(0x9E9E9E), 0);
        if (bl.mark)
          lv_obj_add_flag(bl.mark, LV_OBJ_FLAG_HIDDEN); // cache le "?"
        player.velY = 2.0f;                             // petit rebond vers le bas
        player.y = by2 + PH_eff;                        // repousse la tête hors du bloc

        // Type 0 = brique décorative : pas d'item
        if (bl.powerUpType > 0 && itemCount < MAX_ITEMS)
        {
          uint32_t col = (bl.powerUpType == 1)   ? 0xE53935  // champignon rouge
                         : (bl.powerUpType == 2) ? 0xFF9900  // fleur orange
                                                 : 0x1E88E5; // mini bleu
          lv_obj_t *itObj = makeRect(lv_scr_act(),
                                     (int)(bl.x + 2 - cameraX),
                                     (int)(bl.y - 14),
                                     12, 12, col);
          items[itemCount++] = {bl.x + 2, bl.y - 14, -1.5f,
                                bl.powerUpType, true, false, itObj};
        }
      }
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  // ITEMS : physique + collecte
  // ══════════════════════════════════════════════════════════════════════════
  for (int i = 0; i < itemCount; i++)
  {
    Item &it = items[i];
    if (!it.active)
      continue;

    if (!it.onGround)
      it.velY += 0.25f;
    it.y += it.velY;

    // Atterrissage sur une dalle de sol (pas sur un élément "solid" : grotte,
    // tube ou marche d'escalier). Limitation connue : un item qui tomberait
    // exactement au-dessus d'un tube ou d'une marche traverserait sans s'y
    // poser (comme il le faisait déjà avec les grottes) — sans conséquence
    // dans nos niveaux puisqu'aucun bloc ? n'est placé au-dessus d'un tube
    // ou d'un escalier.
    if (!it.onGround)
    {
      float it_bot = it.y + 12;
      float it_bot_prev = it_bot - it.velY;
      for (int j = 0; j < platformCount; j++)
      {
        Platform &pp = platforms[j];
        if (pp.solid)
          continue;
        if (it.x + 12 <= pp.x || it.x >= pp.x + pp.w)
          continue;
        if (it_bot_prev <= pp.y && it_bot >= pp.y)
        {
          it.y = pp.y - 12;
          it.velY = 0.0f;
          it.onGround = true;
          break;
        }
      }
    }

    // Tombé dans un trou : disparaît
    if (it.y > SCREEN_H + 30)
    {
      it.active = false;
      if (it.obj)
        lv_obj_add_flag(it.obj, LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    // Collecte (hitbox généreuse +4px)
    bool tX = (player.x + PW + 4 > it.x) && (player.x - 4 < it.x + 12);
    bool tY = (player.y > it.y - 4) && (player.y - PH_eff < it.y + 16);
    if (tX && tY)
    {
      player.powerUp = (PowerUp)it.type;
      // Avant : seule la fleur de feu obtenait un timer. Maintenant, TOUS
      // les power-ups démarrent avec les mêmes 125 frames (5s) — voir le
      // calcul détaillé dans GameTypes.h (POWERUP_DURATION).
      powerUpTimer = POWERUP_DURATION;
      player.score += 50;
      it.active = false;
      if (it.obj)
        lv_obj_add_flag(it.obj, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  // POWER-UP : décompte du timer (commun aux 3 power-ups) + tir de boules de
  // feu + déplacement des boules
  // ══════════════════════════════════════════════════════════════════════════
  if (powerUpTimer > 0)
  {
    powerUpTimer--;
    if (powerUpTimer == 0)
    {
      // Cas particulier MINI → NORMAL : le joueur passe de 14px à 26px de
      // haut, donc la tête monte de 12px. Si un obstacle solide (plateforme,
      // grotte, tube, marche d'escalier) occupe cette bande de 12px
      // juste au-dessus de la tête actuelle, le faire regrandir maintenant
      // l'enfoncerait dans le décor. On repousse alors la fin de l'effet
      // d'une frame (powerUpTimer=1 → re-testé la frame suivante) jusqu'à ce
      // que la voie soit libre, exactement comme le vrai Mario ne peut pas
      // sortir d'un passage bas tant qu'il n'en est pas dégagé.
      bool blocked = false;
      if (player.powerUp == POWERUP_MINI)
      {
        // PH_eff vaut ici 14.0f (taille MINI, calculée plus haut dans cette
        // même frame puisque player.powerUp n'a pas encore changé) ; PH vaut
        // 26.0f (taille normale). curHeadY/newHeadY = position de la tête
        // AVANT/APRÈS le retour à la taille normale.
        float curHeadY = player.y - PH_eff;
        float newHeadY = player.y - PH;
        for (int k = 0; k < platformCount; k++)
        {
          Platform &pp = platforms[k];
          bool overlapX2 = (player.x + PW > pp.x) && (player.x < pp.x + pp.w);
          bool overlapY2 = (pp.y + pp.h > newHeadY) && (pp.y < curHeadY);
          if (overlapX2 && overlapY2)
          {
            blocked = true;
            break;
          }
        }
      }
      if (blocked)
        powerUpTimer = 1;
      else
        player.powerUp = POWERUP_NONE;
    }
  }

  // Tir : joystick HAUT + bouton accroupi, 1 tir par appui (cooldown).
  // joyUpBufferFrames tolère un petit décalage entre les deux appuis (voir
  // la déclaration en haut du fichier) : on se souvient d'un appui HAUT
  // pendant ~240ms, donc presser le bouton juste après (ou juste avant)
  // avoir poussé le manche suffit toujours à tirer.
  if (in.isUp())
    joyUpBufferFrames = 6;
  else if (joyUpBufferFrames > 0)
    joyUpBufferFrames--;

  // Le cooldown ne se réinitialise plus que sur le RELÂCHEMENT du bouton :
  // avant, il fallait aussi relâcher le manche, ce qui rendait les tirs
  // répétés (garder le manche en haut, marteler le bouton) plus durs que
  // nécessaire.
  if (!in.buttonDown)
    fireCooldown = false;

  if (player.powerUp == POWERUP_FIRE && powerUpTimer > 0 &&
      joyUpBufferFrames > 0 && in.buttonDown && !fireCooldown)
  {
    fireCooldown = true;
    int fbSlot = -1;
    for (int k = 0; k < MAX_FIREBALLS; k++)
      if (!fireballs[k].active)
      {
        fbSlot = k;
        break;
      }
    if (fbSlot < 0 && fireballCount < MAX_FIREBALLS)
      fbSlot = fireballCount++;
    // BUG FIX : quand un emplacement libre était trouvé par le scan juste
    // au-dessus (cas le plus courant), fireballCount n'était JAMAIS avancé.
    // Or les boucles de déplacement et de rendu sont bornées par
    // fireballCount ("for i < fireballCount") : un emplacement utilisé mais
    // situé au-delà de fireballCount n'était donc jamais déplacé ni repositionné
    // à l'écran -> il restait figé à sa position de création (0,0), exactement
    // le symptôme "boule fantôme en haut à gauche" déjà observé. On force
    // maintenant fireballCount à couvrir tout emplacement réellement utilisé.
    if (fbSlot >= fireballCount)
      fireballCount = fbSlot + 1;
    if (fbSlot >= 0)
    {
      float fbVel = (player.velX >= 0.0f) ? 5.0f : -5.0f;
      if (!fireballs[fbSlot].obj)
        fireballs[fbSlot].obj = makeRect(lv_scr_act(), 0, 0, 8, 8, 0xFF6600);
      else
        lv_obj_clear_flag(fireballs[fbSlot].obj, LV_OBJ_FLAG_HIDDEN);
      fireballs[fbSlot] = {player.x + 4, player.y - 12, fbVel, true, fireballs[fbSlot].obj};
    }
  }

  for (int i = 0; i < fireballCount; i++)
  {
    FireBall &fb = fireballs[i];
    if (!fb.active)
      continue;
    fb.x += fb.velX;
    if (fb.x < 0 || fb.x > WORLD_W)
    {
      fb.active = false;
      if (fb.obj)
        lv_obj_add_flag(fb.obj, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    for (int j = 0; j < goombaCount; j++)
    {
      Goomba &g = goombas[j];
      if (!g.alive)
        continue;
      if (fb.x + 8 > g.x && fb.x < g.x + GOOMBA_W &&
          fb.y + 8 > g.y - GOOMBA_H && fb.y < g.y)
      {
        g.alive = false;
        if (g.objHat)
          lv_obj_add_flag(g.objHat, LV_OBJ_FLAG_HIDDEN);
        if (g.objBody)
          lv_obj_add_flag(g.objBody, LV_OBJ_FLAG_HIDDEN);
        if (g.objFeetL)
          lv_obj_add_flag(g.objFeetL, LV_OBJ_FLAG_HIDDEN);
        if (g.objFeetR)
          lv_obj_add_flag(g.objFeetR, LV_OBJ_FLAG_HIDDEN);
        player.score += 200;
        fb.active = false;
        if (fb.obj)
          lv_obj_add_flag(fb.obj, LV_OBJ_FLAG_HIDDEN);
        break;
      }
    }
    // BOSS : une boule de feu qui touche le boss compte comme un coup (voir
    // damageBoss()) — c'est le premier des deux moyens valides de le vaincre,
    // 5 coups suffisent (référence au "5000 points" du jeu original).
    // `fb.active` est revérifié ici car la boucle Goomba juste au-dessus a pu
    // déjà désactiver cette boule de feu sur CE même frame.
    if (fb.active && bossActive && boss.alive &&
        fb.x + 8 > boss.x && fb.x < boss.x + BOSS_W &&
        fb.y + 8 > boss.y - BOSS_H && fb.y < boss.y)
    {
      damageBoss();
      fb.active = false;
      if (fb.obj)
        lv_obj_add_flag(fb.obj, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  // TUBES : le joueur est-il debout sur le CHAPEAU d'un tube descendable, et
  // pousse-t-il le joystick vers le BAS ?
  // ══════════════════════════════════════════════════════════════════════════
  // Note : ceci utilise in.isDown() (la position du MANCHE), pas le bouton
  // accroupi — bien différent du tir de boules de feu qui utilise lui le
  // BOUTON. Les deux gestes ne doivent jamais se confondre : monter le
  // manche + bouton = tirer ; manche vers le bas (seul) = descendre.
  if (pipeWarpCooldown <= 0)
  {
    for (int i = 0; i < pipeCount; i++)
    {
      Pipe &pp = pipes[i];
      if (!pp.warp)
        continue;
      float dy = player.y - pp.topY;
      if (dy < 0.0f)
        dy = -dy;
      bool standingOnTop = player.onGround && dy < 0.6f &&
                           (player.x + PW > pp.x) && (player.x < pp.x + pp.w);
      if (standingOnTop && in.isDown())
      {
        pipeWarpTimer = PIPE_WARP_DURATION;
        pipeWarpExitX = pp.exitX;
        break;
      }
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  // DRAPEAU / CHUTE / GOOMBAS
  // ══════════════════════════════════════════════════════════════════════════
  float pcx = player.x + PW * 0.5f;
  if (pcx >= flagX - 8 && pcx <= flagX + FLAG_POLE_W + 8 && player.y >= GROUND_Y - PH - 10)
  {
    // BOSS : sur ce niveau précis (bossActive), le drapeau ne ramène pas
    // directement au menu via nextLevel() (qui ferait ça pour currentLevel
    // >= 4, voir plus bas) — il déclenche l'écran de victoire à la place.
    // Si le boss est déjà vaincu (!boss.alive), bonus de 5000 points avant
    // d'y entrer — référence directe au bonus "5 boules de feu d'affilée"
    // du jeu original. Pas besoin de bonus s'il l'a juste évité en courant
    // par-dessous : c'est une stratégie valide, mais moins payante.
    if (bossActive)
    {
      if (!boss.alive)
        player.score += 5000;
      triggerVictory();
      return;
    }
    nextLevel();
    return;
  }

  if (player.y > SCREEN_H + 30)
  {
    player.lives--;
    if (player.lives < 0)
      player.lives = 0;
    if (player.lives == 0)
    {
      saves[activeSaveSlot].score = player.score;
      saves[activeSaveSlot].currentLevel = currentLevel;
      triggerGameOver();
      return;
    }
    saves[activeSaveSlot].lives = player.lives;
    saves[activeSaveSlot].score = player.score;
    saves[activeSaveSlot].currentLevel = currentLevel;
    player.x = 50.0f;
    player.y = GROUND_Y;
    player.velX = 0.0f;
    player.velY = 0.0f;
    player.onGround = true;
    player.crouching = false;
    player.powerUp = POWERUP_NONE;
    powerUpTimer = 0;
    cameraX = 0.0f;
  }
  if (player.x < 0.0f)
    player.x = 0.0f;
  if (player.x > WORLD_W - 16.0f)
    player.x = WORLD_W - 16.0f;

  float gSpeed = GOOMBA_SPEED;
  if (currentLevel == 2)
    gSpeed = GOOMBA_SPEED * 1.5f;
  else if (currentLevel >= 3)
    gSpeed = GOOMBA_SPEED * 2.2f;

  for (int i = 0; i < goombaCount; i++)
  {
    Goomba &g = goombas[i];
    if (!g.alive)
      continue;
    g.x += g.velX;
    if (g.x <= g.patrolLeft)
    {
      g.x = g.patrolLeft;
      g.velX = gSpeed;
    }
    if (g.x + GOOMBA_W >= g.patrolRight)
    {
      g.x = g.patrolRight - GOOMBA_W;
      g.velX = -gSpeed;
    }
    float gx1 = g.x, gx2 = g.x + GOOMBA_W, gy1 = g.y - GOOMBA_H, gy2 = g.y;
    float px1 = player.x, px2 = player.x + PW, py1 = player.y - PH, py2n = player.y;
    bool oX = (px2 > gx1) && (px1 < gx2), oY = (py2n > gy1) && (py1 < gy2);
    if (oX && oY)
    {
      bool stomped = (py2_avant <= gy1) && (py2n >= gy1) && (player.velY > 0);
      if (stomped)
      {
        g.alive = false;
        if (g.objHat)
          lv_obj_add_flag(g.objHat, LV_OBJ_FLAG_HIDDEN);
        if (g.objBody)
          lv_obj_add_flag(g.objBody, LV_OBJ_FLAG_HIDDEN);
        if (g.objFeetL)
          lv_obj_add_flag(g.objFeetL, LV_OBJ_FLAG_HIDDEN);
        if (g.objFeetR)
          lv_obj_add_flag(g.objFeetR, LV_OBJ_FLAG_HIDDEN);
        player.velY = JUMP_VELOCITY * 0.5f;
        player.onGround = false;
        player.score += 100;
      }
      else
      {
        // BUG FIX : sans compteur d'invincibilité, le léger recul (2px/frame)
        // après avoir perdu le power-up était trop lent pour sortir de la
        // hitbox du Goomba en un seul frame. Le contact se redéclenchait donc
        // la frame suivante, mais avec powerUp déjà à NONE -> on retombait
        // dans la branche "perdre une vie" : le joueur perdait le power-up
        // ET une vie pour un seul contact. hitCooldown ignore tout nouveau
        // contact Goomba pendant les frames suivant un coup, comme les
        // frames d'invincibilité du vrai Mario après une touche.
        if (player.hitCooldown <= 0)
        {
          if (player.powerUp != POWERUP_NONE)
          {
            // Power-up actif : le perdre au lieu d'une vie
            player.powerUp = POWERUP_NONE;
            powerUpTimer = 0;
            player.velX = (player.x > goombas[i].x) ? 2.0f : -2.0f;
            player.velY = JUMP_VELOCITY * 0.4f;
            player.onGround = false;
            player.hitCooldown = 40; // ~1.6s d'invincibilité (40 frames @ 25fps)
          }
          else
          {
            player.lives--;
            if (player.lives < 0)
              player.lives = 0;
            if (player.lives == 0)
            {
              saves[activeSaveSlot].score = player.score;
              saves[activeSaveSlot].currentLevel = currentLevel;
              triggerGameOver();
              return;
            }
            saves[activeSaveSlot].lives = player.lives;
            saves[activeSaveSlot].score = player.score;
            saves[activeSaveSlot].currentLevel = currentLevel;
            player.x = 50.0f;
            player.y = GROUND_Y;
            player.velX = 0.0f;
            player.velY = 0.0f;
            player.onGround = true;
            player.hitCooldown = 40;
            cameraX = 0.0f;
          }
        }
      }
    }
  }

  // ══════════════════════════════════════════════════════════════════════════
  // BOSS FINAL : patrouille, marteaux, collisions (rien ici si bossActive est
  // resté à false, c'est-à-dire pour tous les niveaux sauf celui qui appelle
  // addBoss() — voir Level.h / Level4.h)
  // ══════════════════════════════════════════════════════════════════════════
  if (bossActive && boss.alive)
  {
    if (boss.hitCooldown > 0)
      boss.hitCooldown--;

    // Patrouille : même logique de rebond aux bornes que les Goombas
    // ci-dessus, mais le boss ne tombe jamais (il reste sur son pont).
    boss.x += boss.velX;
    if (boss.x <= boss.patrolLeft)
    {
      boss.x = boss.patrolLeft;
      boss.velX = BOSS_SPEED;
    }
    if (boss.x + BOSS_W >= boss.patrolRight)
    {
      boss.x = boss.patrolRight - BOSS_W;
      boss.velX = -BOSS_SPEED;
    }

    // Lancer de marteau : l'intervalle RACCOURCIT avec les PV restants
    // (formule détaillée en commentaire dans GameTypes.h) pour un combat
    // qui devient plus intense au fil des coups portés.
    boss.throwTimer--;
    if (boss.throwTimer <= 0)
    {
      int interval = BOSS_HAMMER_INTERVAL_MAX - (BOSS_MAX_HP - boss.hp) * 8;
      if (interval < BOSS_HAMMER_INTERVAL_MIN)
        interval = BOSS_HAMMER_INTERVAL_MIN;
      boss.throwTimer = interval;

      // Recherche d'un emplacement libre, même motif que les boules de feu
      // (fbSlot) plus haut dans cette fonction.
      int hSlot = -1;
      for (int k = 0; k < MAX_HAMMERS; k++)
        if (!hammers[k].active)
        {
          hSlot = k;
          break;
        }
      if (hSlot < 0 && hammerCount < MAX_HAMMERS)
        hSlot = hammerCount++;
      if (hSlot >= hammerCount)
        hammerCount = hSlot + 1;
      if (hSlot >= 0)
      {
        // Trajectoire parabolique : vitesse verticale initiale vers le HAUT
        // (-6, un peu plus "lourd" que le saut du joueur à -8), puis GRAVITY
        // s'accumule chaque frame ci-dessous jusqu'à ce qu'il retombe au
        // sol — exactement le même calcul physique que player.velY.
        float hVelX = (player.x > boss.x) ? 2.5f : -2.5f;
        if (!hammers[hSlot].obj)
          hammers[hSlot].obj = makeRect(lv_scr_act(), 0, 0, 8, 8, 0x616161);
        else
          lv_obj_clear_flag(hammers[hSlot].obj, LV_OBJ_FLAG_HIDDEN);
        hammers[hSlot] = {boss.x + BOSS_W * 0.5f, boss.y - BOSS_H, hVelX, -6.0f, true, hammers[hSlot].obj};
      }
    }

    // Déplacement + collision des marteaux déjà en vol.
    for (int i = 0; i < hammerCount; i++)
    {
      Hammer &h = hammers[i];
      if (!h.active)
        continue;
      h.velY += GRAVITY;
      h.x += h.velX;
      h.y += h.velY;
      if (h.y >= GROUND_Y || h.x < 0 || h.x > WORLD_W)
      {
        h.active = false;
        if (h.obj)
          lv_obj_add_flag(h.obj, LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      bool hOX = (player.x + PW > h.x) && (player.x < h.x + 8.0f);
      bool hOY = (player.y > h.y) && (player.y - PH < h.y + 8.0f);
      if (hOX && hOY)
      {
        h.active = false;
        if (h.obj)
          lv_obj_add_flag(h.obj, LV_OBJ_FLAG_HIDDEN);
        if (!hurtPlayer(boss.x))
          return;
      }
    }

    // Contact direct avec le corps du boss : par-dessus = coup de tête
    // sauté (second moyen valide de le vaincre, voir damageBoss()), par le
    // côté = danger (comme un Goomba géant). Même structure que le bloc
    // Goomba ci-dessus, avec ses propres variables locales (px1/px2/py1/py2n
    // y sont déjà sorties de portée puisqu'elles étaient déclarées DANS la
    // boucle "for" des Goombas).
    float bx1 = boss.x, bx2 = boss.x + BOSS_W, by1 = boss.y - BOSS_H, by2 = boss.y;
    float bpx1 = player.x, bpx2 = player.x + PW, bpy1 = player.y - PH, bpy2 = player.y;
    bool bOX = (bpx2 > bx1) && (bpx1 < bx2), bOY = (bpy2 > by1) && (bpy1 < by2);
    if (bOX && bOY)
    {
      bool bStomped = (py2_avant <= by1) && (bpy2 >= by1) && (player.velY > 0);
      if (bStomped)
      {
        damageBoss();
        player.velY = JUMP_VELOCITY * 0.5f;
        player.onGround = false;
      }
      else if (!hurtPlayer(boss.x))
        return;
    }
  }
}

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
  lv_obj_set_style_bg_color(joyCursor, lv_color_hex(ox == 0 && oy == 0 ? 0x999999 : 0x1D9E75), 0);
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
      lv_obj_set_style_bg_color(charBtns[i], lv_color_hex(i == menuSelectedChar ? charColSel[i] : charColIdle[i]), 0);
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

// Palette : ciel bleu saturé façon NES, sol "herbe verte sur terre brune"
// façon Super Mario World / New Super Mario Bros (GROUND_DIRT_COLOR et
// GROUND_GRASS_COLOR sont définis dans GameTypes.h, partagés avec Level.h).
#define SKY_COLOR 0x5C94FC
#define GOOMBA_HAT_COL 0x4E342E
#define GOOMBA_BODY_COL 0x795548
#define GOOMBA_FEET_COL 0xD9B98C
// ── Boss final : palette ORIGINALE (pas une recopie des couleurs d'un
// personnage existant) — gris-bleu foncé pour la carapace, un plastron
// gris-bleu plus CLAIR (même famille de teintes, juste éclairci) pour casser
// la silhouette, brun pour la tête, deux yeux rouges symétriques surmontés
// d'un sourcil presque noir pour l'expression "en colère", et deux cornes
// ivoire pour la silhouette — voir le détail des positions dans
// createBossSprites().
#define BOSS_BODY_COL 0x37474F
#define BOSS_BELLY_COL 0x607D8B
#define BOSS_HEAD_COL 0x6D4C41
#define BOSS_EYE_COL 0xD32F2F
#define BOSS_BROW_COL 0x212121
#define BOSS_HORN_COL 0xECEFF1

static lv_obj_t *objSky = nullptr, *objGround = nullptr;
static lv_obj_t *objPlayer = nullptr, *objHead = nullptr;
static lv_obj_t *lblScore = nullptr, *lblLives = nullptr, *lblLevel = nullptr, *lblDebug = nullptr;
static lv_obj_t *spHat = nullptr, *spHatTop = nullptr, *spHatBrim = nullptr;
static lv_obj_t *spHair[2] = {nullptr, nullptr}, *spSpot3 = nullptr;
static lv_obj_t *spEye[2] = {nullptr, nullptr};
static lv_obj_t *spMustache = nullptr, *spShirt = nullptr;
static lv_obj_t *spArm[2] = {nullptr, nullptr};
static lv_obj_t *spLegL = nullptr, *spLegR = nullptr, *spLegMid = nullptr;
static lv_obj_t *spShoeL = nullptr, *spShoeR = nullptr;

// ── makeRect : plus static (appelée depuis Level*.h) ─────────────────────────
lv_obj_t *makeRect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
  lv_obj_t *obj = lv_obj_create(parent);
  // BUG FIX (durcissement) : si le pool mémoire de LVGL (LV_MEM_SIZE dans
  // lv_conf.h) est plein au moment de cet appel, lv_obj_create() renvoie
  // nullptr. Sans cette vérification, chacun des lv_obj_set_xxx() suivants
  // déréférençait ce pointeur nul -> Hard Fault / freeze total de la carte.
  // On retourne maintenant nullptr immédiatement : il manquera un sprite à
  // l'écran (dégradation visible mais bénigne) au lieu de figer la carte.
  if (!obj)
    return nullptr;
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
  // BUG FIX (durcissement) : même raison que dans makeRect() juste au-dessus.
  if (!lbl)
    return nullptr;
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

// ── createGoombaSprites : plus static (appelée depuis Level*.h) ───────────────
void createGoombaSprites(lv_obj_t *scr)
{
  const int S = 2;
  for (int i = 0; i < goombaCount; i++)
  {
    Goomba &g = goombas[i];
    int sx = (int)(g.x - cameraX), sy = (int)(g.y);
    g.objHat = makeRect(scr, sx, sy - (int)GOOMBA_H, 7 * S, 2 * S, GOOMBA_HAT_COL);
    g.objBody = makeRect(scr, sx, sy - (int)GOOMBA_H + 2 * S, 7 * S, 4 * S, GOOMBA_BODY_COL);
    g.objFeetL = makeRect(scr, sx, sy - 2 * S, 3 * S, 2 * S, GOOMBA_FEET_COL);
    g.objFeetR = makeRect(scr, sx + 4 * S, sy - 2 * S, 3 * S, 2 * S, GOOMBA_FEET_COL);
  }
}

// ── createBossSprites : sprites du boss final, appelée depuis Level4.h (même
// motif que createGoombaSprites() juste au-dessus : c'est le Level*.h qui
// décide quand les créer, après avoir rempli boss via addBoss()). Ne fait
// rien si bossActive est resté false (sécurité, normalement jamais appelée
// dans ce cas puisque seul Level4.h l'appelle, juste après addBoss()).
//
// Découpage en 8 rectangles (corps, plastron, tête, 2 yeux, sourcil,
// 2 cornes) — voir GameTypes.h pour le pourquoi du redesign. Toutes les
// positions ci-dessous sont calculées pour être SYMÉTRIQUES par rapport à
// l'axe vertical de la tête (headX + headW/2), contrairement à l'ancienne
// version :
//   tête  : x=[sx+2, sx+26], largeur 24, centre à sx+14
//   cornes: gauche x=sx+2 (centre sx+4.5), droite x=sx+21 (centre sx+23.5)
//           -> 9.5px de chaque côté du centre de tête (sx+14)
//   yeux  : gauche x=sx+5 (centre sx+7), droite x=sx+19 (centre sx+21)
//           -> 7px de chaque côté du centre de tête
//   sourcil: x=sx+5, largeur 18 -> couvre exactement les deux yeux
//   plastron: largeur 12 centrée sur le corps (largeur 28) -> x=sx+8
void createBossSprites(lv_obj_t *scr)
{
  if (!bossActive)
    return;
  int sx = (int)(boss.x - cameraX), sy = (int)(boss.y);
  const int headH = 12, headW = (int)BOSS_W - 4, headX = 2;
  int bodyH = (int)BOSS_H - headH;

  boss.objBody = makeRect(scr, sx, sy - bodyH, (int)BOSS_W, bodyH, BOSS_BODY_COL);
  boss.objBelly = makeRect(scr, sx + 8, sy - bodyH + 4, 12, 14, BOSS_BELLY_COL);
  boss.objHead = makeRect(scr, sx + headX, sy - (int)BOSS_H, headW, headH, BOSS_HEAD_COL);

  boss.objHornL = makeRect(scr, sx + headX, sy - (int)BOSS_H - 6, 5, 6, BOSS_HORN_COL);
  boss.objHornR = makeRect(scr, sx + headX + headW - 5, sy - (int)BOSS_H - 6, 5, 6, BOSS_HORN_COL);

  boss.objBrow = makeRect(scr, sx + headX + 3, sy - (int)BOSS_H + 2, 18, 2, BOSS_BROW_COL);
  boss.objEyeL = makeRect(scr, sx + headX + 3, sy - (int)BOSS_H + 4, 4, 4, BOSS_EYE_COL);
  boss.objEyeR = makeRect(scr, sx + headX + headW - 7, sy - (int)BOSS_H + 4, 4, 4, BOSS_EYE_COL);
}

void triggerGameOver()
{
  objGround = nullptr;
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
  objFlagPole = nullptr;
  objFlagTop = nullptr;
  for (int i = 0; i < MAX_PLATFORMS; i++)
  {
    platObjs[i] = nullptr;
    grassCapObjs[i] = nullptr;
  }
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
  for (int k = 0; k < blockCount; k++)
  {
    blocks[k].obj = nullptr;
    blocks[k].mark = nullptr;
  }
  blockCount = 0;
  for (int k = 0; k < itemCount; k++)
    items[k].obj = nullptr;
  itemCount = 0;
  // BUG FIX : fireballs[] n'etait jamais reinitialise ici (contrairement
  // a blocks[]/items[] juste au-dessus). Si une boule de feu restait
  // marquee active a la fin du niveau, son pointeur .obj devenait pendant
  // apres le lv_obj_clean() du changement de niveau -> reutilise plus tard,
  // ca declenchait un Hard Fault (figure / freeze total).
  for (int k = 0; k < MAX_FIREBALLS; k++)
  {
    fireballs[k].obj = nullptr;
    fireballs[k].active = false;
  }
  fireballCount = 0;
  // Boss/marteaux : même raison que le BUG FIX fireballs[] juste au-dessus —
  // repartir avec un état et des pointeurs propres, qu'il y ait eu un boss
  // sur ce niveau ou non (bossActive ne redevient true que si Level4::load()
  // est rappelé ensuite, via addBoss()).
  bossActive = false;
  boss.alive = false;
  boss.objBody = boss.objBelly = boss.objHead = boss.objEyeL = boss.objEyeR = boss.objBrow = boss.objHornL = boss.objHornR = nullptr;
  for (int k = 0; k < MAX_HAMMERS; k++)
  {
    hammers[k].obj = nullptr;
    hammers[k].active = false;
  }
  hammerCount = 0;
  powerUpTimer = 0;
  fireCooldown = false;
  joyUpBufferFrames = 0;
  pipeCount = 0;
  pipeWarpTimer = 0;
  pipeWarpCooldown = 0;
  player.powerUp = POWERUP_NONE;
  // BUG FIX : avant, ces 3 lignes remettaient TOUJOURS lives/score/niveau à
  // zéro, écrasant la sauvegarde que l'appelant venait de faire juste avant
  // (score et currentLevel au moment de la mort). Le joueur perdait donc tout
  // son score et redémarrait au niveau 0 en rechargeant la partie, au lieu de
  // reprendre au niveau qu'il n'avait pas réussi avec son score. On garde
  // uniquement la réinitialisation des vies (3 vies fraîches pour retenter),
  // score et currentLevel restent ceux déjà sauvegardés par l'appelant.
  saves[activeSaveSlot].lives = 3;
  lv_obj_clean(lv_scr_act());
  currentScreen = SCREEN_MENU;
  showMenu();
}

// ── triggerVictory : déclenchée par le drapeau final UNIQUEMENT quand
// bossActive est vrai (voir le bloc DRAPEAU dans updateGame()) ───────────────
// Reprend EXACTEMENT le même nettoyage que triggerGameOver() juste au-dessus
// (mêmes pointeurs à remettre à nullptr, pour la même raison anti-Hard-Fault)
// mais termine sur un écran de victoire dédié plutôt que de retourner
// silencieusement au menu. Le score et le niveau sauvegardés sont remis à
// zéro : c'est exactement ce que faisait déjà l'ancien chemin "currentLevel
// >= 4" de nextLevel() avant l'ajout du boss (terminer le jeu = repartir
// d'une sauvegarde fraîche) ; seul l'écran affiché entre-temps change.
void triggerVictory()
{
  int finalScore = player.score;
  objGround = nullptr;
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
  objFlagPole = nullptr;
  objFlagTop = nullptr;
  for (int i = 0; i < MAX_PLATFORMS; i++)
  {
    platObjs[i] = nullptr;
    grassCapObjs[i] = nullptr;
  }
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
  for (int k = 0; k < blockCount; k++)
  {
    blocks[k].obj = nullptr;
    blocks[k].mark = nullptr;
  }
  blockCount = 0;
  for (int k = 0; k < itemCount; k++)
    items[k].obj = nullptr;
  itemCount = 0;
  for (int k = 0; k < MAX_FIREBALLS; k++)
  {
    fireballs[k].obj = nullptr;
    fireballs[k].active = false;
  }
  fireballCount = 0;
  bossActive = false;
  boss.alive = false;
  boss.objBody = boss.objBelly = boss.objHead = boss.objEyeL = boss.objEyeR = boss.objBrow = boss.objHornL = boss.objHornR = nullptr;
  for (int k = 0; k < MAX_HAMMERS; k++)
  {
    hammers[k].obj = nullptr;
    hammers[k].active = false;
  }
  hammerCount = 0;
  powerUpTimer = 0;
  fireCooldown = false;
  joyUpBufferFrames = 0;
  pipeCount = 0;
  pipeWarpTimer = 0;
  pipeWarpCooldown = 0;
  player.powerUp = POWERUP_NONE;
  saves[activeSaveSlot].score = 0;
  saves[activeSaveSlot].currentLevel = 0;
  saves[activeSaveSlot].lives = 3;
  lv_obj_clean(lv_scr_act());
  currentScreen = SCREEN_VICTORY;
  showVictory(finalScore);
}

static void victoryOkCb(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  lv_obj_clean(lv_scr_act());
  currentScreen = SCREEN_MENU;
  showMenu();
}

// ── showVictory : écran statique affiché une fois après triggerVictory() ──
// Même palette de couleurs que showMenu() (COL_BG/COL_TEXT_DARK/...) pour
// rester visuellement cohérent, et même motif de bouton unique que
// joyTestOkCb()/showJoystickTest() (un lv_button_create + lv_obj_add_event_cb
// déclenché sur LV_EVENT_CLICKED). Écran purement statique : myTask() n'a
// rien de spécial à faire pour SCREEN_VICTORY tant qu'on attend le clic,
// exactement comme SCREEN_MENU n'apparaît pas non plus dans son if/else.
void showVictory(int finalScore)
{
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "VICTOIRE !");
  lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT_DARK), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *msg = lv_label_create(scr);
  lv_label_set_text(msg, "Boss final vaincu, jeu termine !");
  lv_obj_set_style_text_color(msg, lv_color_hex(COL_TEXT_MID), 0);
  lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 100);

  lv_obj_t *scoreLbl = lv_label_create(scr);
  char buf[40];
  snprintf(buf, sizeof(buf), "Score final : %d", finalScore);
  lv_label_set_text(scoreLbl, buf);
  lv_obj_set_style_text_color(scoreLbl, lv_color_hex(COL_TEXT_DARK), 0);
  lv_obj_align(scoreLbl, LV_ALIGN_TOP_MID, 0, 140);

  lv_obj_t *btnOk = lv_button_create(scr);
  lv_obj_set_size(btnOk, 180, 44);
  lv_obj_set_pos(btnOk, (480 - 180) / 2, 200);
  lv_obj_set_style_radius(btnOk, 22, 0);
  lv_obj_set_style_bg_color(btnOk, lv_color_hex(COL_START_ON), 0);
  lv_obj_add_event_cb(btnOk, victoryOkCb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lblOk = lv_label_create(btnOk);
  lv_label_set_text(lblOk, "Retour au menu");
  lv_obj_set_style_text_color(lblOk, lv_color_hex(0xF1EFE8), 0);
  lv_obj_center(lblOk);
}

void nextLevel()
{
  saves[activeSaveSlot].score = player.score;
  saves[activeSaveSlot].lives = player.lives;
  saves[activeSaveSlot].currentLevel = currentLevel + 1;
  if (currentLevel >= 4)
  {
    saves[activeSaveSlot].currentLevel = 0;
    saves[activeSaveSlot].score = 0;
    triggerGameOver();
    return;
  }
  currentLevel++;
  objGround = nullptr;
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
  objFlagPole = nullptr;
  objFlagTop = nullptr;
  for (int i = 0; i < MAX_PLATFORMS; i++)
  {
    platObjs[i] = nullptr;
    grassCapObjs[i] = nullptr;
  }
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
  for (int k = 0; k < blockCount; k++)
  {
    blocks[k].obj = nullptr;
    blocks[k].mark = nullptr;
  }
  blockCount = 0;
  for (int k = 0; k < itemCount; k++)
    items[k].obj = nullptr;
  itemCount = 0;
  // BUG FIX : fireballs[] n'etait jamais reinitialise ici (contrairement
  // a blocks[]/items[] juste au-dessus). Si une boule de feu restait
  // marquee active a la fin du niveau, son pointeur .obj devenait pendant
  // apres le lv_obj_clean() du changement de niveau -> reutilise plus tard,
  // ca declenchait un Hard Fault (figure / freeze total).
  for (int k = 0; k < MAX_FIREBALLS; k++)
  {
    fireballs[k].obj = nullptr;
    fireballs[k].active = false;
  }
  fireballCount = 0;
  // Boss/marteaux : même raison que le BUG FIX fireballs[] juste au-dessus.
  bossActive = false;
  boss.alive = false;
  boss.objBody = boss.objBelly = boss.objHead = boss.objEyeL = boss.objEyeR = boss.objBrow = boss.objHornL = boss.objHornR = nullptr;
  for (int k = 0; k < MAX_HAMMERS; k++)
  {
    hammers[k].obj = nullptr;
    hammers[k].active = false;
  }
  hammerCount = 0;
  powerUpTimer = 0;
  fireCooldown = false;
  joyUpBufferFrames = 0;
  pipeCount = 0;
  pipeWarpTimer = 0;
  pipeWarpCooldown = 0;
  player.x = 50.0f;
  player.y = GROUND_Y;
  player.velX = 0.0f;
  player.velY = 0.0f;
  player.onGround = true;
  player.crouching = false;
  cameraX = 0.0f;
  lv_obj_clean(lv_scr_act());
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
      snprintf(buf, sizeof(buf), "%s\nNiv.%d  %d pts", cN[saves[i].characterId], saves[i].currentLevel + 1, saves[i].score);
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

static void drawPlayer(lv_obj_t *scr)
{
  const int S = 2;
  int c = player.characterId;
  uint32_t colHat = (c == CHAR_LUIGI) ? 0x388E3C : 0xE53935;
  uint32_t colPants = 0x1565C0, colShoes = 0x5D4037, colSkin = 0xFFB385;
  uint32_t colHair = (c == CHAR_TOAD) ? 0xFFFFFF : 0x5D4037;
  uint32_t colEyes = 0x212121, colNose = 0xFF8A80, colBrim = 0xEEEEEE;
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
  lblScore = makeHudLabel(scr, 8, 4, 160);
  lv_label_set_text(lblScore, "Score: 000000");
  lblLives = makeHudLabel(scr, 210, 4, 60);
  lv_label_set_text(lblLives, "x3");
  lblLevel = makeHudLabel(scr, 400, 4, 70);
  lv_label_set_text(lblLevel, "Niv.1");
  lblDebug = makeHudLabel(scr, 8, SCREEN_H - 16, 140);
  lv_label_set_text(lblDebug, "x:0 y:0");

  // ── Charge le niveau via LevelManager ────────────────────────────────────
  {
    Level *lvl = LevelManager::create(currentLevel);
    if (lvl)
    {
      lvl->load(scr);
      delete lvl;
    }
  }
  // ── Crée les sprites LVGL des plateformes (UNE SEULE FOIS) ────────────────
  for (int i = 0; i < platformCount; i++)
    platObjs[i] = makeRect(scr,
                           (int)(platforms[i].x - cameraX), (int)(platforms[i].y),
                           (int)platforms[i].w, (int)platforms[i].h, platforms[i].color);

  // ── Bande d'herbe verte sur chaque segment de SOL uniquement ──────────────
  // Un cap par segment (jamais sur les plateformes flottantes, grottes,
  // tubes ou marches d'escalier), et seulement là où il y a vraiment du sol :
  // contrairement à l'ancienne bande unique sur toute la largeur du monde,
  // ça ne dessine jamais d'herbe flottant au-dessus d'un trou.
  // !solid && y==GROUND_Y : seule façon de reconnaître "ceci est un segment
  // de sol" sans ajouter de champ dédié à Platform (addGround est le seul
  // appel qui pose y=GROUND_Y ; addCave/addStaircase/addPipe posent solid=
  // true à une autre hauteur, addPlatform pose solid=false à une autre
  // hauteur aussi).
  for (int i = 0; i < platformCount; i++)
  {
    if (!platforms[i].solid && platforms[i].y == GROUND_Y)
      grassCapObjs[i] = makeRect(scr,
                                 (int)(platforms[i].x - cameraX), GROUND_VISUAL_Y,
                                 (int)platforms[i].w, GRASS_CAP_H, GROUND_GRASS_COLOR);
    else
      grassCapObjs[i] = nullptr;
  }

  // ── Crée les sprites LVGL des blocs ─────────────────────────────────────
  // Type 0 = brique décorative (couleur brique, pas de "?")
  // Type 1/2/3 = bloc "?" doré avec le symbole "?" affiché par-dessus,
  // exactement comme dans le vrai Mario : carré 16x16, jaune/orange.
  for (int i = 0; i < blockCount; i++)
  {
    Block &bl = blocks[i];
    uint32_t blockColor = (bl.powerUpType == 0) ? 0xC77B3D : 0xF7B733;
    bl.obj = makeRect(scr,
                      (int)(bl.x - cameraX), (int)(bl.y),
                      16, 16, blockColor);
    if (bl.powerUpType > 0)
    {
      bl.mark = lv_label_create(scr);
      // BUG FIX (durcissement) : même raison que makeRect() — lv_label_create
      // peut renvoyer nullptr si le pool LVGL est plein.
      if (bl.mark)
      {
        lv_label_set_text(bl.mark, "?");
        lv_obj_set_style_text_color(bl.mark, lv_color_hex(0x3E2723), 0);
        lv_obj_set_style_text_font(bl.mark, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(bl.mark, (int)(bl.x - cameraX) + 4, (int)(bl.y) - 1);
      }
    }
    else
    {
      bl.mark = nullptr;
    }
  }

  // ── Marqueurs "▼" au-dessus des tubes descendables ────────────────────────
  // Le tube lui-même n'a besoin d'AUCUN sprite dédié : il est déjà visible
  // grâce aux deux Platform "solid" (chapeau + corps) créées par addPipe(),
  // rendues comme n'importe quelle autre plateforme par la boucle juste
  // au-dessus. Seuls les tubes WARP reçoivent ce petit indicateur en plus,
  // pour que le joueur sache lequel accepte la descente (joystick bas).
  for (int i = 0; i < pipeCount; i++)
  {
    if (!pipes[i].warp)
    {
      pipes[i].mark = nullptr;
      continue;
    }
    pipes[i].mark = lv_label_create(scr);
    // BUG FIX (durcissement) : idem, lv_label_create peut renvoyer nullptr.
    if (!pipes[i].mark)
      continue;
    lv_label_set_text(pipes[i].mark, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(pipes[i].mark, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(pipes[i].mark,
                   (int)(pipes[i].x + pipes[i].w / 2.0f - 6.0f - cameraX),
                   (int)(pipes[i].topY) - 16);
  }

  // Drapeau
  int fsx = (int)(flagX - cameraX);
  objFlagPole = makeRect(scr, fsx, GROUND_VISUAL_Y - FLAG_POLE_H, FLAG_POLE_W, FLAG_POLE_H, 0x9E9E9E);
  objFlagTop = makeRect(scr, fsx - FLAG_TOP_W + FLAG_POLE_W, GROUND_VISUAL_Y - FLAG_POLE_H, FLAG_TOP_W, FLAG_TOP_H, 0x2E7D32);

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
  int baseH = (player.characterId == CHAR_TOAD) ? 15 * S : 13 * S;
  int spriteH;
  if (player.powerUp == POWERUP_MUSHROOM)
    spriteH = baseH + 6 * S;
  else if (player.powerUp == POWERUP_MINI)
    spriteH = baseH - 5 * S;
  else
    spriteH = baseH;
  int screenPY = (int)(player.y);
  int legOffset = player.crouching ? 2 * S : 0;
  int by = screenPY - spriteH + legOffset, bx = screenPX, c = player.characterId;

  // ── Mise à l'échelle verticale des sprites selon le power-up ───────────────
  // baseH/spriteH/by ci-dessus changent avec le power-up, mais les décalages
  // dy de chaque partie du corps (ex: "8*S" pour les jambes) étaient codés en
  // dur pour la taille de base : ils ne suivaient pas spriteH. Résultat : en
  // champignon (spriteH > baseH) les pieds se retrouvaient dessinés au-dessus
  // de player.y -> impression de voler ; en mini (spriteH < baseH) ils se
  // retrouvaient dessous -> impression d'être enfoncé dans le sol.
  // Fix : on multiplie chaque décalage vertical par scaleY = spriteH/baseH,
  // ce qui étire/compresse le corps proportionnellement et garde toujours les
  // pieds exactement à by + spriteH = screenPY (le sol), quel que soit le
  // power-up. Les décalages horizontaux (dx) ne sont pas affectés : seule la
  // hauteur change, comme dans le design d'origine.
  float scaleY = (float)spriteH / (float)baseH;
  auto sy = [scaleY](int v)
  { return (int)(v * scaleY); };

  if (c == CHAR_MARIO || c == CHAR_LUIGI)
  {
    int lo = player.crouching ? 2 * S : 0;
    moveSprite(spHatTop, bx, by, 3 * S, sy(0 * S));
    moveSprite(spHat, bx, by, 2 * S, sy(1 * S));
    moveSprite(objHead, bx, by, 3 * S, sy(2 * S));
    moveSprite(spHair[0], bx, by, 3 * S, sy(2 * S));
    moveSprite(spHair[1], bx, by, 7 * S, sy(2 * S));
    moveSprite(spEye[0], bx, by, 4 * S, sy(3 * S));
    moveSprite(spEye[1], bx, by, 7 * S, sy(3 * S));
    moveSprite(spMustache, bx, by, 3 * S, sy(4 * S));
    moveSprite(spShirt, bx, by, 2 * S, sy(5 * S));
    moveSprite(spArm[0], bx, by, 1 * S, sy(5 * S));
    moveSprite(spArm[1], bx, by, 10 * S, sy(5 * S));
    moveSprite(spLegL, bx, by, 2 * S, sy(8 * S) - lo);
    moveSprite(spLegR, bx, by, 7 * S, sy(8 * S) - lo);
    moveSprite(spLegMid, bx, by, 4 * S, sy(8 * S) - lo);
    moveSprite(spShoeL, bx, by, 2 * S, sy(11 * S) - lo);
    moveSprite(spShoeR, bx, by, 7 * S, sy(11 * S) - lo);
  }
  else
  {
    int lo = player.crouching ? 2 * S : 0;
    moveSprite(spHatTop, bx, by, 0 * S, sy(0 * S));
    moveSprite(spHat, bx, by, 0 * S, sy(1 * S));
    moveSprite(spHatBrim, bx, by, 1 * S, sy(4 * S));
    moveSprite(spHair[0], bx, by, 1 * S, sy(1 * S));
    moveSprite(spHair[1], bx, by, 5 * S, sy(1 * S));
    moveSprite(spSpot3, bx, by, 8 * S, sy(1 * S));
    moveSprite(objHead, bx, by, 2 * S, sy(4 * S));
    moveSprite(spEye[0], bx, by, 3 * S, sy(5 * S));
    moveSprite(spEye[1], bx, by, 7 * S, sy(5 * S));
    moveSprite(spMustache, bx, by, 5 * S, sy(6 * S));
    moveSprite(spShirt, bx, by, 2 * S, sy(7 * S));
    moveSprite(spArm[0], bx, by, 1 * S, sy(7 * S));
    moveSprite(spArm[1], bx, by, 10 * S, sy(7 * S));
    moveSprite(spLegL, bx, by, 2 * S, sy(10 * S) - lo);
    moveSprite(spLegR, bx, by, 7 * S, sy(10 * S) - lo);
    moveSprite(spLegMid, bx, by, 4 * S, sy(10 * S) - lo);
    moveSprite(spShoeL, bx, by, 2 * S, sy(13 * S) - lo);
    moveSprite(spShoeR, bx, by, 7 * S, sy(13 * S) - lo);
  }

  char buf[48];
  snprintf(buf, sizeof(buf), "Score: %06d", player.score);
  lv_label_set_text(lblScore, buf);
  snprintf(buf, sizeof(buf), "x%d", player.lives);
  lv_label_set_text(lblLives, buf);
  // Affiche le compte à rebours du power-up actif dans le label niveau, quel
  // qu'il soit (avant : seule la fleur de feu avait droit à cet affichage).
  // F = Feu, C = Champignon, P = Petit (mini). +1 pour arrondir au-dessus :
  // à 26 frames restantes (juste après le passage sous le seuil des 25, donc
  // au tout début de la 2e seconde), /25=1, +1=2 → on affiche encore "2"
  // tant qu'il reste plus d'une seconde pleine, comme un compte à rebours
  // normal (jamais "0" tant que l'effet est actif).
  if (player.powerUp != POWERUP_NONE && powerUpTimer > 0)
  {
    char tag = (player.powerUp == POWERUP_FIRE) ? 'F' : (player.powerUp == POWERUP_MUSHROOM) ? 'C'
                                                                                             : 'P';
    snprintf(buf, sizeof(buf), "Niv.%d [%c%d]", currentLevel + 1, tag, powerUpTimer / 25 + 1);
  }
  else
    snprintf(buf, sizeof(buf), "Niv.%d", currentLevel + 1);
  lv_label_set_text(lblLevel, buf);
  snprintf(buf, sizeof(buf), "x:%.0f y:%.0f", player.x, player.y);
  lv_label_set_text(lblDebug, buf);

  for (int i = 0; i < platformCount; i++)
  {
    if (platObjs[i])
      lv_obj_set_pos(platObjs[i], (int)(platforms[i].x - cameraX), (int)(platforms[i].y));
    if (grassCapObjs[i])
      lv_obj_set_pos(grassCapObjs[i], (int)(platforms[i].x - cameraX), GROUND_VISUAL_Y);
  }

  // Repositionne les marqueurs "▼" des tubes warp
  for (int i = 0; i < pipeCount; i++)
    if (pipes[i].mark)
      lv_obj_set_pos(pipes[i].mark,
                     (int)(pipes[i].x + pipes[i].w / 2.0f - 6.0f - cameraX),
                     (int)(pipes[i].topY) - 16);

  // Repositionne les blocs et leur label "?"
  for (int i = 0; i < blockCount; i++)
  {
    if (blocks[i].obj)
      lv_obj_set_pos(blocks[i].obj, (int)(blocks[i].x - cameraX), (int)(blocks[i].y));
    if (blocks[i].mark)
      lv_obj_set_pos(blocks[i].mark, (int)(blocks[i].x - cameraX) + 4, (int)(blocks[i].y) - 1);
  }

  // Repositionne les items au sol
  for (int i = 0; i < itemCount; i++)
    if (items[i].active && items[i].obj)
      lv_obj_set_pos(items[i].obj, (int)(items[i].x - cameraX), (int)(items[i].y));

  // Repositionne les boules de feu
  for (int i = 0; i < fireballCount; i++)
    if (fireballs[i].active && fireballs[i].obj)
      lv_obj_set_pos(fireballs[i].obj, (int)(fireballs[i].x - cameraX), (int)(fireballs[i].y));

  const int GS = 2;
  for (int i = 0; i < goombaCount; i++)
  {
    Goomba &g = goombas[i];
    if (!g.alive)
      continue;
    int gsx = (int)(g.x - cameraX), gsy = (int)(g.y);
    if (g.objHat)
      lv_obj_set_pos(g.objHat, gsx, gsy - (int)GOOMBA_H);
    if (g.objBody)
      lv_obj_set_pos(g.objBody, gsx, gsy - (int)GOOMBA_H + 2 * GS);
    if (g.objFeetL)
      lv_obj_set_pos(g.objFeetL, gsx, gsy - 2 * GS);
    if (g.objFeetR)
      lv_obj_set_pos(g.objFeetR, gsx + 4 * GS, gsy - 2 * GS);
  }
  if (objFlagPole)
    lv_obj_set_pos(objFlagPole, (int)(flagX - cameraX), GROUND_VISUAL_Y - FLAG_POLE_H);
  if (objFlagTop)
    lv_obj_set_pos(objFlagTop, (int)(flagX - cameraX) - FLAG_TOP_W + FLAG_POLE_W, GROUND_VISUAL_Y - FLAG_POLE_H);

  // Repositionne le boss (mêmes 8 rectangles que createBossSprites()) et ses
  // marteaux en vol. Ne fait rien si bossActive est resté false ou si le
  // boss est déjà vaincu (boss.alive==false ; ses sprites ont alors déjà été
  // cachés une fois pour toutes par damageBoss(), pas besoin de les bouger).
  if (bossActive && boss.alive)
  {
    int bsx = (int)(boss.x - cameraX), bsy = (int)(boss.y);
    const int headH = 12, headW = (int)BOSS_W - 4, headX = 2;
    const int bodyH = (int)BOSS_H - headH;
    if (boss.objBody)
      lv_obj_set_pos(boss.objBody, bsx, bsy - bodyH);
    if (boss.objBelly)
      lv_obj_set_pos(boss.objBelly, bsx + 8, bsy - bodyH + 4);
    if (boss.objHead)
      lv_obj_set_pos(boss.objHead, bsx + headX, bsy - (int)BOSS_H);
    if (boss.objHornL)
      lv_obj_set_pos(boss.objHornL, bsx + headX, bsy - (int)BOSS_H - 6);
    if (boss.objHornR)
      lv_obj_set_pos(boss.objHornR, bsx + headX + headW - 5, bsy - (int)BOSS_H - 6);
    if (boss.objBrow)
      lv_obj_set_pos(boss.objBrow, bsx + headX + 3, bsy - (int)BOSS_H + 2);
    if (boss.objEyeL)
      lv_obj_set_pos(boss.objEyeL, bsx + headX + 3, bsy - (int)BOSS_H + 4);
    if (boss.objEyeR)
      lv_obj_set_pos(boss.objEyeR, bsx + headX + headW - 7, bsy - (int)BOSS_H + 4);
  }
  for (int i = 0; i < hammerCount; i++)
    if (hammers[i].active && hammers[i].obj)
      lv_obj_set_pos(hammers[i].obj, (int)(hammers[i].x - cameraX), (int)(hammers[i].y));
}

void showGame()
{
  lv_obj_t *scr = lv_scr_act();
  cameraX = 0.0f;
  objSky = nullptr;
  objGround = nullptr;
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
  objFlagPole = nullptr;
  objFlagTop = nullptr;
  for (int i = 0; i < MAX_PLATFORMS; i++)
  {
    platObjs[i] = nullptr;
    grassCapObjs[i] = nullptr;
  }
  platformCount = 0;
  for (int i = 0; i < MAX_GOOMBAS; i++)
  {
    goombas[i].alive = false;
    goombas[i].objHat = nullptr;
    goombas[i].objBody = nullptr;
    goombas[i].objFeetL = nullptr;
    goombas[i].objFeetR = nullptr;
  }
  goombaCount = 0;
  for (int k = 0; k < blockCount; k++)
  {
    blocks[k].obj = nullptr;
    blocks[k].mark = nullptr;
  }
  blockCount = 0;
  for (int k = 0; k < itemCount; k++)
    items[k].obj = nullptr;
  itemCount = 0;
  // BUG FIX : fireballs[] n'etait jamais reinitialise ici (contrairement
  // a blocks[]/items[] juste au-dessus). Si une boule de feu restait
  // marquee active a la fin du niveau, son pointeur .obj devenait pendant
  // apres le lv_obj_clean() du changement de niveau -> reutilise plus tard,
  // ca declenchait un Hard Fault (figure / freeze total).
  for (int k = 0; k < MAX_FIREBALLS; k++)
  {
    fireballs[k].obj = nullptr;
    fireballs[k].active = false;
  }
  fireballCount = 0;
  // Boss/marteaux : même raison que le BUG FIX fireballs[] juste au-dessus.
  // bossActive ne repasse à true que si le niveau chargé ensuite par
  // initGameObjects() -> Level::load() appelle addBoss() (seul Level4 le
  // fait) — donc pour tous les autres niveaux, ce bloc garantit qu'aucun
  // combat de boss "fantôme" d'un niveau précédent ne reste actif.
  bossActive = false;
  boss.alive = false;
  boss.objBody = boss.objBelly = boss.objHead = boss.objEyeL = boss.objEyeR = boss.objBrow = boss.objHornL = boss.objHornR = nullptr;
  for (int k = 0; k < MAX_HAMMERS; k++)
  {
    hammers[k].obj = nullptr;
    hammers[k].active = false;
  }
  hammerCount = 0;
  powerUpTimer = 0;
  fireCooldown = false;
  joyUpBufferFrames = 0;
  for (int k = 0; k < MAX_PIPES; k++)
    pipes[k].mark = nullptr;
  pipeCount = 0;
  pipeWarpTimer = 0;
  pipeWarpCooldown = 0;
  initGameObjects(scr);
}

void mySetup()
{
  initInputs();
  initSaves();
  showJoystickTest();
}

void loop() {}

void myTask(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
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
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(40));
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