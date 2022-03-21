/*

библиотека LVGL

https://lvgl.io/
https://github.com/espressif/esp-iot-solution/blob/master/documents/hmi_solution/littlevgl/littlevgl_guide_en.md
https://github.com/lvgl/lv_platformio
https://github.com/lvgl/lv_port_esp32  актуальное портирование под ESP32

дисплей ILI9341 2,8 дюйма + тачпад на XPT2046
дисплей ILI9488 3,5 дюйма + тачпад на XPT2046
шины SPI дисплея и тачпада раздельные
частота шины XPT2046 не должна превышать 2 МГц, для дисплея - 8 МГц (разные конфиги!)

необходимо соединить выводы ESP32 и дисплея согласно таблице + настроить значения выводов в menuconfig
пины дисплей            пины ESP32 devkit1
тачпад
T_IRQ                   D27  
T_DO (MISO)             D19
T_DIN (MOSI)            D23
T_CS                    D25
T_CLK (SCK)             D18
дисплей
SDO (MISO)              <не используется>
LED (подсветка)         D15 (3.3v)
SCK (CLK)               D14
SDI (MOSI)              D13
DC                      D26
RESET                   D4
CS                      D5
GND                     GND
Vcc                     3.3v




управление ШИМ (PWM) реализовано аппаратно
см доку https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html

три этапа инициализации:
1 сконфигурировать таймер, установить частоту ШИМ (до десятков МГц при низком разрешении) и его разрешение (до 16 бит)
2 сконфигурировать канал и связать его с GPIO
3 настроить ШИМ сигнал: скважность

на этом принципе нужно будет настроить управление яркостью дисплея
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <float.h>
#include <math.h>

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "sys/unistd.h" //для usleep

#include "lvgl.h"
#include "lvgl_helpers.h"

#include "lv_screen.h"


/************объявления переменных************/

#define TAG "LVSCRN"

//период обновления тиков в lvgl 
//см https://docs.lvgl.io/v7/en/html/porting/tick.html
#define LV_TICK_PERIOD_MS 10

#ifndef DISP_HOR_RES
    #define DISP_HOR_RES 480
#endif
#ifndef DISP_VERT_RES
    #define DISP_VERT_RES 320
#endif

//описатель буфера дисплея
static lv_disp_draw_buf_t disp_buf;



//два буфера второй буфер опциональный
static lv_color_t buf_1[DISP_HOR_RES * 10];
static lv_color_t buf_2[DISP_HOR_RES * 10];
//static lv_color_t *buf_1;
//static lv_color_t *buf_2;


//описатель драйвера дисплея
static lv_disp_drv_t disp_drv;

//описатель устройства ввода (тачскрин)
static lv_indev_drv_t indev_drv;


#define WAIT_TICK_COUNT portMAX_DELAY //кол-во тиков, которое будем ожидать для получения семафора

#define BACKLIGHT_PIN GPIO_NUM_15 //пин для управляения яркостью дисплея

//таймер с обработчик тиков
esp_timer_handle_t volatile periodic_tick_timer;
//таймер с обработчик тасок
esp_timer_handle_t volatile periodic_task_timer;

//семафор для синхронизации потоков
SemaphoreHandle_t xGuiSemaphore;




/************реализация************/

//счетчик тиков для lvgl
void _tick_task(void *arg) {
    //см https://docs.lvgl.io/v7/en/html/porting/tick.html    
    if (xSemaphoreTake(xGuiSemaphore, (TickType_t)WAIT_TICK_COUNT) == pdTRUE) {
        lv_tick_inc(LV_TICK_PERIOD_MS);
        xSemaphoreGive(xGuiSemaphore);
    }     
}

/**
 * бесконечный обработчик событий GUI
 */
void _process_task(void *arg) {
    //см https://docs.lvgl.io/v7/en/html/porting/task-handler.html    
    if (xSemaphoreTake(xGuiSemaphore, (TickType_t)WAIT_TICK_COUNT) == pdTRUE) {
        lv_task_handler();
        xSemaphoreGive(xGuiSemaphore);
    } 
}


//рисует линию между двумя точками
void _draw_line(const lv_point_t p1, const lv_point_t p2) {
    static lv_point_t line_points[2] = {0};
    line_points[0] = p1;
    line_points[1] = p2;

    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 4);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_line_rounded(&style_line, true);

    lv_obj_t * line1;
    line1 = lv_line_create(lv_scr_act());
    lv_line_set_points(line1, line_points, 2);   
    lv_obj_add_style(line1, &style_line, 0);
    lv_obj_center(line1);

    ESP_LOGI(TAG, "Line x1=%d, y1=%d, x2=%d, y2=%d", 
        line_points[0].x, line_points[0].y,
        line_points[1].x, line_points[1].y); 
}


//обработчик клика по изображению шкалы сенсора
void _screen_click_handler(lv_event_t *event) {
    //ESP_LOGI(TAG, "Event screen");

    lv_event_code_t code = lv_event_get_code(event);
    //lv_obj_t * btn = lv_event_get_target(e);

    //получить координаты точки события
    lv_point_t pos;
    lv_indev_get_point(lv_indev_get_act(), &pos);

    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "CLICK x=%d, y=%d", pos.x, pos.y);
    }
    else if (code == LV_EVENT_SCROLL_BEGIN) {
        ESP_LOGI(TAG, "SCROLL_BEGIN x=%d, y=%d", pos.x, pos.y);
    }   
    else if (code == LV_EVENT_SCROLL_END) {
        ESP_LOGI(TAG, "SCROLL_END x=%d, y=%d", pos.x, pos.y);
    }
    else if (code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "PRESSED x=%d, y=%d", pos.x, pos.y);
    }   
    else if (code == LV_EVENT_PRESSING) {
        ESP_LOGI(TAG, "PRESSING x=%d, y=%d", pos.x, pos.y);        
    }
    else if (code == LV_EVENT_RELEASED) {
        ESP_LOGI(TAG, "RELEASED x=%d, y=%d", pos.x, pos.y);   
        static lv_point_t p1 = {0,0};
        _draw_line(p1, pos);     
    }
}


void lv_example_btn_1(void)
{
    lv_obj_t * label;

    lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);

    label = lv_label_create(btn1);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);

    lv_obj_t * btn2 = lv_btn_create(lv_scr_act());
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_height(btn2, LV_SIZE_CONTENT);

    label = lv_label_create(btn2);
    lv_label_set_text(label, "Toggle");
    lv_obj_center(label);
}


/*
главный экран приложения
*/
void _create_main_screen() {
    /*
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 5);

    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    static lv_grad_dsc_t grad;
    grad.dir = LV_GRAD_DIR_VER;
    grad.stops_count = 2;
    grad.stops[0].color = lv_palette_lighten(LV_PALETTE_GREY, 1);
    grad.stops[1].color = lv_palette_main(LV_PALETTE_BLUE);

    grad.stops[0].frac  = 128;
    grad.stops[1].frac  = 192;

    lv_style_set_bg_grad(&style, &grad);

    lv_obj_t * obj = lv_obj_create(lv_scr_act());
    lv_obj_add_style(obj, &style, 0);
    lv_obj_center(obj);
    */

    //lv_obj_set_click(lv_scr_act(), true);
    lv_obj_add_event_cb(lv_scr_act(), _screen_click_handler, LV_EVENT_ALL, NULL);

    lv_disp_set_bg_color(lv_disp_get_default(), lv_color_make(255, 0, 0)); 

    lv_example_btn_1();
}





/*
инициализирует GUI библиотеку и дисплей
*/
void lv_screen_init(void) {
    xGuiSemaphore = xSemaphoreCreateMutex();

    //_lv_init_backlight_pin(BACKLIGHT_PIN);
    //gpio_set_level(BACKLIGHT_PIN, 0);

    ESP_LOGI(TAG, "LVGL version: %d.%d.%d %s", 
        lv_version_major(), lv_version_minor(), lv_version_patch(), 
        lv_version_info());

    //инициализация пошагово описана тут https://docs.lvgl.io/master/porting/project.html#initialization
    //1. инициализируем GUI библиотеку
    lv_init();

    //2. инициализируем драйвер дисплея
    lvgl_driver_init();
    ESP_LOGI(TAG, "LVGL SPI driver init");

    //3.1. инициализируем  дисплей и регистрируем его в библиотеке
    //см https://docs.lvgl.io/master/porting/display.html

    //сначала инициализируем буферы и переменные типа lv_disp_draw_buf_t и lv_disp_drv_t 

    /*устарело, актуально для LVGL v7 
    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_buf_t disp_buf;
    uint32_t size_in_px = DISP_BUF_SIZE;
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);
    */
    
    //buf_1 = heap_caps_malloc(DISP_HOR_RES*10 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    //assert(buf_1 != NULL);
    //buf_2 = heap_caps_malloc(DISP_HOR_RES*10 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    //assert(buf_2 != NULL);

    lv_disp_draw_buf_init(&disp_buf, buf_1, buf_2, DISP_HOR_RES*10);
    ESP_LOGI(TAG, "LVGL buffer init");

    //затем инициализируем драйвер дисплея
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.draw_buf = &disp_buf; 
    disp_drv.hor_res = DISP_HOR_RES;            
    disp_drv.ver_res = DISP_VERT_RES;               

    lv_disp_drv_register(&disp_drv); 
    ESP_LOGI(TAG, "LVGL display driver init");



    //3.2. инициализируем устройство ввода (тачпад/тачскрин) и регистрируем его в библиотеке
    //инициализация устройства ввода обязательно после регистрации дисплея!
    //см https://docs.lvgl.io/master/porting/indev.html
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
    ESP_LOGI(TAG, "LVGL touch driver init");


    //4. устаналиваем тики по таймеру
    //см https://docs.lvgl.io/v7/en/html/porting/tick.html
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &_tick_task,
        .name = "periodic_tick"
    };
    
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_tick_timer, LV_TICK_PERIOD_MS * 1000));

    //5. устаналиваем обработку тасок
    const esp_timer_create_args_t periodic_timer_task_args = {
        .callback = &_process_task,
        .name = "periodic_task"
    };

    //usleep(1000);//небольшая пауза, чтобы разнести по времени старт двух таймеров
    
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_task_args, &periodic_task_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_task_timer, LV_TICK_PERIOD_MS * 1000));

    
    //создаем экран
    _create_main_screen();
    ESP_LOGI(TAG, "Screen created");

/*
    //_lv_init_backlight_pin(BACKLIGHT_PIN);

    //esp_err_t e = gpio_set_level(BACKLIGHT_PIN, 1);
    //ESP_LOGI(TAG, "BACKLIGHT_PIN %d set level=%d, error=%d", BACKLIGHT_PIN, 1, e);
    _lv_init_pwm(BACKLIGHT_PIN);

    ledc_set_fade_time_and_start(
        LEDC_HIGH_SPEED_MODE,
        LEDC_CHANNEL_0,
        256,
        5*1000,//задаем время затухания в мс
        LEDC_FADE_WAIT_DONE//ожидаем завершения
    );
*/
    
    //запомним основной экран
    //main_screen = lv_scr_act(); 
}