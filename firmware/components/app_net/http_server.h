#pragma once
#include "esp_err.h"
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t http_server_start(app_config_t *cfg);

#ifdef __cplusplus
}
#endif
