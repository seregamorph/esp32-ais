/*
Проект собирается в VisualStudio code с плагином Platformio

Используется UI библиотека LVGL
Настройку компонент библиотеки см https://github.com/lvgl/lv_port_esp32 раздел "Use LVGL in your ESP-IDF project"

Для запуска конфигуратора ESP32 в консоли (значок Platformio на тулбаре слева->Miscellaneous->New terminal) выполнить:
pio run -t menuconfig

в конфигураторе ESP32 настроить подключение тачдисплея - параметры SPI, см детали в lv_screen.c

*/
#include <math.h>
#include <float.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "sys/unistd.h" //для usleep
#include "soc/rtc.h"

#include "lv_screen.h"


#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define TAG "MAIN"



void _print_chip_info() {
    ESP_LOGI(TAG, "Free heap %d kb, minimum %d kb", esp_get_free_heap_size()>>10, esp_get_minimum_free_heap_size()>>10);
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "CPU core %d", chip_info.cores);
       
    //установить частоту работы CPU
    //void rtc_clk_cpu_freq_set_config(const rtc_cpu_freq_config_t* config);
    rtc_cpu_freq_config_t cpu_freq;
    rtc_clk_cpu_freq_get_config(&cpu_freq);
    ESP_LOGI(TAG, "CPU freq=%d MHz, source freq=%d MHz, divider=%d", cpu_freq.freq_mhz, cpu_freq.source_freq_mhz, cpu_freq.div);
    ESP_LOGI(TAG, "APB freq=%0.2f MHz", rtc_clk_apb_freq_get()/(1000.0*1000));
    
    ESP_LOGI(TAG, "ESP-IDF version [%s]", esp_get_idf_version());
}


void _set_loggers() {
    esp_log_level_set("*", ESP_LOG_ERROR);  //установить уровень логгирования для всех компонент ERROR
    
    esp_log_level_set(TAG, ESP_LOG_INFO);

    esp_log_level_set("LVSCRN", ESP_LOG_INFO);
    esp_log_level_set("XPT2046", ESP_LOG_INFO);
    esp_log_level_set("disp_spi", ESP_LOG_INFO);
    esp_log_level_set("lvgl_helpers", ESP_LOG_INFO);
}



//точка входа в программу
void app_main() {
    _set_loggers();

    _print_chip_info();

    lv_screen_init();

}