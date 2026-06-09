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

// variables globales de l'écran de test 
// globaux pour que updateJoystickTest() puisse es modifier chaque frame depuis myTask().
// "nullptr" = pointeur vide, sera rempli dans showJoystickTest().
static lv_obj_t* joyCursor    = nullptr; // point vert mobile
static lv_obj_t* joyCircle    = nullptr; // grand cercle de référence
static lv_obj_t* labelRaw     = nullptr; // valeurs brutes X/Y
static lv_obj_t* labelNorm    = nullptr; // valeurs normalisées
static lv_obj_t* labelDir     = nullptr; // direction textuelle
static lv_obj_t* labelButtons = nullptr; // état boutons

// Appelé quand l'utilisateur touche "OK" sur l'écran tactile.
// Supprime l'écran de test et affiche le menu (testLvgl pour l'instant).
static void joyTestOkCb(lv_event_t* e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    // Nettoie les pointeurs avant de supprimer l'écran
    joyCursor    = nullptr;
    joyCircle    = nullptr;
    labelRaw     = nullptr;
    labelNorm    = nullptr;
    labelDir     = nullptr;
    labelButtons = nullptr;

    // Charge un écran vierge puis y affiche le menu
    // lv_scr_load() + lv_obj_clean() est plus sûr que lv_obj_del() sur l'écran actif
    lv_obj_clean(lv_scr_act());
    currentScreen = SCREEN_MENU;
    testLvgl(); // remplacé par showMenu() à l'étape suivante
  }
}

//création de l'écran de test
// Constantes de mise en page (écran STM32F746 = 480 x 272 pixels)
// Le cercle est à gauche, les labels à droite.
#define JOY_CIRCLE_R  90   // rayon du cercle en px
#define JOY_CIRCLE_CX 110  // centre X du cercle depuis bord gauche
#define JOY_CURSOR_R   8   // rayon du point curseur

void showJoystickTest()
{
  lv_obj_t* scr = lv_scr_act(); // on travaille sur l'écran actif existant

  // --- Titre ---
  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Test joystick");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  // --- Grand cercle (zone de déplacement visuelle) ---
  // Un objet carré avec radius = LV_RADIUS_CIRCLE devient un cercle parfait.
  // Taille = diamètre = JOY_CIRCLE_R * 2
  joyCircle = lv_obj_create(scr);
  lv_obj_set_size(joyCircle, JOY_CIRCLE_R * 2, JOY_CIRCLE_R * 2);
  lv_obj_set_style_radius(joyCircle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(joyCircle, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_bg_opa(joyCircle, LV_OPA_40, 0);
  lv_obj_set_style_border_color(joyCircle, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(joyCircle, 2, 0);
  lv_obj_set_style_pad_all(joyCircle, 0, 0); // retire le padding par défaut
  // Positionné à gauche, centré verticalement
  lv_obj_align(joyCircle, LV_ALIGN_LEFT_MID, 10, 10);

  // --- Cercle de zone morte (cercle intérieur) ---
  // Rayon en pixels : on projette JOY_DEADZONE sur le rayon du cercle.
  // JOY_DEADZONE=80, demi-plage ADC=511 → ratio = 80/511 ≈ 0.156 → 14px
  int dzPx = (int)(((float)JOY_DEADZONE / ((float)JOY_MAX / 2.0f)) * JOY_CIRCLE_R);
  lv_obj_t* dzCircle = lv_obj_create(scr);
  lv_obj_set_size(dzCircle, dzPx * 2, dzPx * 2);
  lv_obj_set_style_radius(dzCircle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dzCircle, lv_color_hex(0x999999), 0);
  lv_obj_set_style_bg_opa(dzCircle, LV_OPA_50, 0);
  lv_obj_set_style_border_width(dzCircle, 1, 0);
  lv_obj_set_style_border_color(dzCircle, lv_color_hex(0x555555), 0);
  lv_obj_set_style_pad_all(dzCircle, 0, 0);
  // Centré sur le grand cercle
  lv_obj_align_to(dzCircle, joyCircle, LV_ALIGN_CENTER, 0, 0);

  // --- Curseur (point vert mobile) ---
  // Commence au centre. Position mise à jour dans updateJoystickTest().
  joyCursor = lv_obj_create(scr);
  lv_obj_set_size(joyCursor, JOY_CURSOR_R * 2, JOY_CURSOR_R * 2);
  lv_obj_set_style_radius(joyCursor, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(joyCursor, lv_color_hex(0x1D9E75), 0); // vert
  lv_obj_set_style_border_width(joyCursor, 0, 0);
  lv_obj_set_style_pad_all(joyCursor, 0, 0);
  // Centré sur le grand cercle au départ
  lv_obj_align_to(joyCursor, joyCircle, LV_ALIGN_CENTER, 0, 0);

  // --- Labels (côté droit, x=220) ---
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

  // --- Bouton OK (bas droite) ---
  lv_obj_t* btnOk = lv_button_create(scr);
  lv_obj_align(btnOk, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_add_event_cb(btnOk, joyTestOkCb, LV_EVENT_ALL, NULL);
  lv_obj_t* lblOk = lv_label_create(btnOk);
  lv_label_set_text(lblOk, "OK -> menu");
  lv_obj_center(lblOk);
}


//mise à jour de l'écran de test (appelée à 60 FPS)
void updateJoystickTest(InputState& in)
{
  // Sécurité : si les objets n'existent plus (après appui OK), on sort
  if (joyCursor == nullptr || joyCircle == nullptr) return;

  // --- Position du curseur ---
  // normalizedX/Y ∈ [-1.0, +1.0]
  // On multiplie par JOY_CIRCLE_R pour obtenir le décalage en pixels.
  // Ex : nX=+1.0 → curseur à +90px du centre = bord droit du cercle
  int offsetX = (int)(in.normalizedX() * JOY_CIRCLE_R);
  int offsetY = (int)(in.normalizedY() * JOY_CIRCLE_R);

  // Repositionne le curseur relativement au centre du grand cercle
  lv_obj_align_to(joyCursor, joyCircle, LV_ALIGN_CENTER, offsetX, offsetY);

  // Change la couleur : gris dans la zone morte, vert sinon
  if (offsetX == 0 && offsetY == 0)
    lv_obj_set_style_bg_color(joyCursor, lv_color_hex(0x999999), 0);
  else
    lv_obj_set_style_bg_color(joyCursor, lv_color_hex(0x1D9E75), 0);

  // --- Labels ---
  char buf[48];

  snprintf(buf, sizeof(buf), "X:  %4d\nY:  %4d", in.joyX, in.joyY);
  lv_label_set_text(labelRaw, buf);

  snprintf(buf, sizeof(buf), "nX: %+.2f\nnY: %+.2f",
           in.normalizedX(), in.normalizedY());
  lv_label_set_text(labelNorm, buf);

  // Direction textuelle
  if (offsetX == 0 && offsetY == 0) {
    lv_label_set_text(labelDir, "Zone morte");
  } else {
    char dir[32] = "";
    if (in.isLeft())  strcat(dir, "GAUCHE ");
    if (in.isRight()) strcat(dir, "DROITE ");
    if (in.isUp())    strcat(dir, "HAUT ");
    if (in.isDown())  strcat(dir, "BAS ");
    lv_label_set_text(labelDir, dir);
  }

  snprintf(buf, sizeof(buf), "JUMP:%d  DOWN:%d",
           (int)in.buttonJump, (int)in.buttonDown);
  lv_label_set_text(labelButtons, buf);
}


// à décommenter pour tester la démo
// #include "demos/lv_demos.h"

void mySetup()
{
  // à décommenter pour tester la démo
  // lv_demo_widgets();
  initInputs();

  //remplace testLvgl() par showJoystickTest() au démarrage ═══
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

    // ═══ AJOUT DÉBUT : dispatch selon l'écran courant ═══
    if (currentScreen == SCREEN_JOYSTICK_TEST)
    {
      updateJoystickTest(inputs); // met à jour curseur + labels
    }
    // SCREEN_MENU : LVGL gère seul (callbacks tactiles)
    // SCREEN_GAME : sera ajouté à l'étape suivante
    // ═══ AJOUT FIN ═══

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