cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

#подключаем LVGL и другие компоненты
add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE)

list(APPEND EXTRA_COMPONENT_DIRS 
#    "${CMAKE_SOURCE_DIR}/src/"
    "${CMAKE_SOURCE_DIR}/components/lvgl/" 
    "${CMAKE_SOURCE_DIR}/components/lvgl_esp32_drivers/lvgl_tft/" 
    "${CMAKE_SOURCE_DIR}/components/lvgl_esp32_drivers/lvgl_touch/" 
    "${CMAKE_SOURCE_DIR}/components/lvgl_esp32_drivers/")


project(esp32-ais)
