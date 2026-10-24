#pragma once
#include <stdbool.h>
#include "aiusage_service.h"

void ai_usage_screen_init(int width, int height);
void ai_usage_screen_update(const ai_provider_t *windows, int count);
void ai_usage_screen_show(void);
void ai_usage_screen_hide(void);
bool ai_usage_screen_visible(void);
