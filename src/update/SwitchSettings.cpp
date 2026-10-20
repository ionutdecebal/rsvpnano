#include "update/SwitchSettings.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <glaze/glaze.hpp>

std::expected<void, std::string> OtaUpdater::finalizeInstall(settings::SettingsStore& store, const Offer& offer) {
    const auto* partition = esp_ota_get_next_update_partition(nullptr);
    esp_app_desc_t description{};
    if (!partition || esp_ota_get_partition_description(partition, &description) != ESP_OK)
        return std::unexpected("Verified firmware slot unavailable");
    auto& active = store.settings().updates;
    const auto previous = active;
    auto fail = [](const char* message) -> std::expected<void, std::string> { return std::unexpected(message); };
    if (offer.switching && (!previous.repositoryOwner.empty() || !previous.releaseTag.empty())) {
        // Keep a recoverable copy of an advanced source before an explicit switch.
        auto json = glz::write_json(previous);
        Preferences backup;
        if (!json || !backup.begin("rsvp_updates")) return fail("Could not back up custom update source");
        const char* key = std::string_view{update::kCurrentChannel} == "new-ui" ? "new-ui-source" : "classic-source";
        const bool saved = backup.putString(key, json->c_str()) == json->size();
        backup.end();
        if (!saved) return fail("Could not back up custom update source");
    }
    if (offer.switching) {
        active.repositoryOwner.clear(); active.releaseTag.clear();
        store.acceptChanges();
        if (auto flushed = store.flush(); !flushed) {
            active = previous; store.acceptChanges(); store.flush();
            return fail("Could not save update selection; switch cancelled");
        }
    }
    if (esp_ota_set_boot_partition(partition) != ESP_OK) {
        if (offer.switching) { active = previous; store.acceptChanges(); store.flush(); }
        return fail("Could not select new firmware; current software retained");
    }
    return {};
}
