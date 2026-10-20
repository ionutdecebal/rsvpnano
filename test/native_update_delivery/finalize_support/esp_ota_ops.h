#pragma once
#include "settings/SettingsStore.h"
struct esp_partition_t {};
struct esp_app_desc_t {};
inline constexpr int ESP_OK = 0;
inline const esp_partition_t* esp_ota_get_next_update_partition(const void*) {
    static esp_partition_t slot;
    return fake::partition ? &slot : nullptr;
}
inline int esp_ota_get_partition_description(const esp_partition_t*, esp_app_desc_t*) { return ESP_OK; }
inline int esp_ota_set_boot_partition(const esp_partition_t*) {
    fake::events.push_back("boot");
    return fake::bootWrite ? ESP_OK : -1;
}
