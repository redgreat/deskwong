#include "aiusage_service.h"
#include "http_util.h"
#include <assert.h>
#include <string.h>
static const char *response;
cJSON *http_get_json(const char *url, const char *token) {
    assert(strcmp(url, "http://mock/ai/usage") == 0);
    return response ? cJSON_Parse(response) : NULL;
}
int main(void) {
    ai_provider_t providers[3]; int count = 0;
    aiusage_service_init("http://mock", "test");
    response = "{\"data\":{\"providers\":["
        "{\"id\":\"chatgpt_5h\",\"window_minutes\":300,\"remaining_percent\":72,\"resets_at\":1789223400},"
        "{\"id\":\"chatgpt_weekly\",\"window_minutes\":10080,\"used_percent\":56,\"resets_at\":1789741800},"
        "{\"id\":\"deepseek\",\"display\":\"12345\"}]}}";
    assert(aiusage_service_fetch(providers, 3, &count) == 0 && count == 3);
    assert(providers[0].window_minutes == 300 && providers[0].remaining_percent == 72);
    assert(providers[0].resets_at == 1789223400LL);
    assert(providers[1].window_minutes == 10080 && providers[1].remaining_percent == 44);
    assert(providers[2].window_minutes == 0 && providers[2].resets_at == 0 && providers[2].remaining_percent == -1);
    response = "{\"data\":{\"providers\":[{\"window_minutes\":\"bad\",\"resets_at\":null}]}}";
    assert(aiusage_service_fetch(providers, 3, &count) == 0 && count == 1);
    assert(providers[0].window_minutes == 0 && providers[0].resets_at == 0);
    response = NULL;
    assert(aiusage_service_fetch(providers, 3, &count) != 0);
    return 0;
}
