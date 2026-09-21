cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "0.3.0-weather-service")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(deskwong)

# Keep LVGL's fixed TLSF pool out of internal RAM needed by WiFi and DMA.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_include_directories(${lvgl_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/components/lvgl_bsp")
target_compile_definitions(${lvgl_lib} PRIVATE
    LV_MEM_POOL_INCLUDE="lvgl_memory_pool.h"
    LV_MEM_POOL_ALLOC=deskwong_lvgl_pool)
