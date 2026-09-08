#include "companion/http/CompanionApi.h"

#include <glaze/json.hpp>

#include <utility>

#include "board/BoardDisplay.h"

namespace api = companion::api;

esp_err_t CompanionApi::handleSettings(httpd_req_t* request) {
    if (request == nullptr || request->handle == nullptr)
        return ESP_ERR_INVALID_ARG;

    auto* self = static_cast<CompanionApi*>(httpd_get_global_user_ctx(request->handle));
    if (self == nullptr || !self->active())
        return ESP_ERR_INVALID_STATE;
    if (!self->browserOriginAllowed(*request))
        return self->sendError(*request, api::httpError(HTTP_CODE_FORBIDDEN, "origin_forbidden", "This browser origin is not allowed"));
    if (request->content_len != 0) {
        return self->sendError(*request, api::httpError(HTTP_CODE_BAD_REQUEST, "unexpected_body",
                                                        "This endpoint does not accept a request body", std::nullopt,
                                                        api::ConnectionPolicy::Close));
    }
    const std::lock_guard operationLock{self->operationsMutex_};
    return self->sendSettings(*request);
}

esp_err_t CompanionApi::sendSettings(httpd_req_t& request) {
    const auto& settings = settingsStore_.settings();
    const auto response =
        glz::obj{"reading", settings.reading, "interface", settings.interface, "updates", settings.updates};
    auto json = encodeResponse(response);
    if (!json)
        return sendError(request, std::move(json.error()));
    return sendJson(request, HTTP_CODE_OK, *json);
}

api::Result<> CompanionApi::patchReadingSettings(httpd_req_t& request) {
    return readJson(request, settings::kMaxSettingsBytes, "Settings payload exceeds 8 KB",
                    settingsStore_.settings().reading)
        .transform([this](settings::ReadingSettings reading) {
            reading.typography.fontId = settingsStore_.settings().reading.typography.fontId;
            const bool leftHanded = settingsStore_.settings().reading.leftHanded;
            settingsStore_.settings().reading = std::move(reading);
            settingsStore_.acceptChanges();
            readerScreen_.releaseRuntimeCaches();
            if (leftHanded != settingsStore_.settings().reading.leftHanded) {
                ui_.setOrientation(settingsStore_.settings().reading.leftHanded
                                       ? Board::Display::rotatedUiOrientation()
                                       : Board::Display::defaultUiOrientation());
            }
        });
}

api::Result<> CompanionApi::patchDisplaySettings(httpd_req_t& request) {
    return readJson(request, settings::kMaxSettingsBytes, "Display settings payload exceeds 8 KB",
                    settingsStore_.settings().interface)
        .transform([this](settings::InterfaceSettings interface) {
            interface.locale = settingsStore_.settings().interface.locale;
            interface.selectedThemeId = settingsStore_.settings().interface.selectedThemeId;
            settingsStore_.settings().interface = std::move(interface);
            settingsStore_.acceptChanges();
            Board::Display::setBrightness(settingsStore_.settings().interface.brightnessPercent);
        });
}

api::Result<> CompanionApi::patchUpdateSettings(httpd_req_t& request) {
    return readJson(request, settings::kMaxSettingsBytes, "Update settings payload exceeds 8 KB",
                    settingsStore_.settings().updates)
        .transform([this](settings::UpdateSettings updates) {
            settingsStore_.settings().updates = std::move(updates);
            settingsStore_.acceptChanges();
            networkScreen_.begin(settingsStore_);
            networkScreen_.startupCheckPending = false;
        });
}
