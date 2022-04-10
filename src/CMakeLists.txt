# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# idf_component_register(SRCS ${app_sources})

idf_component_register(SRCS "lv_screen.c" "main.cpp")
