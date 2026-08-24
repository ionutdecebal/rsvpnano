#pragma once

#include <Arduino.h>
#include <esp_http_server.h>
#include <esp_log.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "companion/http/CompanionHttp.h"
#include "locales/LocaleCatalog.h"
#include "settings/SettingsGlaze.h"
#include "settings/SettingsStore.h"
#include "storage/StorageManager.h"
#include "storage/migration/Migration.h"
#include "timer/FocusTimers.h"
#include "ui/Ui.h"
#include "ui/screens/LibraryScreen.h"
#include "ui/screens/ReaderScreen.h"
#include "ui/screens/Screens.h"

class CompanionSerial;

class CompanionApi {
public:
    CompanionApi(settings::SettingsStore& settingsStore, StorageManager& storage, locales::Catalog& localeCatalog,
                 ui::Context& ui, screens::ReaderScreen& readerScreen, screens::InterfaceScreen& interfaceScreen,
                 screens::NetworkScreen& networkScreen, screens::LibraryScreen& libraryScreen,
                 screens::FocusScreen& focusScreen) :
            settingsStore_(settingsStore),
            storage_(storage),
            localeCatalog_(localeCatalog),
            ui_(ui),
            readerScreen_(readerScreen),
            interfaceScreen_(interfaceScreen),
            networkScreen_(networkScreen),
            libraryScreen_(libraryScreen),
            focusScreen_(focusScreen) {}

    bool begin();
    void end();
    [[nodiscard]] bool active() const;
    [[nodiscard]] std::string_view statusLine1() const;
    [[nodiscard]] std::string_view statusLine2() const;

private:
    friend class CompanionSerial;

    // TODO(feat/lang): Move endpoint operations into one companion::operations namespace and call those typed
    // operations from both CompanionApi and CompanionSerial after the in-flight HTTP API work is merged.
    using FeedList = std::vector<std::string>;
    using OperationResult = std::expected<void, std::string>;

    // Network and server lifecycle
    OperationResult startStation();
    OperationResult startAccessPoint();
    OperationResult startNetworkEvents();
    void stopNetworkEvents();
    void queueNetworkState();
    static void applyNetworkState(void* context);
    OperationResult startMdns();
    void stopMdns();
    OperationResult startServer();
    void stopServer();
    void drainServer();
    static void notifyServerDrained(void* context);

    // Device
    companion::api::Result<companion::api::DeviceInfo> getDevice(httpd_req_t& request);
    [[nodiscard]] companion::api::DeviceInfo deviceInfo() const;
    companion::api::Result<StorageMigration::Report> repairStorage(httpd_req_t& request);

    // Library
    companion::api::Result<std::string> installLibraryItem(httpd_req_t& request);
    companion::api::Result<> deleteLibraryItem(httpd_req_t& request);
    companion::api::Result<> putBookPosition(httpd_req_t& request);
    companion::api::Result<> putBookLanguageFonts(httpd_req_t& request);

    // Themes, fonts, and locales
    companion::api::Result<std::span<const ui::themes::Theme>> getThemes(httpd_req_t& request);
    companion::api::Result<companion::api::Located<ui::themes::Theme>> postTheme(httpd_req_t& request);
    companion::api::Result<> deleteTheme(httpd_req_t& request);
    companion::api::Result<std::span<const FontCatalog::Family>> getFonts(httpd_req_t& request);
    companion::api::Result<companion::api::Located<FontCatalog::Family>> postFont(httpd_req_t& request);
    companion::api::Result<> deleteFont(httpd_req_t& request);
    companion::api::Result<std::span<const locales::InstalledPack>> getLocales(httpd_req_t& request);
    companion::api::Result<companion::api::Located<locales::InstalledPack>> postLocale(httpd_req_t& request);
    companion::api::Result<> deleteLocale(httpd_req_t& request);

    // Active appearance selections
    companion::api::Result<> putThemeSelection(httpd_req_t& request);
    companion::api::Result<> putFontSelection(httpd_req_t& request);
    companion::api::Result<> putLocaleSelection(httpd_req_t& request);

    // Reader settings
    companion::api::Result<> patchReadingSettings(httpd_req_t& request);
    companion::api::Result<> patchDisplaySettings(httpd_req_t& request);
    companion::api::Result<> patchUpdateSettings(httpd_req_t& request);

    // Network configuration and reader content
    companion::api::Result<const settings::NetworkSettings*> getNetwork(httpd_req_t& request);
    companion::api::Result<> putNetwork(httpd_req_t& request);
    companion::api::Result<> updateNetwork(companion::api::NetworkUpdate update);
    companion::api::Result<> deleteNetwork(httpd_req_t& request);
    companion::api::Result<FeedList> getFeeds(httpd_req_t& request);
    companion::api::Result<> putFeeds(httpd_req_t& request);
    companion::api::Result<std::span<const focus::Timer>> getFocusTimers(httpd_req_t& request);
    companion::api::Result<> putFocusTimers(httpd_req_t& request);

    // HTTP transport
    static esp_err_t handleLibrary(httpd_req_t* request);
    static esp_err_t handleLibraryInstall(httpd_req_t* request);
    static esp_err_t handleSettings(httpd_req_t* request);
    static esp_err_t handleOptions(httpd_req_t* request);
    static esp_err_t handleNotFound(httpd_req_t* request, httpd_err_code_t error);

    template<auto endpoint>
    static esp_err_t handle(httpd_req_t* request) {
        if (request == nullptr || request->handle == nullptr)
            return ESP_ERR_INVALID_ARG;

        auto* self = static_cast<CompanionApi*>(httpd_get_global_user_ctx(request->handle));
        if (self == nullptr)
            return ESP_ERR_INVALID_STATE;
        if (!self->active())
            return ESP_ERR_INVALID_STATE;
        if (!self->browserOriginAllowed(*request)) {
            return self->sendError(*request,
                                   companion::api::httpError(HTTP_CODE_FORBIDDEN, "origin_forbidden",
                                                             "This browser origin is not allowed"));
        }

        if (request->content_len != 0 && (request->method == HTTP_GET || request->method == HTTP_DELETE)) {
            return self->sendError(*request,
                                   companion::api::httpError(HTTP_CODE_BAD_REQUEST, "unexpected_body",
                                                             "This endpoint does not accept a request body",
                                                             std::nullopt, companion::api::ConnectionPolicy::Close));
        }

        const std::lock_guard operationLock{self->operationsMutex_};
        auto result = (self->*endpoint)(*request);
        if (!result)
            return self->sendError(*request, std::move(result.error()));

        using Value = typename std::remove_cvref_t<decltype(result)>::value_type;
        if constexpr (std::is_void_v<Value>) {
            return self->sendNoContent(*request);
        } else if constexpr (companion::api::isLocated<Value>) {
            return self->sendLocated(*request, std::move(result).value());
        } else {
            return self->sendData(*request, std::move(result).value());
        }
    }

    esp_err_t sendJson(httpd_req_t& request, t_http_codes status, std::string_view json,
                       companion::api::ConnectionPolicy connection = companion::api::ConnectionPolicy::KeepAlive);
    esp_err_t sendLibrary(httpd_req_t& request);
    esp_err_t sendSettings(httpd_req_t& request);
    esp_err_t sendError(httpd_req_t& request, companion::api::HttpError error);
    esp_err_t sendNoContent(httpd_req_t& request);
    esp_err_t setBrowserResponseHeaders(httpd_req_t& request);
    [[nodiscard]] bool browserOriginAllowed(httpd_req_t& request) const;

    template<typename T>
    companion::api::Result<std::string> encodeResponse(const T& data) {
        std::string json;
        return companion::api::encode(data, json)
            .transform([&json] {
                return std::move(json);
            })
            .transform_error([](std::string message) {
                ESP_LOGE("companion", "response encode failed: %s", message.c_str());
                return companion::api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "encode_failed",
                                                 "Response could not be encoded");
            });
    }

    template<typename T>
    esp_err_t sendData(httpd_req_t& request, const T& data) {
        auto json = encodeResponse(data);
        if (!json)
            return sendError(request, std::move(json.error()));
        return sendJson(request, HTTP_CODE_OK, *json);
    }

    template<typename T>
    esp_err_t sendLocated(httpd_req_t& request, companion::api::Located<T> response) {
        auto json = encodeResponse(response.value.get());
        if (!json)
            return sendError(request, std::move(json.error()));
        if (const esp_err_t error = httpd_resp_set_hdr(&request, "Location", response.location.c_str());
            error != ESP_OK) {
            return error;
        }
        return sendJson(request, HTTP_CODE_CREATED, *json);
    }

    template<typename T>
    companion::api::Result<T> readJson(httpd_req_t& request, size_t maximum, std::string_view tooLargeMessage,
                                       T value = {}) {
        return readBody(request, maximum, tooLargeMessage)
            .and_then([value = std::move(value)](std::string body) mutable -> companion::api::Result<T> {
                return companion::api::decode<T>(body, std::move(value)).transform_error([](std::string message) {
                    return companion::api::httpError(HTTP_CODE_BAD_REQUEST, "invalid_json", std::move(message));
                });
            });
    }

    companion::api::Result<std::string> readBody(httpd_req_t& request, size_t maximum,
                                                 std::string_view tooLargeMessage);
    [[nodiscard]] std::optional<std::string> queryParameter(httpd_req_t& request, std::string_view name) const;
    companion::api::Result<std::string> requiredQueryParameter(httpd_req_t& request, std::string_view name,
                                                               std::string_view missingMessage) const;
    companion::api::Result<std::string> routeId(const httpd_req_t& request, std::string_view prefix,
                                                std::string_view suffix = {}) const;

    // Shared domain helpers
    companion::api::Result<std::string> encodeBook(size_t index, const BookMetadata* availableMetadata = nullptr,
                                                   const reading::BookIdentity* availableIdentity = nullptr);
    companion::api::Result<std::string> readSelectionId(httpd_req_t& request);
    void storeNetwork(std::string ssid, std::string password);
    [[nodiscard]] std::string deviceSuffix() const;
    [[nodiscard]] std::optional<size_t> findBookIndex(std::string_view id) const;

    httpd_handle_t server_ = nullptr;
    std::atomic_bool active_ = false;
    std::atomic_bool stationConnected_ = false;
    std::mutex networkStateMutex_;
    std::mutex operationsMutex_;
    std::string accessPointSsid_;
    settings::SettingsStore& settingsStore_;
    StorageManager& storage_;
    locales::Catalog& localeCatalog_;
    ui::Context& ui_;
    screens::ReaderScreen& readerScreen_;
    screens::InterfaceScreen& interfaceScreen_;
    screens::NetworkScreen& networkScreen_;
    screens::LibraryScreen& libraryScreen_;
    screens::FocusScreen& focusScreen_;
    std::string statusLine1_ = "Idle";
    std::string statusLine2_;
    std::string stationUrl_;
    size_t wifiEventId_ = 0;
    bool mdnsStarted_ = false;
};
