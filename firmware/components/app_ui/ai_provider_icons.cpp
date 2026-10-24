#include "ai_provider_icons.h"
#include <string.h>

#include "ai_provider_icons.inc"

#define ICON_DSC(name, size) \
    static const lv_img_dsc_t name##_##size = { \
        .header = {.cf = LV_IMG_CF_ALPHA_1BIT, .always_zero = 0, .reserved = 0, .w = size, .h = size}, \
        .data_size = sizeof(s_##name##_##size##_alpha), .data = s_##name##_##size##_alpha \
    }

ICON_DSC(chatgpt, 16); ICON_DSC(chatgpt, 24); ICON_DSC(chatgpt, 32);
ICON_DSC(devin, 16); ICON_DSC(devin, 24); ICON_DSC(devin, 32);
ICON_DSC(claude, 16); ICON_DSC(claude, 24); ICON_DSC(claude, 32);
ICON_DSC(mimo, 16); ICON_DSC(mimo, 24); ICON_DSC(mimo, 32);

const lv_img_dsc_t *ai_provider_icon(const char *provider_id, int size) {
    const char *id = provider_id ? provider_id : "chatgpt";
    if (size != 16 && size != 24 && size != 32) size = 24;
#define PICK(name) if (strcmp(id, #name) == 0) return size == 16 ? &name##_16 : size == 32 ? &name##_32 : &name##_24
    PICK(devin);
    PICK(claude);
    PICK(mimo);
    return size == 16 ? &chatgpt_16 : size == 32 ? &chatgpt_32 : &chatgpt_24;
#undef PICK
}
