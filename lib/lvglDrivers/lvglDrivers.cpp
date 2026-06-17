#include "lvglDrivers.h"
#include "lv_conf.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_ts.h"
#include <string.h>

static SemaphoreHandle_t lvglMutex;

bool lvglLock(TickType_t xBlockTime)
{
    if (xSemaphoreTake(lvglMutex, xBlockTime) == pdTRUE)
    {
        return true;
    }
    return false;
}

bool lvglUnlock()
{
    if (xSemaphoreGive(lvglMutex) == pdTRUE)
    {
        return true;
    }
    return false;
}

static void lvglTask(void *pvParameters)
{
    while (1)
    {
        xSemaphoreTake(lvglMutex, portMAX_DELAY);
        uint32_t time_till_next = lv_timer_handler();
        xSemaphoreGive(lvglMutex);
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

static void my_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    // BUG FIX (performance — c'est la cause probable du lag, et des
    // "freeze" si une zone rafraîchie est assez grande pour bloquer trop
    // longtemps) : cette fonction dessinait CHAQUE PIXEL un par un via
    // BSP_LCD_DrawPixel — qui elle-même rappelle BSP_LCD_GetXSize() à
    // CHAQUE pixel. Pour un simple rectangle de 64×16px (1024 pixels),
    // ça fait plus de 2000 appels de fonction. Le calque LCD est
    // configuré en ARGB8888 32 bits (BSP_LCD_LayerDefaultInit, dans
    // setup() plus bas) — EXACTEMENT le même format que LV_COLOR_DEPTH=32
    // utilisé par LVGL (voir lv_conf.h). Comme les deux formats sont
    // identiques, on peut copier chaque LIGNE entière d'un seul coup avec
    // memcpy, directement dans le framebuffer (en SDRAM, à
    // LCD_FB_START_ADDRESS), au lieu d'écrire pixel par pixel — un seul
    // appel par ligne au lieu d'un par pixel.
    const int32_t screenW = (int32_t)BSP_LCD_GetXSize();
    uint32_t *fb = (uint32_t *)LCD_FB_START_ADDRESS;
    uint32_t *src = (uint32_t *)px_map;
    int32_t rowPixels = area->x2 - area->x1 + 1;

    for (int32_t y = area->y1; y <= area->y2; y++)
    {
        uint32_t *dstRow = fb + ((int32_t)y * screenW + area->x1);
        memcpy(dstRow, src, (size_t)rowPixels * sizeof(uint32_t));
        src += rowPixels;
    }

    // IMPORTANT!!!
    // Inform LVGL that you are ready with the flushing and buf is not used anymore
    lv_display_flush_ready(display);
}

static void my_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    TS_StateTypeDef TS_State;
    BSP_TS_GetState(&TS_State);

    if (TS_State.touchDetected != 0)
    {
        data->point.x = TS_State.touchX[0];
        data->point.y = TS_State.touchY[0];
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Start");

    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS);

    BSP_TS_Init(480, 272);

    lvglMutex = xSemaphoreCreateMutex();

    lv_init();

    lv_log_register_print_cb([](lv_log_level_t level, const char *buf)
                             { Serial.printf("%s", buf); });

    lv_display_t *display = lv_display_create(480, 272);

    lv_display_set_flush_cb(display, my_flush_cb);

    static uint32_t buf[480 * 272 / 10];

    lv_display_set_buffers(display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_read_cb);

    lv_tick_set_cb(xTaskGetTickCount);

    mySetup();

    // On nomme maintenant les deux taches (au lieu de NULL) : ca ne change
    // rien au comportement, mais ca permet au hook de depassement de pile
    // (vApplicationStackOverflowHook dans STM32FreeRTOS.c) de dire LAQUELLE
    // des deux a deborde, via un clignotement different selon le nom recu.
    //
    // Pile augmentee de 16384 a 20480 mots (64 Ko -> 80 Ko, +16 Ko chacune) :
    // marge de securite modeste en attendant de savoir, grace au diagnostic
    // ci-dessus, laquelle des deux taches a reellement deborde. A 80 Ko x 2 =
    // 160 Ko sur les ~199 Ko habituellement disponibles (LV_MEM_SIZE = 64 Ko
    // dans lv_conf.h), il reste encore une marge confortable pour le reste.
    xTaskCreate(lvglTask, "lvglTask", 20480, NULL, osPriorityNormal, NULL);
    xTaskCreate(myTask, "myTask", 20480, NULL, osPriorityNormal, NULL);

    vTaskStartScheduler();
    Serial.println("Insufficient RAM");
    while (1)
        ;
}