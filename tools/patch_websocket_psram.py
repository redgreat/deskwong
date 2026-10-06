from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: patch_websocket_psram.py <esp_websocket_client.c>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    text = path.read_text(encoding="utf-8")
    marker = "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT"
    if marker in text:
        return 0

    include_old = '#include "esp_system.h"\n'
    include_new = include_old + '#include "esp_heap_caps.h"\n'
    create_old = '''    if (xTaskCreatePinnedToCore(esp_websocket_client_task, client->config->task_name ? client->config->task_name : "websocket_task",
                                client->config->task_stack, client, client->config->task_prio, &client->task_handle, client->config->task_core_id) != pdTRUE) {
'''
    create_new = '''    BaseType_t task_created = xTaskCreatePinnedToCoreWithCaps(
        esp_websocket_client_task,
        client->config->task_name ? client->config->task_name : "websocket_task",
        client->config->task_stack, client, client->config->task_prio,
        &client->task_handle, client->config->task_core_id,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (task_created != pdTRUE) {
        task_created = xTaskCreatePinnedToCore(
            esp_websocket_client_task,
            client->config->task_name ? client->config->task_name : "websocket_task",
            client->config->task_stack, client, client->config->task_prio,
            &client->task_handle, client->config->task_core_id);
    }
    if (task_created != pdTRUE) {
'''
    if include_old not in text or create_old not in text:
        print(f"unsupported esp_websocket_client source: {path}", file=sys.stderr)
        return 1
    text = text.replace(include_old, include_new, 1).replace(create_old, create_new, 1)
    path.write_text(text, encoding="utf-8", newline="\n")
    print(f"patched WebSocket task stack allocation: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
