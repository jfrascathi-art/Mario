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
  if (code == LV_EVENT_CLICKED) { LV_LOG_USER("Clicked"); }
  else if (code == LV_EVENT_VALUE_CHANGED) { LV_LOG_USER("Toggled"); }
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
  int joyX; int joyY;
  bool buttonJump; bool buttonDown;
  bool isLeft()  const { return joyX < JOY_CENTER - JOY_DEADZONE; }
  bool isRight() const { return joyX > JOY_CENTER + JOY_DEADZONE; }
  bool isUp()    const { return joyY < JOY_CENTER - JOY_DEADZONE; }
  bool isDown()  const { return joyY > JOY_CENTER + JOY_DEADZONE; }
  float normalizedX() const {
    const float MAX_RANGE = 65.0f;
    if (isLeft())  return -(float)(JOY_CENTER-JOY_DEADZONE-joyX)/MAX_RANGE;
    if (isRight()) return  (float)(joyX-JOY_CENTER-JOY_DEADZONE)/MAX_RANGE;
    return 0.0f;
  }
  float normalizedY() const {
    if (isUp())   return -(float)(JOY_CENTER-JOY_DEADZONE-joyY)/(float)(JOY_CENTER-JOY_DEADZONE);
    if (isDown()) return  (float)(joyY-JOY_CENTER-JOY_DEADZONE)/(float)(JOY_MAX-JOY_CENTER-JOY_DEADZONE);
    return 0.0f;
  }
};
void initInputs(); void updateInputs();
InputState inputs;
void initInputs() {
  pinMode(BUTTON_JUMP,INPUT_PULLUP); pinMode(BUTTON_DOWN,INPUT_PULLUP);
  pinMode(A0,INPUT); pinMode(A1,INPUT);
}
void updateInputs() {
  inputs.joyX=analogRead(JOY_X);
  inputs.joyY=1023-analogRead(JOY_Y);
  inputs.buttonJump=!digitalRead(BUTTON_JUMP);
  inputs.buttonDown=!digitalRead(BUTTON_DOWN);
}
InputState getInputs() { return inputs; }
InputState getInputs();

static lv_obj_t *joyCursor=nullptr, *joyCircle=nullptr;
static lv_obj_t *labelRaw=nullptr,  *labelNorm=nullptr;
static lv_obj_t *labelDir=nullptr,  *labelButtons=nullptr;

struct SaveSlot { bool used; int characterId,currentLevel,score,lives; char name[12]; };
#define MAX_SAVES 3
SaveSlot saves[MAX_SAVES];
enum PowerUp     { POWERUP_NONE=0,POWERUP_MUSHROOM=1,POWERUP_FIRE=2,POWERUP_MINI=3 };
enum CharacterId { CHAR_MARIO=0,CHAR_LUIGI=1,CHAR_TOAD=2 };
struct Player {
  float x,y,velX,velY;
  bool onGround,isAlive,crouching;
  PowerUp powerUp;
  int lives,score,characterId;
};
Player player;
int currentLevel=0, activeSaveSlot=0;

#define GRAVITY       0.5f
#define WALK_SPEED    2.0f  // réduit (était 3.0) pour moins de vitesse horizontale
#define JUMP_VELOCITY -8.0f // légèrement augmenté pour compenser la vitesse réduite
#define GROUND_Y    200.0f
#define SCREEN_W    480
#define SCREEN_H    272
#define PLAYER_W     16
#define PLAYER_H     24
#define GROUND_VISUAL_Y 200

// ── ÉTAPE 7 : WORLD_W agrandi pour des niveaux plus longs ────────────────────
// Les niveaux vont jusqu'à ~3000px de large pour avoir plus d'espace.
#define WORLD_W 3200

static float cameraX = 0.0f;

// Déclarations anticipées
void triggerGameOver();
void showMenu();
void nextLevel(); // ← ÉTAPE 7 : passage au niveau suivant

struct Platform {
  float x,y,w,h;
  uint32_t color;
  // isCave=true → bloc de roche (grotte). Bloque aussi latéralement debout.
  // Accroupi : hitbox réduite → le joueur peut entrer par le côté.
  bool isCave;
};

// ── ÉTAPE 7 : MAX_PLATFORMS agrandi (plus de dalles par niveau long) ─────────
#define MAX_PLATFORMS 32
Platform platforms[MAX_PLATFORMS];
int platformCount=0;
static lv_obj_t *platObjs[MAX_PLATFORMS]={};

#define MAX_GOOMBAS  12   // plus de Goombas possible sur les niveaux durs
#define GOOMBA_W   14.0f
#define GOOMBA_H   16.0f
#define GOOMBA_SPEED 1.2f // vitesse de base (multipliée selon le niveau)

struct Goomba {
  float x,y,velX;
  bool alive;
  float patrolLeft,patrolRight;
  lv_obj_t *objHat,*objBody,*objFeetL,*objFeetR;
};
Goomba goombas[MAX_GOOMBAS];
int goombaCount=0;

// ── ÉTAPE 7 : Drapeau de fin de niveau ───────────────────────────────────────
// Le drapeau = une barre grise verticale + un carré vert en haut.
// flagX = position X monde du drapeau (définie dans loadLevelPlatforms).
// Quand le joueur touche la zone du drapeau → nextLevel().
static float flagX = 0.0f;          // position monde X du drapeau
static lv_obj_t *objFlagPole = nullptr; // barre grise
static lv_obj_t *objFlagTop  = nullptr; // carré vert en haut
// Dimensions du drapeau
#define FLAG_POLE_W  6
#define FLAG_POLE_H  60
#define FLAG_TOP_W   20
#define FLAG_TOP_H   14
// ── FIN ÉTAPE 7 : Drapeau ────────────────────────────────────────────────────

void initSaves() {
  for(int i=0;i<MAX_SAVES;i++){
    saves[i].used=false; saves[i].characterId=CHAR_MARIO;
    saves[i].currentLevel=0; saves[i].score=0; saves[i].lives=3;
    saves[i].name[0]='P'; saves[i].name[1]='a'; saves[i].name[2]='r';
    saves[i].name[3]='t'; saves[i].name[4]='i'; saves[i].name[5]='e';
    saves[i].name[6]=' '; saves[i].name[7]='1'+i; saves[i].name[8]='\0';
  }
}
void loadSave(int idx){
  activeSaveSlot=idx; currentLevel=saves[idx].currentLevel;
  player.score=saves[idx].score; player.lives=saves[idx].lives;
  player.characterId=saves[idx].characterId; player.powerUp=POWERUP_NONE;
  player.isAlive=true; player.x=50.0f; player.y=GROUND_Y;
  player.velX=0.0f; player.velY=0.0f; player.onGround=true; player.crouching=false;
}
void newGame(int idx,int charId){
  activeSaveSlot=idx; saves[idx].used=true; saves[idx].characterId=charId;
  saves[idx].currentLevel=0; saves[idx].score=0; saves[idx].lives=3;
  loadSave(idx);
}
void saveCurrentGame(){
  saves[activeSaveSlot].used=true; saves[activeSaveSlot].currentLevel=currentLevel;
  saves[activeSaveSlot].score=player.score; saves[activeSaveSlot].lives=player.lives;
  saves[activeSaveSlot].characterId=player.characterId;
}

void updateGame(InputState &in)
{
  player.crouching = in.buttonDown && player.onGround;
  float speedMult = player.crouching ? 0.5f : 1.0f;
  player.velX = in.normalizedX()*WALK_SPEED*speedMult;
  if(in.buttonJump && player.onGround && !player.crouching){
    player.velY=JUMP_VELOCITY; player.onGround=false;
  }
  player.velY+=GRAVITY;
  player.x+=player.velX; player.y+=player.velY;

  const float PW=16.0f, PH=26.0f;
  float py2_avant=player.y-player.velY;
  // Hauteur effective selon accroupissement
  float PH_eff = player.crouching ? 22.0f : PH;

  for(int i=0;i<platformCount;i++){
    Platform &p=platforms[i];
    float px1=player.x, px2=player.x+PW;
    float py2=player.y, py1=player.y-PH_eff;
    float plx1=p.x, plx2=p.x+p.w;
    float ply1=p.y, ply2=p.y+p.h;

    bool overlapX=(px2>plx1)&&(px1<plx2);

    // ── Collision verticale ───────────────────────────────────────────────
    if(overlapX){
      float py2_av = player.y - player.velY;
      float py1_av = py2_av - PH_eff;

      // Atterrissage : pieds traversent le haut
      // crossedTop : traversée franche | skinnedTop : plateforme fine (12px)
      bool crossedTop = (py2_av <= ply1) && (py2 >= ply1);
      bool skinnedTop = (py2 > ply1) && (py2 <= ply1+6.0f) && (py2_av <= ply1+2.0f);
      if((crossedTop || skinnedTop) && player.velY >= 0){
        player.y=ply1; player.velY=0.0f; player.onGround=true;
      }
      // Plafond : tête traverse le bas
      else if(py1_av >= ply2 && py1 <= ply2 && player.velY < 0){
        player.y=ply2+PH_eff; player.velY=0.0f;
      }
    }

    // ── Collision latérale pour les grottes ──────────────────────────────
    // Un bloc isCave descend du haut de l'écran (y=0) jusqu'à CAVE_H.
    // Si le joueur DEBOUT chevauche verticalement le bloc → il est bloqué.
    // Si le joueur est ACCROUPI → PH_eff=22 < CAVE_H → py1=y-22 ne touche
    //   plus le bas du bloc → overlapY=false → il passe librement.
    if(p.isCave){
      bool overlapY=(py2>ply1)&&(py1<ply2);
      if(overlapY){
        float px1_avant = player.x - player.velX;
        float px2_avant = px1_avant + PW;
        if(px2_avant <= plx1 && px2 > plx1){
          player.x = plx1 - PW; player.velX = 0.0f;
        }
        else if(px1_avant >= plx2 && px1 < plx2){
          player.x = plx2; player.velX = 0.0f;
        }
      }
    }
  }

  // ── ÉTAPE 7 : détection drapeau ──────────────────────────────────────────
  // Zone de contact : largeur FLAG_POLE_W + 8px de marge de chaque côté.
  // Quand le joueur centre-x entre dans cette zone → niveau suivant.
  float pcx = player.x + PW*0.5f; // centre X du joueur
  if(pcx >= flagX - 8 && pcx <= flagX + FLAG_POLE_W + 8 &&
     player.y >= GROUND_Y - PH - 10)
  {
    nextLevel();
    return;
  }
  // ── FIN ÉTAPE 7 : drapeau ────────────────────────────────────────────────

  if(player.y>SCREEN_H+30){
    player.lives--;
    if(player.lives<0) player.lives=0;
    if(player.lives==0){ triggerGameOver(); return; }
    player.x=50.0f; player.y=GROUND_Y;
    player.velX=0.0f; player.velY=0.0f;
    player.onGround=true; player.crouching=false; cameraX=0.0f;
  }
  if(player.x<0.0f) player.x=0.0f;
  if(player.x>WORLD_W-16.0f) player.x=WORLD_W-16.0f;

  // ── ÉTAPE 7 : vitesse Goombas selon niveau ────────────────────────────────
  // Niveaux 0-1 : vitesse normale | 2 : ×1.5 | 3-4 : ×2.2
  float gSpeed = GOOMBA_SPEED;
  if(currentLevel==2) gSpeed = GOOMBA_SPEED*1.5f;
  else if(currentLevel>=3) gSpeed = GOOMBA_SPEED*2.2f;
  // ── FIN ÉTAPE 7 ──────────────────────────────────────────────────────────

  for(int i=0;i<goombaCount;i++){
    Goomba &g=goombas[i];
    if(!g.alive) continue;
    g.x+=g.velX;
    // Demi-tour : on maintient la vitesse courante mais on ajuste la direction
    if(g.x<=g.patrolLeft) { g.x=g.patrolLeft; g.velX= gSpeed; }
    if(g.x+GOOMBA_W>=g.patrolRight){ g.x=g.patrolRight-GOOMBA_W; g.velX=-gSpeed; }

    float gx1=g.x,gx2=g.x+GOOMBA_W,gy1=g.y-GOOMBA_H,gy2=g.y;
    float px1=player.x,px2=player.x+PW,py1=player.y-PH,py2_now=player.y;
    bool oX=(px2>gx1)&&(px1<gx2), oY=(py2_now>gy1)&&(py1<gy2);
    if(oX&&oY){
      bool stomped=(py2_avant<=gy1)&&(py2_now>=gy1)&&(player.velY>0);
      if(stomped){
        g.alive=false;
        if(g.objHat)   lv_obj_add_flag(g.objHat,  LV_OBJ_FLAG_HIDDEN);
        if(g.objBody)  lv_obj_add_flag(g.objBody, LV_OBJ_FLAG_HIDDEN);
        if(g.objFeetL) lv_obj_add_flag(g.objFeetL,LV_OBJ_FLAG_HIDDEN);
        if(g.objFeetR) lv_obj_add_flag(g.objFeetR,LV_OBJ_FLAG_HIDDEN);
        player.velY=JUMP_VELOCITY*0.5f; player.onGround=false;
        player.score+=100;
      } else {
        player.lives--;
        if(player.lives<0) player.lives=0;
        if(player.lives==0){ triggerGameOver(); return; }
        player.x=50.0f; player.y=GROUND_Y;
        player.velX=0.0f; player.velY=0.0f;
        player.onGround=true; cameraX=0.0f;
      }
    }
  }
}

#define JOY_CIRCLE_R 90
#define JOY_CURSOR_R 8
void showMenu();
static void joyTestOkCb(lv_event_t *e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
  joyCursor=nullptr;joyCircle=nullptr;
  labelRaw=nullptr;labelNorm=nullptr;labelDir=nullptr;labelButtons=nullptr;
  lv_obj_clean(lv_scr_act()); currentScreen=SCREEN_MENU; showMenu();
}
void showJoystickTest(){
  lv_obj_t *scr=lv_scr_act();
  lv_obj_t *title=lv_label_create(scr); lv_label_set_text(title,"Test joystick");
  lv_obj_align(title,LV_ALIGN_TOP_MID,0,6);
  joyCircle=lv_obj_create(scr);
  lv_obj_set_size(joyCircle,JOY_CIRCLE_R*2,JOY_CIRCLE_R*2);
  lv_obj_set_style_radius(joyCircle,LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_bg_color(joyCircle,lv_color_hex(0xCCCCCC),0);
  lv_obj_set_style_bg_opa(joyCircle,LV_OPA_40,0);
  lv_obj_set_style_border_color(joyCircle,lv_color_hex(0x888888),0);
  lv_obj_set_style_border_width(joyCircle,2,0);
  lv_obj_set_style_pad_all(joyCircle,0,0);
  lv_obj_align(joyCircle,LV_ALIGN_LEFT_MID,10,10);
  int dzPx=(int)(((float)JOY_DEADZONE/((float)JOY_MAX/2.0f))*JOY_CIRCLE_R);
  lv_obj_t *dzCircle=lv_obj_create(scr);
  lv_obj_set_size(dzCircle,dzPx*2,dzPx*2);
  lv_obj_set_style_radius(dzCircle,LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_bg_color(dzCircle,lv_color_hex(0x999999),0);
  lv_obj_set_style_bg_opa(dzCircle,LV_OPA_50,0);
  lv_obj_set_style_border_width(dzCircle,1,0);
  lv_obj_set_style_border_color(dzCircle,lv_color_hex(0x555555),0);
  lv_obj_set_style_pad_all(dzCircle,0,0);
  lv_obj_align_to(dzCircle,joyCircle,LV_ALIGN_CENTER,0,0);
  joyCursor=lv_obj_create(scr);
  lv_obj_set_size(joyCursor,JOY_CURSOR_R*2,JOY_CURSOR_R*2);
  lv_obj_set_style_radius(joyCursor,LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_bg_color(joyCursor,lv_color_hex(0x1D9E75),0);
  lv_obj_set_style_border_width(joyCursor,0,0);
  lv_obj_set_style_pad_all(joyCursor,0,0);
  lv_obj_align_to(joyCursor,joyCircle,LV_ALIGN_CENTER,0,0);
  labelRaw=lv_label_create(scr); lv_label_set_text(labelRaw,"X:  512\nY:  512");
  lv_obj_align(labelRaw,LV_ALIGN_LEFT_MID,220,-60);
  labelNorm=lv_label_create(scr); lv_label_set_text(labelNorm,"nX: 0.00\nnY: 0.00");
  lv_obj_align(labelNorm,LV_ALIGN_LEFT_MID,220,0);
  labelDir=lv_label_create(scr); lv_label_set_text(labelDir,"Zone morte");
  lv_obj_align(labelDir,LV_ALIGN_LEFT_MID,220,60);
  labelButtons=lv_label_create(scr); lv_label_set_text(labelButtons,"JUMP:0  DOWN:0");
  lv_obj_align(labelButtons,LV_ALIGN_LEFT_MID,220,90);
  lv_obj_t *btnOk=lv_button_create(scr);
  lv_obj_align(btnOk,LV_ALIGN_BOTTOM_RIGHT,-10,-10);
  lv_obj_add_event_cb(btnOk,joyTestOkCb,LV_EVENT_ALL,NULL);
  lv_obj_t *lblOk=lv_label_create(btnOk);
  lv_label_set_text(lblOk,"OK -> menu"); lv_obj_center(lblOk);
}
void updateJoystickTest(InputState &in){
  if(!joyCursor||!joyCircle) return;
  int ox=(int)(in.normalizedX()*JOY_CIRCLE_R);
  int oy=(int)(in.normalizedY()*JOY_CIRCLE_R);
  lv_obj_align_to(joyCursor,joyCircle,LV_ALIGN_CENTER,ox,oy);
  lv_obj_set_style_bg_color(joyCursor,lv_color_hex(ox==0&&oy==0?0x999999:0x1D9E75),0);
  char buf[48];
  snprintf(buf,sizeof(buf),"X:  %4d\nY:  %4d",in.joyX,in.joyY);
  lv_label_set_text(labelRaw,buf);
  snprintf(buf,sizeof(buf),"nX: %+.2f\nnY: %+.2f",in.normalizedX(),in.normalizedY());
  lv_label_set_text(labelNorm,buf);
  if(ox==0&&oy==0){ lv_label_set_text(labelDir,"Zone morte"); }
  else {
    char dir[32]="";
    if(in.isLeft())  strcat(dir,"GAUCHE ");
    if(in.isRight()) strcat(dir,"DROITE ");
    if(in.isUp())    strcat(dir,"HAUT ");
    if(in.isDown())  strcat(dir,"BAS ");
    lv_label_set_text(labelDir,dir);
  }
  snprintf(buf,sizeof(buf),"JUMP:%d  DOWN:%d",(int)in.buttonJump,(int)in.buttonDown);
  lv_label_set_text(labelButtons,buf);
}

#define COL_BG         0xD6F0E8
#define COL_SLOT_IDLE  0xC8EAE0
#define COL_SLOT_SEL   0x5DCAA5
#define COL_SLOT_BORDER 0x9FE1CB
#define COL_TEXT_DARK  0x085041
#define COL_TEXT_MID   0x0F6E56
#define COL_MARIO_IDLE 0xFFD6D6
#define COL_MARIO_SEL  0xF09595
#define COL_LUIGI_IDLE 0xD6F0DD
#define COL_LUIGI_SEL  0x97C459
#define COL_TOAD_IDLE  0xD6E8FF
#define COL_TOAD_SEL   0x85B7EB
#define COL_START_OFF  0xB4B2A9
#define COL_START_ON   0x1D9E75

static int menuSelectedSlot=-1, menuSelectedChar=CHAR_MARIO;
static bool menuIsNewGame=false;
static lv_obj_t *btnStart=nullptr, *charPanel=nullptr;
static lv_obj_t *slotBtns[MAX_SAVES]={nullptr,nullptr,nullptr};
static lv_obj_t *charBtns[3]={nullptr,nullptr,nullptr};
static const char *charNames[3]={"Mario","Luigi","Toad"};
static const uint32_t charColIdle[3]={COL_MARIO_IDLE,COL_LUIGI_IDLE,COL_TOAD_IDLE};
static const uint32_t charColSel[3]={COL_MARIO_SEL,COL_LUIGI_SEL,COL_TOAD_SEL};
static void styleSlotBtn(lv_obj_t *btn,bool sel){
  lv_obj_set_style_bg_color(btn,lv_color_hex(sel?COL_SLOT_SEL:COL_SLOT_IDLE),0);
  lv_obj_set_style_border_color(btn,lv_color_hex(sel?COL_TEXT_DARK:COL_SLOT_BORDER),0);
  lv_obj_set_style_border_width(btn,2,0);
}
void showGame();
void renderGame();
static void slotBtnCb(lv_event_t *e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
  menuSelectedSlot=(int)(intptr_t)lv_event_get_user_data(e);
  menuIsNewGame=!saves[menuSelectedSlot].used;
  for(int i=0;i<MAX_SAVES;i++) if(slotBtns[i]) styleSlotBtn(slotBtns[i],i==menuSelectedSlot);
  if(menuIsNewGame) lv_obj_clear_flag(charPanel,LV_OBJ_FLAG_HIDDEN);
  else              lv_obj_add_flag(charPanel,LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(btnStart,lv_color_hex(COL_START_ON),0);
  lv_obj_add_flag(btnStart,LV_OBJ_FLAG_CLICKABLE);
}
static void charBtnCb(lv_event_t *e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
  menuSelectedChar=(int)(intptr_t)lv_event_get_user_data(e);
  for(int i=0;i<3;i++) if(charBtns[i])
    lv_obj_set_style_bg_color(charBtns[i],
      lv_color_hex(i==menuSelectedChar?charColSel[i]:charColIdle[i]),0);
}
static void startBtnCb(lv_event_t *e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
  if(menuSelectedSlot<0) return;
  btnStart=nullptr; charPanel=nullptr;
  for(int i=0;i<MAX_SAVES;i++) slotBtns[i]=nullptr;
  for(int i=0;i<3;i++) charBtns[i]=nullptr;
  if(menuIsNewGame) newGame(menuSelectedSlot,menuSelectedChar);
  else              loadSave(menuSelectedSlot);
  lv_obj_clean(lv_scr_act());
  currentScreen=SCREEN_GAME; showGame();
}

#define SKY_COLOR    0xB3E5FC
#define GROUND_COLOR 0x4A7C3F
#define GRASS_COLOR  0x66BB6A
#define GOOMBA_HAT_COL  0x4E342E
#define GOOMBA_BODY_COL 0x795548
#define GOOMBA_FEET_COL 0xA1887F

static const uint32_t playerColors[3]={0xE53935,0x43A047,0x1E88E5};

static lv_obj_t *objSky=nullptr, *objGround=nullptr, *objGrass=nullptr;
static lv_obj_t *objPlayer=nullptr, *objHead=nullptr;
static lv_obj_t *lblScore=nullptr, *lblLives=nullptr, *lblLevel=nullptr, *lblDebug=nullptr;
static lv_obj_t *spHat=nullptr, *spHatTop=nullptr, *spHatBrim=nullptr;
static lv_obj_t *spHair[2]={nullptr,nullptr}, *spSpot3=nullptr;
static lv_obj_t *spEye[2]={nullptr,nullptr};
static lv_obj_t *spMustache=nullptr, *spShirt=nullptr;
static lv_obj_t *spArm[2]={nullptr,nullptr};
static lv_obj_t *spLegL=nullptr, *spLegR=nullptr, *spLegMid=nullptr;
static lv_obj_t *spShoeL=nullptr, *spShoeR=nullptr;

static lv_obj_t *makeRect(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color){
  lv_obj_t *obj=lv_obj_create(parent);
  lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h);
  lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
  lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(obj,0,0);
  lv_obj_set_style_pad_all(obj,0,0);
  lv_obj_set_style_radius(obj,0,0);
  lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(obj,LV_OBJ_FLAG_CLICKABLE);
  return obj;
}
static lv_obj_t *makeHudLabel(lv_obj_t *parent,int x,int y,int w){
  lv_obj_t *lbl=lv_label_create(parent);
  lv_obj_set_pos(lbl,x,y); lv_obj_set_width(lbl,w);
  lv_obj_set_style_text_color(lbl,lv_color_hex(0x212121),0);
  lv_obj_set_style_bg_opa(lbl,LV_OPA_TRANSP,0);
  return lbl;
}
static void moveSprite(lv_obj_t *obj,int bx,int by,int dx,int dy){
  if(obj) lv_obj_set_pos(obj,bx+dx,by+dy);
}

// ── triggerGameOver ───────────────────────────────────────────────────────────
void triggerGameOver(){
  objGround=nullptr; objGrass=nullptr; objPlayer=nullptr; objHead=nullptr;
  lblScore=nullptr; lblLives=nullptr; lblLevel=nullptr; lblDebug=nullptr;
  spHat=nullptr; spHatTop=nullptr; spHatBrim=nullptr;
  spHair[0]=nullptr; spHair[1]=nullptr; spSpot3=nullptr;
  spEye[0]=nullptr; spEye[1]=nullptr;
  spMustache=nullptr; spShirt=nullptr;
  spArm[0]=nullptr; spArm[1]=nullptr;
  spLegL=nullptr; spLegR=nullptr; spLegMid=nullptr;
  spShoeL=nullptr; spShoeR=nullptr;
  objFlagPole=nullptr; objFlagTop=nullptr;
  for(int i=0;i<MAX_PLATFORMS;i++) platObjs[i]=nullptr;
  platformCount=0;
  for(int j=0;j<MAX_GOOMBAS;j++){
    goombas[j].alive=false;
    goombas[j].objHat=nullptr; goombas[j].objBody=nullptr;
    goombas[j].objFeetL=nullptr; goombas[j].objFeetR=nullptr;
  }
  goombaCount=0;
  saves[activeSaveSlot].lives=3; saves[activeSaveSlot].score=0;
  saves[activeSaveSlot].currentLevel=0;
  lv_obj_clean(lv_scr_act());
  currentScreen=SCREEN_MENU; showMenu();
}

// ── ÉTAPE 7 : nextLevel ───────────────────────────────────────────────────────
// Appelée quand le joueur touche le drapeau.
// Si on était au dernier niveau (4) → retour au menu (victoire).
// Sinon → on incrémente currentLevel et on recharge le jeu.
void nextLevel(){
  // Sauvegarde le score avant de changer de niveau
  saves[activeSaveSlot].score        = player.score;
  saves[activeSaveSlot].lives        = player.lives;
  saves[activeSaveSlot].currentLevel = currentLevel + 1;

  if(currentLevel >= 4){
    // Victoire : retour au menu
    saves[activeSaveSlot].currentLevel = 0;
    saves[activeSaveSlot].score = 0;
    triggerGameOver(); // réutilise la même logique de nettoyage + menu
    return;
  }
  currentLevel++;

  // Remet les pointeurs à zéro (même logique que showGame)
  objGround=nullptr; objGrass=nullptr; objPlayer=nullptr; objHead=nullptr;
  lblScore=nullptr; lblLives=nullptr; lblLevel=nullptr; lblDebug=nullptr;
  spHat=nullptr; spHatTop=nullptr; spHatBrim=nullptr;
  spHair[0]=nullptr; spHair[1]=nullptr; spSpot3=nullptr;
  spEye[0]=nullptr; spEye[1]=nullptr;
  spMustache=nullptr; spShirt=nullptr;
  spArm[0]=nullptr; spArm[1]=nullptr;
  spLegL=nullptr; spLegR=nullptr; spLegMid=nullptr;
  spShoeL=nullptr; spShoeR=nullptr;
  objFlagPole=nullptr; objFlagTop=nullptr;
  for(int i=0;i<MAX_PLATFORMS;i++) platObjs[i]=nullptr;
  platformCount=0;
  for(int j=0;j<MAX_GOOMBAS;j++){
    goombas[j].alive=false;
    goombas[j].objHat=nullptr; goombas[j].objBody=nullptr;
    goombas[j].objFeetL=nullptr; goombas[j].objFeetR=nullptr;
  }
  goombaCount=0;

  // Repositionne le joueur au début du nouveau niveau
  player.x=50.0f; player.y=GROUND_Y;
  player.velX=0.0f; player.velY=0.0f;
  player.onGround=true; player.crouching=false;
  cameraX=0.0f;

  lv_obj_clean(lv_scr_act());
  showGame();
}
// ── FIN ÉTAPE 7 : nextLevel ───────────────────────────────────────────────────

void showMenu(){
  menuSelectedSlot=-1; menuSelectedChar=CHAR_MARIO; menuIsNewGame=false;
  lv_obj_t *scr=lv_scr_act();
  lv_obj_set_style_bg_color(scr,lv_color_hex(COL_BG),0);
  lv_obj_set_style_bg_opa(scr,LV_OPA_COVER,0);
  lv_obj_t *title=lv_label_create(scr);
  lv_label_set_text(title,"Super Mario STM32");
  lv_obj_set_style_text_color(title,lv_color_hex(COL_TEXT_DARK),0);
  lv_obj_align(title,LV_ALIGN_TOP_MID,0,8);
  lv_obj_t *sep=lv_obj_create(scr);
  lv_obj_set_size(sep,440,1); lv_obj_align(sep,LV_ALIGN_TOP_MID,0,30);
  lv_obj_set_style_bg_color(sep,lv_color_hex(COL_SLOT_BORDER),0);
  lv_obj_set_style_bg_opa(sep,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(sep,0,0);
  for(int i=0;i<MAX_SAVES;i++){
    slotBtns[i]=lv_button_create(scr);
    lv_obj_set_size(slotBtns[i],140,56);
    lv_obj_set_pos(slotBtns[i],12+i*148,38);
    lv_obj_set_style_radius(slotBtns[i],10,0);
    lv_obj_set_style_pad_all(slotBtns[i],6,0);
    styleSlotBtn(slotBtns[i],false);
    lv_obj_add_event_cb(slotBtns[i],slotBtnCb,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    lv_obj_t *lblNum=lv_label_create(slotBtns[i]);
    char numBuf[12]; snprintf(numBuf,sizeof(numBuf),"Slot %d",i+1);
    lv_label_set_text(lblNum,numBuf);
    lv_obj_set_style_text_color(lblNum,lv_color_hex(COL_TEXT_MID),0);
    lv_obj_align(lblNum,LV_ALIGN_TOP_LEFT,4,2);
    lv_obj_t *lblMain=lv_label_create(slotBtns[i]);
    lv_obj_set_style_text_color(lblMain,lv_color_hex(COL_TEXT_DARK),0);
    if(!saves[i].used){
      lv_label_set_text(lblMain,"Nouvelle partie");
      lv_obj_align(lblMain,LV_ALIGN_CENTER,0,6);
    } else {
      static const char *cN[3]={"Mario","Luigi","Toad"};
      char buf[40];
      snprintf(buf,sizeof(buf),"%s\nNiv.%d  %d pts",
               cN[saves[i].characterId],saves[i].currentLevel+1,saves[i].score);
      lv_label_set_text(lblMain,buf);
      lv_obj_align(lblMain,LV_ALIGN_CENTER,0,6);
    }
  }
  charPanel=lv_obj_create(scr);
  lv_obj_set_size(charPanel,456,52); lv_obj_set_pos(charPanel,12,104);
  lv_obj_set_style_bg_color(charPanel,lv_color_hex(COL_BG),0);
  lv_obj_set_style_bg_opa(charPanel,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(charPanel,0,0); lv_obj_set_style_pad_all(charPanel,0,0);
  lv_obj_clear_flag(charPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(charPanel,LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *charTitle=lv_label_create(charPanel);
  lv_label_set_text(charTitle,"Personnage :");
  lv_obj_set_style_text_color(charTitle,lv_color_hex(COL_TEXT_MID),0);
  lv_obj_align(charTitle,LV_ALIGN_LEFT_MID,0,0);
  for(int i=0;i<3;i++){
    charBtns[i]=lv_button_create(charPanel);
    lv_obj_set_size(charBtns[i],100,40); lv_obj_set_pos(charBtns[i],110+i*108,6);
    lv_obj_set_style_radius(charBtns[i],8,0);
    lv_obj_set_style_bg_color(charBtns[i],lv_color_hex(charColIdle[i]),0);
    lv_obj_set_style_border_color(charBtns[i],lv_color_hex(charColSel[i]),0);
    lv_obj_set_style_border_width(charBtns[i],2,0);
    lv_obj_add_event_cb(charBtns[i],charBtnCb,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    lv_obj_t *lbl=lv_label_create(charBtns[i]);
    lv_label_set_text(lbl,charNames[i]);
    lv_obj_set_style_text_color(lbl,lv_color_hex(COL_TEXT_DARK),0);
    lv_obj_center(lbl);
  }
  lv_obj_set_style_bg_color(charBtns[CHAR_MARIO],lv_color_hex(COL_MARIO_SEL),0);
  btnStart=lv_button_create(scr);
  lv_obj_set_size(btnStart,180,44); lv_obj_set_pos(btnStart,(480-180)/2,214);
  lv_obj_set_style_radius(btnStart,22,0);
  lv_obj_set_style_bg_color(btnStart,lv_color_hex(COL_START_OFF),0);
  lv_obj_clear_flag(btnStart,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btnStart,startBtnCb,LV_EVENT_CLICKED,NULL);
  lv_obj_t *startLbl=lv_label_create(btnStart);
  lv_label_set_text(startLbl,"START");
  lv_obj_set_style_text_color(startLbl,lv_color_hex(0xF1EFE8),0);
  lv_obj_center(startLbl);
}

// ── ÉTAPE 7 : Données des 5 niveaux + drapeau ────────────────────────────────
// Chaque niveau définit :
//   - Des dalles de sol avec trous (y=GROUND_Y, h=SCREEN_H)
//   - Des plateformes flottantes
//   - Des passages bas obligatoires (plafond à y≈GROUND_Y-30 sur les niveaux 1+)
//     → le joueur doit s'accroupir (PH=26, espace libre = 30px < 26 debout)
//   - La position X du drapeau (flagX)
//
// Niveaux :
//   0 — Découverte : 2 trous, sol large, Goombas lents
//   1 — Sauter : 4 trous, plateformes, Goombas normaux
//   2 — S'accroupir : passages bas obligatoires, Goombas 1.5×
//   3 — Difficile : 5 trous, trous larges, passages bas, Goombas 2.2×
//   4 — Expert : 6 trous, enchaînements rapides, Goombas 2.2×

// ── ÉTAPE 7 : constantes grotte ──────────────────────────────────────────────
// CAVE_Y=0 : le bloc de roche part du haut de l'écran
// CAVE_H   : hauteur jusqu'à 22px au-dessus du sol
//   → espace libre pour le joueur = 22px
//   → joueur debout (PH=26) est bloqué par collision plafond
//   → joueur accroupi (by abaissé de 4px, hauteur effective ~22px) passe
#define CAVE_Y    0
#define CAVE_H    (GROUND_Y-22)
#define CAVE_COL  0x4E342E   // marron foncé (niveau 2)
#define CAVE_COL3 0x3E2723   // plus foncé   (niveau 3)
#define CAVE_COL4 0x212121   // presque noir (niveau 4)
// ── FIN ÉTAPE 7 : constantes grotte ──────────────────────────────────────────

void loadLevelPlatforms(lv_obj_t *scr){
  for(int i=0;i<platformCount;i++)
    if(platObjs[i]){ lv_obj_del(platObjs[i]); platObjs[i]=nullptr; }
  platformCount=0;

  if(currentLevel==0){
    // ── Niveau 0 : Découverte ─────────────────────────────────────────────
    // 2 trous, sol large, quelques plateformes. Juste sauter et avancer.
    //   Dalle 0 : x=0..500
    //   Trou 1  : 110px
    //   Dalle 1 : x=610..1100
    //   Trou 2  : 120px
    //   Dalle 2 : x=1220..fin (drapeau à x=2900)
    platforms[0]  = {   0, GROUND_Y, 500, SCREEN_H, 0x4A7C3F, false };
    platforms[1]  = { 610, GROUND_Y, 490, SCREEN_H, 0x4A7C3F, false };
    platforms[2]  = {1220, GROUND_Y,1700, SCREEN_H, 0x4A7C3F, false };
    // Plateformes flottantes
    platforms[3]  = { 200, 165,  70, 12, 0x795548, false };
    platforms[4]  = { 350, 140,  60, 12, 0x795548, false };
    platforms[5]  = { 700, 160,  80, 12, 0x795548, false };
    platforms[6]  = { 900, 135,  60, 12, 0x795548, false };
    platforms[7]  = {1350, 155,  70, 12, 0x795548, false };
    platforms[8]  = {1600, 130,  60, 12, 0x795548, false };
    platforms[9]  = {1900, 155,  80, 12, 0x795548, false };
    platforms[10] = {2200, 140,  60, 12, 0x795548, false };
    platformCount=11;
    flagX=2850.0f;
  }
  else if(currentLevel==1){
    // ── Niveau 1 : Sauter ────────────────────────────────────────────────
    // 4 trous, plateformes plus espacées, sol plus court entre les trous.
    //   Dalle 0 : x=0..350
    //   Trou 1  : 120px
    //   Dalle 1 : x=470..780
    //   Trou 2  : 130px
    //   Dalle 2 : x=910..1180
    //   Trou 3  : 140px
    //   Dalle 3 : x=1320..1600
    //   Trou 4  : 150px
    //   Dalle 4 : x=1750..fin (drapeau à x=2950)
    platforms[0]  = {   0, GROUND_Y, 350, SCREEN_H, 0x4A7C3F, false };
    platforms[1]  = { 470, GROUND_Y, 310, SCREEN_H, 0x4A7C3F, false };
    platforms[2]  = { 910, GROUND_Y, 270, SCREEN_H, 0x4A7C3F, false };
    platforms[3]  = {1320, GROUND_Y, 280, SCREEN_H, 0x4A7C3F, false };
    platforms[4]  = {1750, GROUND_Y,1250, SCREEN_H, 0x4A7C3F, false };
    // Plateformes flottantes (aide pour certains trous)
    platforms[5]  = { 160, 158,  65, 12, 0x5D4037, false };
    platforms[6]  = { 280, 130,  55, 12, 0x5D4037, false };
    platforms[7]  = { 510, 155,  70, 12, 0x5D4037 }; // trou 1
    platforms[8]  = { 700, 130,  60, 12, 0x5D4037, false };
    platforms[9]  = { 820, 155,  60, 12, 0x5D4037 }; // trou 2
    platforms[10] = {1000, 130,  60, 12, 0x5D4037, false };
    platforms[11] = {1100, 155,  60, 12, 0x5D4037 }; // trou 3
    platforms[12] = {1400, 130,  55, 12, 0x5D4037, false };
    platforms[13] = {1550, 155,  60, 12, 0x5D4037 }; // trou 4
    platforms[14] = {1900, 140,  70, 12, 0x5D4037, false };
    platforms[15] = {2200, 125,  60, 12, 0x5D4037, false };
    platforms[16] = {2500, 150,  80, 12, 0x5D4037, false };
    platformCount=17;
    flagX=2900.0f;
  }
  else if(currentLevel==2){
    // ── Niveau 2 : S'accroupir ────────────────────────────────────────────
    // Introduction des grottes style Mario Bros.
    // Une grotte = un grand bloc qui part du HAUT DE L'ÉCRAN (y=0) jusqu'à
    // y = GROUND_Y - 22 (laisse 22px d'espace libre au-dessus du sol).
    //
    // Pourquoi 22px ?
    //   Joueur DEBOUT  : PH = 26px → 26 > 22 → collision plafond → bloqué
    //   Joueur ACCROUPI: by abaissé de 4px → hauteur effective ~22px → passe
    //
    // Structure d'une grotte (largeur L à partir de x=GX) :
    //   [ dalle_avant ][ GROTTE_BLOC ][ dalle_après ]
    //   Le bloc va de y=0 à y=GROUND_Y-22, largeur L.
    //   Le joueur marche sur le sol sous le bloc, doit s'accroupir.
    //
    // 3 trous + 2 grottes obligatoires.

    // Dalles de sol
    platforms[0]  = {   0, GROUND_Y, 400, SCREEN_H, 0x4A7C3F, false };
    // Trou 1 : 110px
    platforms[1]  = { 510, GROUND_Y, 370, SCREEN_H, 0x4A7C3F, false };
    // Trou 2 : 120px
    platforms[2]  = { 990, GROUND_Y,2110, SCREEN_H, 0x4A7C3F, false };

    // Plateformes flottantes (zones hors grotte)
    platforms[3]  = { 150, 158,  70, 12, 0x8D6E63, false };
    platforms[4]  = { 280, 130,  60, 12, 0x8D6E63, false };
    platforms[5]  = { 560, 155,  70, 12, 0x8D6E63 }; // aide trou 1
    platforms[6]  = { 700, 130,  60, 12, 0x8D6E63, false };
    platforms[7]  = { 840, 155,  60, 12, 0x8D6E63 }; // aide trou 2

    // Grotte 1 : x=1100..1350 (250px de large) — joueur DOIT s'accroupir
    platforms[8]  = {1100, CAVE_Y, 250, CAVE_H, CAVE_COL, true };

    // Grotte 2 : x=1700..2000 (300px de large)
    platforms[9]  = {1700, CAVE_Y, 300, CAVE_H, CAVE_COL, true };

    // Plateformes après les grottes
    platforms[10] = {1400, 150,  70, 12, 0x8D6E63, false };
    platforms[11] = {1550, 130,  60, 12, 0x8D6E63, false };
    platforms[12] = {2100, 155,  80, 12, 0x8D6E63, false };
    platforms[13] = {2400, 130,  60, 12, 0x8D6E63, false };
    platforms[14] = {2700, 150,  70, 12, 0x8D6E63, false };
    platformCount=15;
    flagX=2950.0f;
  }
  else if(currentLevel==3){
    // ── Niveau 3 : Difficile ──────────────────────────────────────────────
    // 4 trous + 3 grottes, Goombas 2.2×.
    // Les grottes sont plus longues et certaines sont précédées d'un trou.

    // Dalles de sol
    platforms[0]  = {   0, GROUND_Y, 300, SCREEN_H, 0x4A7C3F, false };
    // Trou 1 : 120px
    platforms[1]  = { 420, GROUND_Y, 260, SCREEN_H, 0x4A7C3F, false };
    // Trou 2 : 130px
    platforms[2]  = { 810, GROUND_Y, 240, SCREEN_H, 0x4A7C3F, false };
    // Trou 3 : 140px
    platforms[3]  = {1190, GROUND_Y, 260, SCREEN_H, 0x4A7C3F, false };
    // Trou 4 : 150px
    platforms[4]  = {1600, GROUND_Y,1500, SCREEN_H, 0x4A7C3F, false };

    // Plateformes flottantes (zones hors grotte, peu d'aide)
    platforms[5]  = { 130, 155,  55, 12, 0x4E342E, false };
    platforms[6]  = { 240, 125,  50, 12, 0x4E342E, false };
    platforms[7]  = { 435, 155,  55, 12, 0x4E342E }; // trou 1
    platforms[8]  = { 580, 130,  50, 12, 0x4E342E, false };
    platforms[9]  = { 700, 155,  50, 12, 0x4E342E }; // trou 2
    platforms[10] = { 870, 130,  50, 12, 0x4E342E, false };
    platforms[11] = {1010, 155,  50, 12, 0x4E342E }; // trou 3

    // Grotte 1 : x=1300..1560 (260px)
    platforms[12] = {1300, CAVE_Y, 260, CAVE_H, CAVE_COL3, true };

    // Grotte 2 : x=1750..2100 (350px) — plus longue
    platforms[13] = {1750, CAVE_Y, 350, CAVE_H, CAVE_COL3, true };

    // Grotte 3 : x=2400..2700 (300px)
    platforms[14] = {2400, CAVE_Y, 300, CAVE_H, CAVE_COL3, true };

    // Plateformes après les grottes
    platforms[15] = {1620, 145,  50, 12, 0x4E342E, false };
    platforms[16] = {2200, 130,  50, 12, 0x4E342E, false };
    platforms[17] = {2750, 140,  55, 12, 0x4E342E, false };
    platforms[18] = {2900, 120,  50, 12, 0x4E342E, false };
    platformCount=19;
    flagX=2980.0f;
  }
  else if(currentLevel==4){
    // ── Niveau 4 : Expert ─────────────────────────────────────────────────
    // 5 trous serrés + 3 grottes longues enchaînées + Goombas 2.2×.
    // Certaines grottes sont juste après un trou → il faut sauter PUIS s'accroupir.

    // Dalles de sol
    platforms[0]  = {   0, GROUND_Y, 250, SCREEN_H, 0x4A7C3F, false };
    // Trou 1 : 120px
    platforms[1]  = { 370, GROUND_Y, 200, SCREEN_H, 0x4A7C3F, false };
    // Trou 2 : 130px
    platforms[2]  = { 700, GROUND_Y, 200, SCREEN_H, 0x4A7C3F, false };
    // Trou 3 : 140px
    platforms[3]  = {1040, GROUND_Y, 190, SCREEN_H, 0x4A7C3F, false };
    // Trou 4 : 150px
    platforms[4]  = {1380, GROUND_Y, 200, SCREEN_H, 0x4A7C3F, false };
    // Trou 5 : 160px
    platforms[5]  = {1740, GROUND_Y,1460, SCREEN_H, 0x4A7C3F, false };

    // Plateformes flottantes très réduites
    platforms[6]  = { 100, 155,  45, 12, 0x212121, false };
    platforms[7]  = { 200, 125,  40, 12, 0x212121, false };
    platforms[8]  = { 375, 155,  45, 12, 0x212121 }; // trou 1
    platforms[9]  = { 500, 130,  40, 12, 0x212121, false };
    platforms[10] = { 615, 155,  40, 12, 0x212121 }; // trou 2

    // Grotte 1 juste après trou 3 : x=1090..1350 (260px)
    // Le joueur saute le trou 3, atterrit, et doit IMMÉDIATEMENT s'accroupir
    platforms[11] = {1090, CAVE_Y, 260, CAVE_H, CAVE_COL4, true };

    // Grotte 2 : x=1460..1700 (240px) — avant trou 5
    platforms[12] = {1460, CAVE_Y, 240, CAVE_H, CAVE_COL4, true };

    // Grotte 3 longue : x=1900..2350 (450px)
    platforms[13] = {1900, CAVE_Y, 450, CAVE_H, CAVE_COL4, true };

    // Grotte 4 : x=2600..2900 (300px)
    platforms[14] = {2600, CAVE_Y, 300, CAVE_H, CAVE_COL4, true };

    // Quelques plateformes hors grottes
    platforms[15] = {1760, 150,  45, 12, 0x212121, false };
    platforms[16] = {2450, 130,  40, 12, 0x212121, false };
    platforms[17] = {2980, 145,  50, 12, 0x212121, false };
    platformCount=18;
    flagX=3050.0f;
  }

  for(int i=0;i<platformCount;i++)
    platObjs[i]=makeRect(scr,
      (int)(platforms[i].x-cameraX),(int)(platforms[i].y),
      (int)platforms[i].w,(int)platforms[i].h,platforms[i].color);
}
// ── FIN ÉTAPE 7 : données niveaux ────────────────────────────────────────────

static void createGoombaSprites(lv_obj_t *scr){
  const int S=2;
  for(int i=0;i<goombaCount;i++){
    Goomba &g=goombas[i];
    int sx=(int)(g.x-cameraX), sy=(int)(g.y);
    g.objHat   =makeRect(scr,sx,      sy-(int)GOOMBA_H,       7*S,2*S,GOOMBA_HAT_COL);
    g.objBody  =makeRect(scr,sx,      sy-(int)GOOMBA_H+2*S,   7*S,4*S,GOOMBA_BODY_COL);
    g.objFeetL =makeRect(scr,sx,      sy-2*S,                 3*S,2*S,GOOMBA_FEET_COL);
    g.objFeetR =makeRect(scr,sx+4*S,  sy-2*S,                 3*S,2*S,GOOMBA_FEET_COL);
  }
}

// ── ÉTAPE 7 : données Goombas pour les 5 niveaux ─────────────────────────────
// La vitesse effective est appliquée dans updateGame() selon currentLevel.
// Ici on stocke la vitesse de base (signe = direction initiale).
void loadLevelGoombas(lv_obj_t *scr){
  goombaCount=0;
  float spd=-GOOMBA_SPEED; // direction initiale : gauche

  if(currentLevel==0){
    // 3 Goombas au sol, 2 sur plateformes
    goombas[0]={250,GROUND_Y,spd,true,10,490,nullptr,nullptr,nullptr,nullptr};
    goombas[1]={700,GROUND_Y,spd,true,615,1090,nullptr,nullptr,nullptr,nullptr};
    goombas[2]={1400,GROUND_Y,spd,true,1225,1800,nullptr,nullptr,nullptr,nullptr};
    // sur platforms[5] (x=350,w=60) et platforms[7] (x=700,w=80)
    goombas[3]={355,platforms[5].y,spd,true,
                platforms[5].x+2,platforms[5].x+platforms[5].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombas[4]={705,platforms[7].y,spd,true,
                platforms[7].x+2,platforms[7].x+platforms[7].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombaCount=5;
  }
  else if(currentLevel==1){
    // 3 Goombas au sol, 2 sur plateformes
    goombas[0]={180,GROUND_Y,spd,true,10,345,nullptr,nullptr,nullptr,nullptr};
    goombas[1]={600,GROUND_Y,spd,true,475,775,nullptr,nullptr,nullptr,nullptr};
    goombas[2]={1850,GROUND_Y,spd,true,1755,2090,nullptr,nullptr,nullptr,nullptr};
    // sur platforms[9] et platforms[14]
    goombas[3]={825,platforms[9].y,spd,true,
                platforms[9].x+2,platforms[9].x+platforms[9].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombas[4]={1905,platforms[14].y,spd,true,
                platforms[14].x+2,platforms[14].x+platforms[14].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombaCount=5;
  }
  else if(currentLevel==2){
    // 3 Goombas au sol, 2 sur plateformes (vitesse x1.5 dans updateGame)
    // Niv2 : platforms[5]=x=560,w=70 | platforms[6]=x=700,w=60
    goombas[0]={200,GROUND_Y,spd,true,10,395,nullptr,nullptr,nullptr,nullptr};
    goombas[1]={620,GROUND_Y,spd,true,515,875,nullptr,nullptr,nullptr,nullptr};
    goombas[2]={1600,GROUND_Y,spd,true,1455,1690,nullptr,nullptr,nullptr,nullptr};
    goombas[3]={565,platforms[5].y,spd,true,
                platforms[5].x+2,platforms[5].x+platforms[5].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombas[4]={705,platforms[6].y,spd,true,
                platforms[6].x+2,platforms[6].x+platforms[6].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombaCount=5;
  }
  else if(currentLevel==3){
    // 3 au sol + 2 sur plateformes (vitesse x2.2 dans updateGame)
    // Niv3 : platforms[5]=x=130,w=55 | platforms[8]=x=435,w=55
    goombas[0]={150,GROUND_Y,spd,true,10,295,nullptr,nullptr,nullptr,nullptr};
    goombas[1]={450,GROUND_Y,spd,true,425,665,nullptr,nullptr,nullptr,nullptr};
    goombas[2]={2050,GROUND_Y,spd,true,1905,2290,nullptr,nullptr,nullptr,nullptr};
    goombas[3]={135,platforms[5].y,spd,true,
                platforms[5].x+2,platforms[5].x+platforms[5].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombas[4]={440,platforms[8].y,spd,true,
                platforms[8].x+2,platforms[8].x+platforms[8].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombaCount=5;
  }
  else if(currentLevel==4){
    // 3 au sol + 2 sur plateformes (vitesse x2.2 dans updateGame)
    // Niv4 : platforms[6]=x=100,w=45 | platforms[9]=x=375,w=45
    goombas[0]={120,GROUND_Y,spd,true,10,245,nullptr,nullptr,nullptr,nullptr};
    goombas[1]={400,GROUND_Y,spd,true,375,565,nullptr,nullptr,nullptr,nullptr};
    goombas[2]={2050,GROUND_Y,spd,true,1945,2290,nullptr,nullptr,nullptr,nullptr};
    goombas[3]={105,platforms[6].y,spd,true,
                platforms[6].x+2,platforms[6].x+platforms[6].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombas[4]={380,platforms[9].y,spd,true,
                platforms[9].x+2,platforms[9].x+platforms[9].w-GOOMBA_W-2,
                nullptr,nullptr,nullptr,nullptr};
    goombaCount=5;
  }
  createGoombaSprites(scr);
}
// ── FIN ÉTAPE 7 : données Goombas ────────────────────────────────────────────

static void drawPlayer(lv_obj_t *scr){
  const int S=2; int c=player.characterId;
  uint32_t colHat  =(c==CHAR_LUIGI)?0x388E3C:0xE53935;
  uint32_t colPants=0x1565C0, colShoes=0x5D4037, colSkin=0xFFCDD2;
  uint32_t colHair =(c==CHAR_TOAD)?0xFFFFFF:0x5D4037;
  uint32_t colEyes=0x212121, colNose=0xFF8A80, colBrim=0xEEEEEE;
  if(c==CHAR_MARIO||c==CHAR_LUIGI){
    spHatTop=makeRect(scr,0,0,6*S,2*S,colHat);
    spHat   =makeRect(scr,0,0,8*S,2*S,colHat); spHatBrim=nullptr;
    objHead =makeRect(scr,0,0,6*S,3*S,colSkin);
    spHair[0]=makeRect(scr,0,0,2*S,1*S,colHair);
    spHair[1]=makeRect(scr,0,0,2*S,1*S,colHair);
    spEye[0]=makeRect(scr,0,0,1*S,1*S,colEyes);
    spEye[1]=makeRect(scr,0,0,1*S,1*S,colEyes);
    spMustache=makeRect(scr,0,0,6*S,1*S,colHair);
    spShirt=makeRect(scr,0,0,8*S,3*S,colHat);
    spArm[0]=makeRect(scr,0,0,1*S,2*S,colSkin);
    spArm[1]=makeRect(scr,0,0,1*S,2*S,colSkin);
    spLegL  =makeRect(scr,0,0,3*S,3*S,colPants);
    spLegR  =makeRect(scr,0,0,3*S,3*S,colPants);
    spLegMid=makeRect(scr,0,0,4*S,2*S,colPants);
    spShoeL =makeRect(scr,0,0,3*S,2*S,colShoes);
    spShoeR =makeRect(scr,0,0,3*S,2*S,colShoes); spSpot3=nullptr;
  } else {
    spHatTop =makeRect(scr,0,0,12*S,1*S,colHat);
    spHat    =makeRect(scr,0,0,12*S,3*S,colHat);
    spHatBrim=makeRect(scr,0,0,10*S,1*S,colBrim);
    spHair[0]=makeRect(scr,0,0,3*S,2*S,colHair);
    spHair[1]=makeRect(scr,0,0,2*S,2*S,colHair);
    spSpot3  =makeRect(scr,0,0,3*S,2*S,colHair);
    objHead  =makeRect(scr,0,0,8*S,3*S,colSkin);
    spEye[0]=makeRect(scr,0,0,2*S,1*S,colEyes);
    spEye[1]=makeRect(scr,0,0,2*S,1*S,colEyes);
    spMustache=makeRect(scr,0,0,2*S,1*S,colNose);
    spShirt  =makeRect(scr,0,0,8*S,3*S,colPants);
    spArm[0]=makeRect(scr,0,0,1*S,2*S,colSkin);
    spArm[1]=makeRect(scr,0,0,1*S,2*S,colSkin);
    spLegL  =makeRect(scr,0,0,3*S,3*S,0xFFFFFF);
    spLegR  =makeRect(scr,0,0,3*S,3*S,0xFFFFFF);
    spLegMid=makeRect(scr,0,0,4*S,2*S,0xFFFFFF);
    spShoeL =makeRect(scr,0,0,3*S,2*S,colShoes);
    spShoeR =makeRect(scr,0,0,3*S,2*S,colShoes);
  }
  objPlayer=spShirt;
}

void initGameObjects(lv_obj_t *scr){
  lv_obj_set_style_bg_color(scr,lv_color_hex(SKY_COLOR),0);
  lv_obj_set_style_bg_opa(scr,LV_OPA_COVER,0);
  lv_obj_set_style_pad_all(scr,0,0);
  lv_obj_set_style_border_width(scr,0,0);
  objGround=nullptr;
  objGrass=makeRect(scr,0,GROUND_VISUAL_Y,WORLD_W,4,GRASS_COLOR);
  lblScore=makeHudLabel(scr,8,4,160);   lv_label_set_text(lblScore,"Score: 000000");
  lblLives=makeHudLabel(scr,210,4,60);  lv_label_set_text(lblLives,"x3");
  lblLevel=makeHudLabel(scr,400,4,70);  lv_label_set_text(lblLevel,"Niv.1");
  lblDebug=makeHudLabel(scr,8,SCREEN_H-16,140); lv_label_set_text(lblDebug,"x:0 y:0");
  loadLevelPlatforms(scr);
  loadLevelGoombas(scr);

  // ── ÉTAPE 7 : crée le drapeau ─────────────────────────────────────────────
  // Barre grise : x=flagX, du sol jusqu'en haut (FLAG_POLE_H px)
  // Carré vert  : en haut de la barre, décalé à gauche
  int fsx=(int)(flagX-cameraX);
  objFlagPole=makeRect(scr,fsx,GROUND_VISUAL_Y-FLAG_POLE_H,
                       FLAG_POLE_W,FLAG_POLE_H,0x9E9E9E);
  objFlagTop =makeRect(scr,fsx-FLAG_TOP_W+FLAG_POLE_W,
                       GROUND_VISUAL_Y-FLAG_POLE_H,
                       FLAG_TOP_W,FLAG_TOP_H,0x2E7D32);
  // ── FIN ÉTAPE 7 : drapeau ────────────────────────────────────────────────

  drawPlayer(scr);
  if(player.y<GROUND_Y) player.y=GROUND_Y;
  if(player.x<50.0f)    player.x=50.0f;
  renderGame();
}

void renderGame(){
  if(!objPlayer) return;
  float targetX=player.x-(float)SCREEN_W/3.0f;
  if(targetX<0.0f) targetX=0.0f;
  cameraX+=(targetX-cameraX)*0.15f;
  int screenPX=(int)(player.x-cameraX);
  const int S=2;
  int spriteH=(player.characterId==CHAR_TOAD)?15*S:13*S;
  int screenPY=(int)(player.y);
  int legOffset=player.crouching?2*S:0;
  int by=screenPY-spriteH+legOffset;
  int bx=screenPX, c=player.characterId;

  if(c==CHAR_MARIO||c==CHAR_LUIGI){
    int lo=player.crouching?2*S:0;
    moveSprite(spHatTop,  bx,by, 3*S, 0*S); moveSprite(spHat,     bx,by, 2*S, 1*S);
    moveSprite(objHead,   bx,by, 3*S, 2*S); moveSprite(spHair[0], bx,by, 3*S, 2*S);
    moveSprite(spHair[1], bx,by, 7*S, 2*S); moveSprite(spEye[0],  bx,by, 4*S, 3*S);
    moveSprite(spEye[1],  bx,by, 7*S, 3*S); moveSprite(spMustache,bx,by, 3*S, 4*S);
    moveSprite(spShirt,   bx,by, 2*S, 5*S); moveSprite(spArm[0],  bx,by, 1*S, 5*S);
    moveSprite(spArm[1],  bx,by,10*S, 5*S);
    moveSprite(spLegL,  bx,by, 2*S, 8*S-lo); moveSprite(spLegR,  bx,by, 7*S, 8*S-lo);
    moveSprite(spLegMid,bx,by, 4*S, 8*S-lo); moveSprite(spShoeL, bx,by, 2*S,11*S-lo);
    moveSprite(spShoeR, bx,by, 7*S,11*S-lo);
  } else {
    int lo=player.crouching?2*S:0;
    moveSprite(spHatTop,  bx,by, 0*S, 0*S); moveSprite(spHat,     bx,by, 0*S, 1*S);
    moveSprite(spHatBrim, bx,by, 1*S, 4*S); moveSprite(spHair[0], bx,by, 1*S, 1*S);
    moveSprite(spHair[1], bx,by, 5*S, 1*S); moveSprite(spSpot3,   bx,by, 8*S, 1*S);
    moveSprite(objHead,   bx,by, 2*S, 4*S); moveSprite(spEye[0],  bx,by, 3*S, 5*S);
    moveSprite(spEye[1],  bx,by, 7*S, 5*S); moveSprite(spMustache,bx,by, 5*S, 6*S);
    moveSprite(spShirt,   bx,by, 2*S, 7*S); moveSprite(spArm[0],  bx,by, 1*S, 7*S);
    moveSprite(spArm[1],  bx,by,10*S, 7*S);
    moveSprite(spLegL,  bx,by, 2*S,10*S-lo); moveSprite(spLegR,  bx,by, 7*S,10*S-lo);
    moveSprite(spLegMid,bx,by, 4*S,10*S-lo); moveSprite(spShoeL, bx,by, 2*S,13*S-lo);
    moveSprite(spShoeR, bx,by, 7*S,13*S-lo);
  }

  char buf[48];
  snprintf(buf,sizeof(buf),"Score: %06d",player.score); lv_label_set_text(lblScore,buf);
  snprintf(buf,sizeof(buf),"x%d",player.lives);         lv_label_set_text(lblLives,buf);
  snprintf(buf,sizeof(buf),"Niv.%d",currentLevel+1);    lv_label_set_text(lblLevel,buf);
  snprintf(buf,sizeof(buf),"x:%.0f y:%.0f",player.x,player.y); lv_label_set_text(lblDebug,buf);

  if(objGrass) lv_obj_set_pos(objGrass,(int)(-cameraX),GROUND_VISUAL_Y);

  for(int i=0;i<platformCount;i++)
    if(platObjs[i])
      lv_obj_set_pos(platObjs[i],(int)(platforms[i].x-cameraX),(int)(platforms[i].y));

  const int GS=2;
  for(int i=0;i<goombaCount;i++){
    Goomba &g=goombas[i];
    if(!g.alive) continue;
    int gsx=(int)(g.x-cameraX), gsy=(int)(g.y);
    if(g.objHat)   lv_obj_set_pos(g.objHat,  gsx,      gsy-(int)GOOMBA_H);
    if(g.objBody)  lv_obj_set_pos(g.objBody, gsx,      gsy-(int)GOOMBA_H+2*GS);
    if(g.objFeetL) lv_obj_set_pos(g.objFeetL,gsx,      gsy-2*GS);
    if(g.objFeetR) lv_obj_set_pos(g.objFeetR,gsx+4*GS, gsy-2*GS);
  }

  // ── ÉTAPE 7 : repositionne le drapeau avec la caméra ─────────────────────
  if(objFlagPole)
    lv_obj_set_pos(objFlagPole,(int)(flagX-cameraX),GROUND_VISUAL_Y-FLAG_POLE_H);
  if(objFlagTop)
    lv_obj_set_pos(objFlagTop,
      (int)(flagX-cameraX)-FLAG_TOP_W+FLAG_POLE_W,
      GROUND_VISUAL_Y-FLAG_POLE_H);
  // ── FIN ÉTAPE 7 : drapeau ────────────────────────────────────────────────
}

void showGame(){
  lv_obj_t *scr=lv_scr_act(); cameraX=0.0f;
  objSky=nullptr; objGround=nullptr; objGrass=nullptr;
  objPlayer=nullptr; objHead=nullptr;
  lblScore=nullptr; lblLives=nullptr; lblLevel=nullptr; lblDebug=nullptr;
  spHat=nullptr; spHatTop=nullptr; spHatBrim=nullptr;
  spHair[0]=nullptr; spHair[1]=nullptr; spSpot3=nullptr;
  spEye[0]=nullptr; spEye[1]=nullptr;
  spMustache=nullptr; spShirt=nullptr;
  spArm[0]=nullptr; spArm[1]=nullptr;
  spLegL=nullptr; spLegR=nullptr; spLegMid=nullptr;
  spShoeL=nullptr; spShoeR=nullptr;
  objFlagPole=nullptr; objFlagTop=nullptr;
  for(int i=0;i<MAX_PLATFORMS;i++) platObjs[i]=nullptr;
  platformCount=0;
  for(int i=0;i<MAX_GOOMBAS;i++){
    goombas[i].alive=false;
    goombas[i].objHat=nullptr; goombas[i].objBody=nullptr;
    goombas[i].objFeetL=nullptr; goombas[i].objFeetR=nullptr;
  }
  goombaCount=0;
  initGameObjects(scr);
}

void mySetup()
{
  initInputs(); initSaves(); showJoystickTest();
}
void loop() {}
void myTask(void *pvParameters)
{
  TickType_t xLastWakeTime=xTaskGetTickCount();
  while(1){
    updateInputs();
    InputState inputs=getInputs();
    lvglLock();
    if(currentScreen==SCREEN_JOYSTICK_TEST) updateJoystickTest(inputs);
    else if(currentScreen==SCREEN_GAME){ updateGame(inputs); renderGame(); }
    lvglUnlock();
    Serial.print("X: "); Serial.print(inputs.joyX);
    Serial.print(" Y: "); Serial.print(inputs.joyY);
    Serial.print(" Jump: "); Serial.print(inputs.buttonJump);
    Serial.print(" Down: "); Serial.println(inputs.buttonDown);
    vTaskDelayUntil(&xLastWakeTime,pdMS_TO_TICKS(40)); // 25fps, réduit le lag LVGL
  }
}
#else
#include "lvgl.h"
#include "app_hal.h"
#include <cstdio>
int main(void)
{
  printf("LVGL Simulator\n"); fflush(stdout);
  lv_init(); hal_setup(); testLvgl(); hal_loop();
  return 0;
}
#endif