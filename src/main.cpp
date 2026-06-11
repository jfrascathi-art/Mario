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
    if (isLeft())  return -(float)(JOY_CENTER-JOY_DEADZONE-joyX)/(float)(JOY_CENTER-JOY_DEADZONE);
    if (isRight()) return  (float)(joyX-JOY_CENTER-JOY_DEADZONE)/(float)(JOY_MAX-JOY_CENTER-JOY_DEADZONE);
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
void initInputs() { pinMode(BUTTON_JUMP,INPUT_PULLUP); pinMode(BUTTON_DOWN,INPUT_PULLUP); }
void updateInputs() {
  inputs.joyX=analogRead(JOY_X); inputs.joyY=analogRead(JOY_Y);
  inputs.buttonJump=!digitalRead(BUTTON_JUMP); inputs.buttonDown=!digitalRead(BUTTON_DOWN);
}
InputState getInputs() { return inputs; }
InputState getInputs();

// ═══ variables globales écran de test ═══
static lv_obj_t* joyCursor=nullptr, *joyCircle=nullptr;
static lv_obj_t* labelRaw=nullptr,  *labelNorm=nullptr;
static lv_obj_t* labelDir=nullptr,  *labelButtons=nullptr;

// ═══ structures du jeu ═══
struct SaveSlot { bool used; int characterId,currentLevel,score,lives; char name[12]; };
#define MAX_SAVES 3
SaveSlot saves[MAX_SAVES];
enum PowerUp     { POWERUP_NONE=0,POWERUP_MUSHROOM=1,POWERUP_FIRE=2,POWERUP_MINI=3 };
enum CharacterId { CHAR_MARIO=0,CHAR_LUIGI=1,CHAR_TOAD=2 };
struct Player {
  float x,y,velX,velY;
  bool  onGround,isAlive;
  PowerUp powerUp;
  int  lives,score,characterId;
};
Player player;
int currentLevel=0, activeSaveSlot=0;

#define GRAVITY        0.5f
#define WALK_SPEED     3.0f
#define JUMP_VELOCITY -9.0f
#define GROUND_Y     200.0f

// sauvegardes
void initSaves() {
  for(int i=0;i<MAX_SAVES;i++){
    saves[i].used=false;saves[i].characterId=CHAR_MARIO;
    saves[i].currentLevel=0;saves[i].score=0;saves[i].lives=3;
    saves[i].name[0]='P';saves[i].name[1]='a';saves[i].name[2]='r';
    saves[i].name[3]='t';saves[i].name[4]='i';saves[i].name[5]='e';
    saves[i].name[6]=' ';saves[i].name[7]='1'+i;saves[i].name[8]='\0';
  }
}
void loadSave(int idx){
  activeSaveSlot=idx;currentLevel=saves[idx].currentLevel;
  player.score=saves[idx].score;player.lives=saves[idx].lives;
  player.characterId=saves[idx].characterId;player.powerUp=POWERUP_NONE;
  player.isAlive=true;player.x=50.0f;player.y=GROUND_Y;
  player.velX=0.0f;player.velY=0.0f;player.onGround=true;
}
void newGame(int idx,int charId){
  activeSaveSlot=idx;saves[idx].used=true;saves[idx].characterId=charId;
  saves[idx].currentLevel=0;saves[idx].score=0;saves[idx].lives=3;
  loadSave(idx);
}
void saveCurrentGame(){
  saves[activeSaveSlot].used=true;saves[activeSaveSlot].currentLevel=currentLevel;
  saves[activeSaveSlot].score=player.score;saves[activeSaveSlot].lives=player.lives;
  saves[activeSaveSlot].characterId=player.characterId;
}

// moteur de jeu (physique)
void updateGame(InputState& in){
  player.velX=in.normalizedX()*WALK_SPEED;
  if(in.buttonJump&&player.onGround){player.velY=JUMP_VELOCITY;player.onGround=false;}
  player.velY+=GRAVITY;
  player.x+=player.velX; player.y+=player.velY;
  if(player.y>=GROUND_Y){player.y=GROUND_Y;player.velY=0.0f;player.onGround=true;}
  if(player.x<0.0f) player.x=0.0f;
}

// écran test joystick
#define JOY_CIRCLE_R 90
#define JOY_CURSOR_R  8
void showMenu();
static void joyTestOkCb(lv_event_t* e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
  joyCursor=nullptr;joyCircle=nullptr;
  labelRaw=nullptr;labelNorm=nullptr;labelDir=nullptr;labelButtons=nullptr;
  lv_obj_clean(lv_scr_act());
  currentScreen=SCREEN_MENU; showMenu();
}
void showJoystickTest(){
  lv_obj_t* scr=lv_scr_act();
  lv_obj_t* title=lv_label_create(scr);
  lv_label_set_text(title,"Test joystick");
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
  lv_obj_t* dzCircle=lv_obj_create(scr);
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
  labelRaw=lv_label_create(scr);lv_label_set_text(labelRaw,"X:  512\nY:  512");
  lv_obj_align(labelRaw,LV_ALIGN_LEFT_MID,220,-60);
  labelNorm=lv_label_create(scr);lv_label_set_text(labelNorm,"nX: 0.00\nnY: 0.00");
  lv_obj_align(labelNorm,LV_ALIGN_LEFT_MID,220,0);
  labelDir=lv_label_create(scr);lv_label_set_text(labelDir,"Zone morte");
  lv_obj_align(labelDir,LV_ALIGN_LEFT_MID,220,60);
  labelButtons=lv_label_create(scr);lv_label_set_text(labelButtons,"JUMP:0  DOWN:0");
  lv_obj_align(labelButtons,LV_ALIGN_LEFT_MID,220,90);
  lv_obj_t* btnOk=lv_button_create(scr);
  lv_obj_align(btnOk,LV_ALIGN_BOTTOM_RIGHT,-10,-10);
  lv_obj_add_event_cb(btnOk,joyTestOkCb,LV_EVENT_ALL,NULL);
  lv_obj_t* lblOk=lv_label_create(btnOk);
  lv_label_set_text(lblOk,"OK -> menu");lv_obj_center(lblOk);
}
void updateJoystickTest(InputState& in){
  if(!joyCursor||!joyCircle) return;
  int ox=(int)(in.normalizedX()*JOY_CIRCLE_R);
  int oy=(int)(in.normalizedY()*JOY_CIRCLE_R);
  lv_obj_align_to(joyCursor,joyCircle,LV_ALIGN_CENTER,ox,oy);
  lv_obj_set_style_bg_color(joyCursor,
    lv_color_hex(ox==0&&oy==0?0x999999:0x1D9E75),0);
  char buf[48];
  snprintf(buf,sizeof(buf),"X:  %4d\nY:  %4d",in.joyX,in.joyY);
  lv_label_set_text(labelRaw,buf);
  snprintf(buf,sizeof(buf),"nX: %+.2f\nnY: %+.2f",in.normalizedX(),in.normalizedY());
  lv_label_set_text(labelNorm,buf);
  if(ox==0&&oy==0){lv_label_set_text(labelDir,"Zone morte");}
  else{
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

// palette pastel menu (parce que c'est plus joli)
#define COL_BG          0xD6F0E8
#define COL_SLOT_IDLE   0xC8EAE0
#define COL_SLOT_SEL    0x5DCAA5
#define COL_SLOT_BORDER 0x9FE1CB
#define COL_TEXT_DARK   0x085041
#define COL_TEXT_MID    0x0F6E56
#define COL_MARIO_IDLE  0xFFD6D6
#define COL_MARIO_SEL   0xF09595
#define COL_LUIGI_IDLE  0xD6F0DD
#define COL_LUIGI_SEL   0x97C459
#define COL_TOAD_IDLE   0xD6E8FF
#define COL_TOAD_SEL    0x85B7EB
#define COL_START_OFF   0xB4B2A9
#define COL_START_ON    0x1D9E75

// variables globales menu
static int  menuSelectedSlot=-1, menuSelectedChar=CHAR_MARIO;
static bool menuIsNewGame=false;
static lv_obj_t* btnStart=nullptr, *charPanel=nullptr;
static lv_obj_t* slotBtns[MAX_SAVES]={nullptr,nullptr,nullptr};
static lv_obj_t* charBtns[3]={nullptr,nullptr,nullptr};
static const char* charNames[3]={"Mario","Luigi","Toad"};
static const uint32_t charColIdle[3]={COL_MARIO_IDLE,COL_LUIGI_IDLE,COL_TOAD_IDLE};
static const uint32_t charColSel[3] ={COL_MARIO_SEL, COL_LUIGI_SEL, COL_TOAD_SEL};
static void styleSlotBtn(lv_obj_t* btn,bool sel){
  lv_obj_set_style_bg_color(btn,lv_color_hex(sel?COL_SLOT_SEL:COL_SLOT_IDLE),0);
  lv_obj_set_style_border_color(btn,lv_color_hex(sel?COL_TEXT_DARK:COL_SLOT_BORDER),0);
  lv_obj_set_style_border_width(btn,2,0);
}
void showGame();
void renderGame(); // déclaration anticipée — défini plus bas, appelé depuis initGameObjects
static void slotBtnCb(lv_event_t* e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
  menuSelectedSlot=(int)(intptr_t)lv_event_get_user_data(e);
  menuIsNewGame=!saves[menuSelectedSlot].used;
  for(int i=0;i<MAX_SAVES;i++) if(slotBtns[i]) styleSlotBtn(slotBtns[i],i==menuSelectedSlot);
  if(menuIsNewGame) lv_obj_clear_flag(charPanel,LV_OBJ_FLAG_HIDDEN);
  else              lv_obj_add_flag(charPanel,LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(btnStart,lv_color_hex(COL_START_ON),0);
  lv_obj_add_flag(btnStart,LV_OBJ_FLAG_CLICKABLE);
}
static void charBtnCb(lv_event_t* e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
  menuSelectedChar=(int)(intptr_t)lv_event_get_user_data(e);
  for(int i=0;i<3;i++) if(charBtns[i])
    lv_obj_set_style_bg_color(charBtns[i],
      lv_color_hex(i==menuSelectedChar?charColSel[i]:charColIdle[i]),0);
}
static void startBtnCb(lv_event_t* e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
  if(menuSelectedSlot<0) return;
  btnStart=nullptr;charPanel=nullptr;
  for(int i=0;i<MAX_SAVES;i++) slotBtns[i]=nullptr;
  for(int i=0;i<3;i++) charBtns[i]=nullptr;
  if(menuIsNewGame) newGame(menuSelectedSlot,menuSelectedChar);
  else              loadSave(menuSelectedSlot);
  lv_obj_clean(lv_scr_act());
  currentScreen=SCREEN_GAME;
  showGame();
}
void showMenu(){
  menuSelectedSlot=-1;menuSelectedChar=CHAR_MARIO;menuIsNewGame=false;
  lv_obj_t* scr=lv_scr_act();
  lv_obj_set_style_bg_color(scr,lv_color_hex(COL_BG),0);
  lv_obj_set_style_bg_opa(scr,LV_OPA_COVER,0);
  lv_obj_t* title=lv_label_create(scr);
  lv_label_set_text(title,"Super Mario STM32");
  lv_obj_set_style_text_color(title,lv_color_hex(COL_TEXT_DARK),0);
  lv_obj_align(title,LV_ALIGN_TOP_MID,0,8);
  lv_obj_t* sep=lv_obj_create(scr);
  lv_obj_set_size(sep,440,1);lv_obj_align(sep,LV_ALIGN_TOP_MID,0,30);
  lv_obj_set_style_bg_color(sep,lv_color_hex(COL_SLOT_BORDER),0);
  lv_obj_set_style_bg_opa(sep,LV_OPA_COVER,0);lv_obj_set_style_border_width(sep,0,0);
  for(int i=0;i<MAX_SAVES;i++){
    slotBtns[i]=lv_button_create(scr);
    lv_obj_set_size(slotBtns[i],140,56);
    lv_obj_set_pos(slotBtns[i],12+i*148,38);
    lv_obj_set_style_radius(slotBtns[i],10,0);
    lv_obj_set_style_pad_all(slotBtns[i],6,0);
    styleSlotBtn(slotBtns[i],false);
    lv_obj_add_event_cb(slotBtns[i],slotBtnCb,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    lv_obj_t* lblNum=lv_label_create(slotBtns[i]);
    char numBuf[12]; snprintf(numBuf,sizeof(numBuf),"Slot %d",i+1);
    lv_label_set_text(lblNum,numBuf);
    lv_obj_set_style_text_color(lblNum,lv_color_hex(COL_TEXT_MID),0);
    lv_obj_align(lblNum,LV_ALIGN_TOP_LEFT,4,2);
    lv_obj_t* lblMain=lv_label_create(slotBtns[i]);
    lv_obj_set_style_text_color(lblMain,lv_color_hex(COL_TEXT_DARK),0);
    if(!saves[i].used){
      lv_label_set_text(lblMain,"Nouvelle partie");
      lv_obj_align(lblMain,LV_ALIGN_CENTER,0,6);
    } else {
      static const char* cN[3]={"Mario","Luigi","Toad"};
      char buf[40];
      snprintf(buf,sizeof(buf),"%s\nNiv.%d  %d pts",
               cN[saves[i].characterId],saves[i].currentLevel+1,saves[i].score);
      lv_label_set_text(lblMain,buf);
      lv_obj_align(lblMain,LV_ALIGN_CENTER,0,6);
    }
  }
  charPanel=lv_obj_create(scr);
  lv_obj_set_size(charPanel,456,52);lv_obj_set_pos(charPanel,12,104);
  lv_obj_set_style_bg_color(charPanel,lv_color_hex(COL_BG),0);
  lv_obj_set_style_bg_opa(charPanel,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(charPanel,0,0);lv_obj_set_style_pad_all(charPanel,0,0);
  lv_obj_clear_flag(charPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(charPanel,LV_OBJ_FLAG_HIDDEN);
  lv_obj_t* charTitle=lv_label_create(charPanel);
  lv_label_set_text(charTitle,"Personnage :");
  lv_obj_set_style_text_color(charTitle,lv_color_hex(COL_TEXT_MID),0);
  lv_obj_align(charTitle,LV_ALIGN_LEFT_MID,0,0);
  for(int i=0;i<3;i++){
    charBtns[i]=lv_button_create(charPanel);
    lv_obj_set_size(charBtns[i],100,40);lv_obj_set_pos(charBtns[i],110+i*108,6);
    lv_obj_set_style_radius(charBtns[i],8,0);
    lv_obj_set_style_bg_color(charBtns[i],lv_color_hex(charColIdle[i]),0);
    lv_obj_set_style_border_color(charBtns[i],lv_color_hex(charColSel[i]),0);
    lv_obj_set_style_border_width(charBtns[i],2,0);
    lv_obj_add_event_cb(charBtns[i],charBtnCb,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    lv_obj_t* lbl=lv_label_create(charBtns[i]);
    lv_label_set_text(lbl,charNames[i]);
    lv_obj_set_style_text_color(lbl,lv_color_hex(COL_TEXT_DARK),0);
    lv_obj_center(lbl);
  }
  lv_obj_set_style_bg_color(charBtns[CHAR_MARIO],lv_color_hex(COL_MARIO_SEL),0);
  btnStart=lv_button_create(scr);
  lv_obj_set_size(btnStart,180,44);lv_obj_set_pos(btnStart,(480-180)/2,214);
  lv_obj_set_style_radius(btnStart,22,0);
  lv_obj_set_style_bg_color(btnStart,lv_color_hex(COL_START_OFF),0);
  lv_obj_clear_flag(btnStart,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btnStart,startBtnCb,LV_EVENT_CLICKED,NULL);
  lv_obj_t* startLbl=lv_label_create(btnStart);
  lv_label_set_text(startLbl,"START");
  lv_obj_set_style_text_color(startLbl,lv_color_hex(0xF1EFE8),0);
  lv_obj_center(startLbl);
}

// Chaque élément du jeu (sol, joueur, HUD) = un lv_obj_t*
// Chaque frame : on met à jour position/taille/couleur des objets

#define SCREEN_W     480
#define SCREEN_H     272
#define PLAYER_W      16
#define PLAYER_H      24
#define GROUND_VISUAL_Y  200

// Couleur ciel (fond de l'écran de jeu)
#define SKY_COLOR    0xB3E5FC  // bleu ciel pastel
#define GROUND_COLOR 0x4A7C3F  // vert herbe foncé
#define GRASS_COLOR  0x66BB6A  // vert herbe clair (bande du dessus)

// Couleurs des personnages
static const uint32_t playerColors[3] = { 0xE53935, 0x43A047, 0x1E88E5 };

// Caméra
static float cameraX = 0.0f;

// Objets LVGL du jeu — créés une fois dans showGame(), mis à jour dans renderGame()
static lv_obj_t* objSky      = nullptr; // fond ciel (couleur de l'écran)
static lv_obj_t* objGround   = nullptr; // rectangle sol vert foncé
static lv_obj_t* objGrass    = nullptr; // bande d'herbe claire (haut du sol)
static lv_obj_t* objPlayer   = nullptr; // corps du joueur
static lv_obj_t* objHead     = nullptr; // tête du joueur (carré chair)
static lv_obj_t* lblScore    = nullptr; // HUD score
static lv_obj_t* lblLives    = nullptr; // HUD vies
static lv_obj_t* lblLevel    = nullptr; // HUD niveau
static lv_obj_t* lblDebug    = nullptr; // coordonnées debug (à enlever plus tard)

// ═══ AJOUT : objets supplémentaires pour les sprites détaillés ═══
// Chaque partie du personnage = un lv_obj_t séparé.
// On les déplace tous ensemble dans renderGame() via un offset commun.
// Mario & Luigi : chapeau, cheveux x2, yeux x2, moustache, salopette bras x2,
//                 jambe gauche, jambe droite, chaussure gauche, chaussure droite
// Toad          : chapeau (bord haut + corps + bord bas), taches x3,
//                 visage, yeux x2, nez, gilet, bras x2,
//                 jambe gauche, jambe droite, chaussure gauche, chaussure droite
static lv_obj_t* spHat       = nullptr; // chapeau principal (Mario/Luigi) ou corps chapeau (Toad)
static lv_obj_t* spHatTop    = nullptr; // haut du chapeau Mario/Luigi (bande étroite) — bord haut rouge Toad
static lv_obj_t* spHatBrim   = nullptr; // bord bas chapeau (Mario/Luigi) — bord bas blanc Toad
static lv_obj_t* spHair[2]   = {nullptr,nullptr}; // touffes de cheveux (Mario/Luigi) — taches 1&2 Toad
static lv_obj_t* spSpot3     = nullptr; // 3e tache Toad (pas utilisé Mario/Luigi)
static lv_obj_t* spEye[2]    = {nullptr,nullptr}; // yeux
static lv_obj_t* spMustache  = nullptr; // moustache (Mario/Luigi) — nez Toad
static lv_obj_t* spShirt     = nullptr; // chemise/gilet
static lv_obj_t* spArm[2]    = {nullptr,nullptr}; // bras gauche et droit (peau)
static lv_obj_t* spLegL      = nullptr; // jambe gauche
static lv_obj_t* spLegR      = nullptr; // jambe droite
static lv_obj_t* spLegMid    = nullptr; // jonction entre les deux jambes
static lv_obj_t* spShoeL     = nullptr; // chaussure gauche
static lv_obj_t* spShoeR     = nullptr; // chaussure droite
// ═══ FIN AJOUT variables sprites ═══

// Helper : crée un rectangle simple sans bordure ni padding
static lv_obj_t* makeRect(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  // Désactive les interactions (pas de scroll, pas de clic)
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

// Helper : crée un label HUD (texte blanc en haut de l'écran)
static lv_obj_t* makeHudLabel(lv_obj_t* parent, int x, int y, int w) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_pos(lbl, x, y);
  lv_obj_set_width(lbl, w);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x212121), 0);
  lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0); // fond transparent
  return lbl;
}

// ═══ AJOUT : helper moveSprite — déplace un sprite à (bx+dx, by+dy) ═══
// bx/by = position de base du personnage sur l'écran
// dx/dy = décalage relatif à cette base (défini dans drawPlayer)
static void moveSprite(lv_obj_t* obj, int bx, int by, int dx, int dy) {
  if (obj) lv_obj_set_pos(obj, bx + dx, by + dy);
}

// ═══ AJOUT : drawPlayer — construit le sprite du personnage ═══
// Appelé UNE SEULE FOIS depuis initGameObjects().
// S=2 : 1 cellule pixel-art = 2×2 px écran.
//
// SYSTÈME DE COORDONNÉES :
//   player.y = position Y du BAS des chaussures (touche le sol).
//   On définit "by" dans renderGame() comme :
//     by = screenPY - SPRITE_H_PX + SHOE_BOTTOM_ROW*S
//   où SPRITE_H_PX = hauteur totale du sprite en px
//   et SHOE_BOTTOM_ROW = ligne pixel-art du bas des chaussures.
//   Ainsi les chaussures touchent exactement player.y → le perso est sur le sol.
//
// Pour Mario/Luigi : sprite 12 col × 13 row → 24×26 px. Chaussures : bas row 12.
// Pour Toad        : sprite 12 col × 15 row → 24×30 px. Chaussures : bas row 14.
static void drawPlayer(lv_obj_t* scr) {
  const int S = 2; // 1 cellule = 2px écran

  int c = player.characterId;

  // ── Couleurs selon personnage ──────────────────────────────────────
  uint32_t colHat   = (c==CHAR_LUIGI) ? 0x388E3C : 0xE53935;
  uint32_t colPants = 0x1565C0;
  uint32_t colShoes = 0x5D4037;
  uint32_t colSkin  = 0xFFCDD2;
  uint32_t colHair  = (c==CHAR_TOAD)  ? 0xFFFFFF : 0x5D4037;
  uint32_t colEyes  = 0x212121;
  uint32_t colNose  = 0xFF8A80;
  uint32_t colBrim  = 0xEEEEEE;

  if (c == CHAR_MARIO || c == CHAR_LUIGI) {
    // ── MARIO / LUIGI  (grille 12×13, S=2 → 24×26 px) ────────────
    spHatTop  = makeRect(scr, 0,0,  6*S, 2*S, colHat );  // col3..8  row0..1
    spHat     = makeRect(scr, 0,0,  8*S, 2*S, colHat );  // col2..9  row1..2
    spHatBrim = nullptr;
    objHead   = makeRect(scr, 0,0,  6*S, 3*S, colSkin);  // col3..8  row2..4
    spHair[0] = makeRect(scr, 0,0,  2*S, 1*S, colHair);  // col3..4  row2
    spHair[1] = makeRect(scr, 0,0,  2*S, 1*S, colHair);  // col7..8  row2
    spEye[0]  = makeRect(scr, 0,0,  1*S, 1*S, colEyes);  // col4     row3
    spEye[1]  = makeRect(scr, 0,0,  1*S, 1*S, colEyes);  // col7     row3
    spMustache= makeRect(scr, 0,0,  6*S, 1*S, colHair);  // col3..8  row4
    spShirt   = makeRect(scr, 0,0,  8*S, 3*S, colHat );  // col2..9  row5..7
    spArm[0]  = makeRect(scr, 0,0,  1*S, 2*S, colSkin);  // col1     row5..6
    spArm[1]  = makeRect(scr, 0,0,  1*S, 2*S, colSkin);  // col10    row5..6
    spLegL    = makeRect(scr, 0,0,  3*S, 3*S, colPants); // col2..4  row8..10
    spLegR    = makeRect(scr, 0,0,  3*S, 3*S, colPants); // col7..9  row8..10
    spLegMid  = makeRect(scr, 0,0,  4*S, 2*S, colPants); // col4..7  row8..9
    spShoeL   = makeRect(scr, 0,0,  3*S, 2*S, colShoes); // col2..4  row11..12
    spShoeR   = makeRect(scr, 0,0,  3*S, 2*S, colShoes); // col7..9  row11..12
    spSpot3   = nullptr;

  } else {
    // ── TOAD  (grille 12×15, S=2 → 24×30 px) ─────────────────────
    spHatTop  = makeRect(scr, 0,0, 12*S, 1*S, colHat );  // col0..11 row0
    spHat     = makeRect(scr, 0,0, 12*S, 3*S, colHat );  // col0..11 row1..3
    spHatBrim = makeRect(scr, 0,0, 10*S, 1*S, colBrim);  // col1..10 row4
    spHair[0] = makeRect(scr, 0,0,  3*S, 2*S, colHair);  // col1..3  row1..2
    spHair[1] = makeRect(scr, 0,0,  2*S, 2*S, colHair);  // col5..6  row1..2
    spSpot3   = makeRect(scr, 0,0,  3*S, 2*S, colHair);  // col8..10 row1..2
    objHead   = makeRect(scr, 0,0,  8*S, 3*S, colSkin);  // col2..9  row4..6
    spEye[0]  = makeRect(scr, 0,0,  2*S, 1*S, colEyes);  // col3..4  row5
    spEye[1]  = makeRect(scr, 0,0,  2*S, 1*S, colEyes);  // col7..8  row5
    spMustache= makeRect(scr, 0,0,  2*S, 1*S, colNose);  // col5..6  row6 (nez)
    spShirt   = makeRect(scr, 0,0,  8*S, 3*S, colPants); // col2..9  row7..9
    spArm[0]  = makeRect(scr, 0,0,  1*S, 2*S, colSkin);  // col1     row7..8
    spArm[1]  = makeRect(scr, 0,0,  1*S, 2*S, colSkin);  // col10    row7..8
    spLegL    = makeRect(scr, 0,0,  3*S, 3*S, 0xFFFFFF); // col2..4  row10..12
    spLegR    = makeRect(scr, 0,0,  3*S, 3*S, 0xFFFFFF); // col7..9  row10..12
    spLegMid  = makeRect(scr, 0,0,  4*S, 2*S, 0xFFFFFF); // col4..7  row10..11
    spShoeL   = makeRect(scr, 0,0,  3*S, 2*S, colShoes); // col2..4  row13..14
    spShoeR   = makeRect(scr, 0,0,  3*S, 2*S, colShoes); // col7..9  row13..14
  }

  // objPlayer pointe sur spShirt pour que renderGame() teste != nullptr
  objPlayer = spShirt;
}
// ═══ FIN AJOUT drawPlayer ═══

// Crée tous les objets du jeu (appelé une fois depuis showGame)
void initGameObjects(lv_obj_t* scr) {
  // Fond ciel = couleur de l'écran lui-même
  lv_obj_set_style_bg_color(scr, lv_color_hex(SKY_COLOR), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // LVGL 9 ajoute du padding par défaut sur lv_scr_act().
  // On le supprime pour que lv_obj_set_pos(obj, 0, 0) = coin haut-gauche réel.
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);

  // Sol (rectangle pleine largeur, du bas de l'écran jusqu'à GROUND_VISUAL_Y)
  objGround = makeRect(scr, 0, GROUND_VISUAL_Y,
                       SCREEN_W, SCREEN_H - GROUND_VISUAL_Y,
                       GROUND_COLOR);

  // Bande d'herbe claire sur le bord haut du sol (4px)
  objGrass = makeRect(scr, 0, GROUND_VISUAL_Y, SCREEN_W, 4, GRASS_COLOR);

  // Labels HUD — z-order : créés après les rectangles = dessinés par-dessus
  lblScore = makeHudLabel(scr, 8,   4, 160);  lv_label_set_text(lblScore, "Score: 000000");
  lblLives = makeHudLabel(scr, 210, 4,  60);  lv_label_set_text(lblLives, "x3");
  lblLevel = makeHudLabel(scr, 400, 4,  70);  lv_label_set_text(lblLevel, "Niv.1");
  lblDebug = makeHudLabel(scr, 8, SCREEN_H-16, 140); lv_label_set_text(lblDebug, "x:0 y:0");

  // ═══ AJOUT : construit le sprite détaillé (remplace objPlayer/objHead basiques) ═══
  drawPlayer(scr);
  // Sécurité : si player.y n'est pas encore initialisé (vaut 0),
  // on le force sur le sol pour éviter le flash en haut de l'écran.
  if (player.y < GROUND_Y) player.y = GROUND_Y;
  if (player.x < 50.0f)    player.x = 50.0f;
  // Force une première position correcte dès la création :
  // sans ça le sprite reste à (0,0) jusqu'à la 1ère frame de renderGame()
  // ce qui provoque un flash en haut à gauche.
  renderGame();
  // ═══ FIN AJOUT ═══
}

// Met à jour la position/couleur des objets — appelé chaque frame
void renderGame() {
  if (!objPlayer) return;

  //  Caméra 
  // Cible : joueur au tiers gauche de l'écran
  float targetX = player.x - (float)SCREEN_W / 3.0f;
  if (targetX < 0.0f) targetX = 0.0f;
  // Interpolation douce (lerp) : 15% de l'écart par frame → ~10 frames pour rattraper
  cameraX += (targetX - cameraX) * 0.15f;

  // Position du joueur à l'écran
  // screenX = worldX - cameraX  (conversion monde → écran)
  int screenPX = (int)(player.x - cameraX);

  // ═══ AJOUT : calcul de by — bas des chaussures = niveau du sol ═══
  // player.y est le bas des pieds (= GROUND_Y quand sur le sol).
  // screenPY = position Y écran du bas des pieds.
  // On remonte de (hauteur totale sprite) pour trouver le haut du chapeau.
  // Mario/Luigi : 13 rows × S=2 = 26px de haut
  // Toad        : 15 rows × S=2 = 30px de haut
  const int S = 2;
  int spriteH = (player.characterId == CHAR_TOAD) ? 15*S : 13*S;
  int screenPY = (int)(player.y); // bas des pieds en Y écran
  // by = coin haut-gauche du sprite (= haut du chapeau)
  // bx = bord gauche du sprite (col 0)
  int by = screenPY - spriteH;
  int bx = screenPX;             // col 0 = bord gauche du sprite
  // ═══ FIN AJOUT calcul by ═══

  // ═══ AJOUT : déplace tous les sous-sprites du personnage ═══
  int c = player.characterId;

  if (c == CHAR_MARIO || c == CHAR_LUIGI) {
    moveSprite(spHatTop,   bx, by,  3*S,  0*S); // col3, row0
    moveSprite(spHat,      bx, by,  2*S,  1*S); // col2, row1
    moveSprite(objHead,    bx, by,  3*S,  2*S); // col3, row2
    moveSprite(spHair[0],  bx, by,  3*S,  2*S); // col3, row2
    moveSprite(spHair[1],  bx, by,  7*S,  2*S); // col7, row2
    moveSprite(spEye[0],   bx, by,  4*S,  3*S); // col4, row3
    moveSprite(spEye[1],   bx, by,  7*S,  3*S); // col7, row3
    moveSprite(spMustache, bx, by,  3*S,  4*S); // col3, row4
    moveSprite(spShirt,    bx, by,  2*S,  5*S); // col2, row5
    moveSprite(spArm[0],   bx, by,  1*S,  5*S); // col1, row5
    moveSprite(spArm[1],   bx, by, 10*S,  5*S); // col10,row5
    moveSprite(spLegL,     bx, by,  2*S,  8*S); // col2, row8
    moveSprite(spLegR,     bx, by,  7*S,  8*S); // col7, row8
    moveSprite(spLegMid,   bx, by,  4*S,  8*S); // col4, row8
    moveSprite(spShoeL,    bx, by,  2*S, 11*S); // col2, row11
    moveSprite(spShoeR,    bx, by,  7*S, 11*S); // col7, row11
  } else {
    moveSprite(spHatTop,   bx, by,  0*S,  0*S); // col0, row0
    moveSprite(spHat,      bx, by,  0*S,  1*S); // col0, row1
    moveSprite(spHatBrim,  bx, by,  1*S,  4*S); // col1, row4
    moveSprite(spHair[0],  bx, by,  1*S,  1*S); // col1, row1
    moveSprite(spHair[1],  bx, by,  5*S,  1*S); // col5, row1
    moveSprite(spSpot3,    bx, by,  8*S,  1*S); // col8, row1
    moveSprite(objHead,    bx, by,  2*S,  4*S); // col2, row4
    moveSprite(spEye[0],   bx, by,  3*S,  5*S); // col3, row5
    moveSprite(spEye[1],   bx, by,  7*S,  5*S); // col7, row5
    moveSprite(spMustache, bx, by,  5*S,  6*S); // col5, row6
    moveSprite(spShirt,    bx, by,  2*S,  7*S); // col2, row7
    moveSprite(spArm[0],   bx, by,  1*S,  7*S); // col1, row7
    moveSprite(spArm[1],   bx, by, 10*S,  7*S); // col10,row7
    moveSprite(spLegL,     bx, by,  2*S, 10*S); // col2, row10
    moveSprite(spLegR,     bx, by,  7*S, 10*S); // col7, row10
    moveSprite(spLegMid,   bx, by,  4*S, 10*S); // col4, row10
    moveSprite(spShoeL,    bx, by,  2*S, 13*S); // col2, row13
    moveSprite(spShoeR,    bx, by,  7*S, 13*S); // col7, row13
  }
  // ═══ FIN AJOUT déplacement sprites ═══

  // HUD
  char buf[48];
  snprintf(buf, sizeof(buf), "Score: %06d", player.score);
  lv_label_set_text(lblScore, buf);

  snprintf(buf, sizeof(buf), "x%d", player.lives);
  lv_label_set_text(lblLives, buf);

  snprintf(buf, sizeof(buf), "Niv.%d", currentLevel + 1);
  lv_label_set_text(lblLevel, buf);

  // Debug : position monde du joueur (à commenter une fois le jeu stable)
  snprintf(buf, sizeof(buf), "x:%.0f y:%.0f", player.x, player.y);
  lv_label_set_text(lblDebug, buf);
}

// Lance l'écran de jeu (appelé depuis startBtnCb)
void showGame() {
  lv_obj_t* scr = lv_scr_act();

  // Remet la caméra au début du niveau
  cameraX = 0.0f;

  // Remet les pointeurs à zéro (au cas où on reviendrait du menu)
  objSky=nullptr; objGround=nullptr; objGrass=nullptr;
  objPlayer=nullptr; objHead=nullptr;
  lblScore=nullptr; lblLives=nullptr; lblLevel=nullptr; lblDebug=nullptr;

  // ═══ AJOUT : remet aussi les sprites à zéro ═══
  spHat=nullptr; spHatTop=nullptr; spHatBrim=nullptr;
  spHair[0]=nullptr; spHair[1]=nullptr; spSpot3=nullptr;
  spEye[0]=nullptr; spEye[1]=nullptr;
  spMustache=nullptr; spShirt=nullptr;
  spArm[0]=nullptr; spArm[1]=nullptr;
  spLegL=nullptr; spLegR=nullptr; spLegMid=nullptr;
  spShoeL=nullptr; spShoeR=nullptr;
  // ═══ FIN AJOUT ═══

  // Crée tous les objets visuels du jeu
  initGameObjects(scr);
}

// à décommenter pour tester la démo
// #include "demos/lv_demos.h"

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
  xLastWakeTime=xTaskGetTickCount();
  while(1)
  {
    updateInputs();
    InputState inputs=getInputs();

    if(currentScreen==SCREEN_JOYSTICK_TEST) {
      updateJoystickTest(inputs);
    }
    else if(currentScreen==SCREEN_GAME) {
      updateGame(inputs);  // physique
      renderGame();        // rendu
    }

    Serial.print("X: "); Serial.print(inputs.joyX);
    Serial.print(" Y: "); Serial.print(inputs.joyY);
    Serial.print(" Jump: "); Serial.print(inputs.buttonJump);
    Serial.print(" Down: "); Serial.println(inputs.buttonDown);
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