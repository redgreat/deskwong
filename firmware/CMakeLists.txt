cmake_minimum_required(VERSION 3.16)

# Release builds inject DESKWONG_VERSION from the Git tag. Local builds are
# visibly marked as development images and therefore cannot masquerade as a
# published firmware version.
if(DEFINED ENV{DESKWONG_VERSION} AND NOT "$ENV{DESKWONG_VERSION}" STREQUAL "")
    set(PROJECT_VER "$ENV{DESKWONG_VERSION}")
    string(REGEX REPLACE "^v" "" PROJECT_VER "${PROJECT_VER}")
else()
    execute_process(
        COMMAND git describe --tags --match "v[0-9]*" --always --dirty
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
        OUTPUT_VARIABLE PROJECT_VER
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    string(REGEX REPLACE "^v" "" PROJECT_VER "${PROJECT_VER}")
    if(PROJECT_VER STREQUAL "")
        set(PROJECT_VER "0.0.0-dev")
    elseif(PROJECT_VER MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        set(PROJECT_VER "${PROJECT_VER}-dev")
    endif()
endif()
message(STATUS "deskwong firmware version: ${PROJECT_VER}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(deskwong)

# The upstream WebSocket component always creates its task in internal RAM.
# Voice keeps this task resident, which starves WiFi and LCD DMA on ESP32-S3.
# Apply an idempotent source patch after Component Manager resolves dependencies.
set(WEBSOCKET_CLIENT_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__esp_websocket_client/esp_websocket_client.c")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../tools/patch_websocket_psram.py"
            "${WEBSOCKET_CLIENT_SOURCE}"
    RESULT_VARIABLE WEBSOCKET_PATCH_RESULT)
if(NOT WEBSOCKET_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to enable PSRAM stack for esp_websocket_client")
endif()

# Keep LVGL's fixed TLSF pool out of internal RAM needed by WiFi and DMA.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_include_directories(${lvgl_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/components/lvgl_bsp")
target_compile_definitions(${lvgl_lib} PRIVATE
    LV_MEM_POOL_INCLUDE="lvgl_memory_pool.h"
    LV_MEM_POOL_ALLOC=deskwong_lvgl_pool)
