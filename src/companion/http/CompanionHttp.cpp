#include "companion/http/CompanionApi.h"
#include "companion/serial/CompanionBufferedRequest.h"
#include <esp_log.h>
#include <glaze/net/url.hpp>

#include <array>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <string_view>
#include <utility>

namespace {

    namespace api = companion::api;

    [[nodiscard]] std::string_view requestPath(const httpd_req_t& request) {
        if (request.uri == nullptr)
            return {};
        const std::string_view uri{request.uri};
        return uri.substr(0, uri.find('?'));
    }

    [[nodiscard]] httpd_uri_t makeRoute(const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t route{};
        route.uri = uri;
        route.method = method;
        route.handler = handler;
        return route;
    }

    bool matchRoute(const char* route, const char* uri, size_t length) {
        if (route == nullptr || uri == nullptr)
            return false;

        constexpr std::string_view parameter = "{id}";
        const std::string_view pattern{route};
        std::string_view path{uri, length};
        path = path.substr(0, path.find('?'));

        if (pattern.ends_with('*'))
            return path.starts_with(pattern.substr(0, pattern.size() - 1));

        const size_t parameterOffset = pattern.find(parameter);
        if (parameterOffset == std::string_view::npos)
            return path == pattern;

        const std::string_view prefix = pattern.substr(0, parameterOffset);
        const std::string_view suffix = pattern.substr(parameterOffset + parameter.size());
        if (!path.starts_with(prefix) || !path.ends_with(suffix) || path.size() <= prefix.size() + suffix.size()) {
            return false;
        }

        const std::string_view value = path.substr(prefix.size(), path.size() - prefix.size() - suffix.size());
        return !value.contains('/');
    }

    [[nodiscard]] std::optional<std::string> requestHeader(httpd_req_t& request, const char* name) {
        const size_t length = httpd_req_get_hdr_value_len(&request, name);
        if (length == 0)
            return std::nullopt;

        std::string value(length + 1, '\0');
        if (httpd_req_get_hdr_value_str(&request, name, value.data(), value.size()) != ESP_OK)
            return std::nullopt;
        value.resize(length);
        return value;
    }

    [[nodiscard]] bool isLocalDevelopmentOrigin(std::string_view origin, std::string_view host) {
        for (const std::string_view scheme: {std::string_view{"http://"}, std::string_view{"https://"}}) {
            const std::string prefix = std::string{scheme} + std::string{host};
            if (origin == prefix)
                return true;
            if (!origin.starts_with(prefix) || origin.size() <= prefix.size() + 1 || origin[prefix.size()] != ':')
                continue;
            const std::string_view port = origin.substr(prefix.size() + 1);
            if (std::all_of(port.begin(), port.end(), [](char value) { return value >= '0' && value <= '9'; }))
                return true;
        }
        return false;
    }

    [[nodiscard]] bool isAllowedOrigin(std::string_view origin) {
        return origin == "https://ionutdecebal.github.io" || isLocalDevelopmentOrigin(origin, "localhost") ||
               isLocalDevelopmentOrigin(origin, "127.0.0.1") || isLocalDevelopmentOrigin(origin, "[::1]");
    }

} // namespace

CompanionApi::OperationResult CompanionApi::startServer() {
    // Keep the HTTP owner task for the firmware lifetime. Stopping it while an already-dispatched
    // Wi-Fi event is queueing work can invalidate the server handle during sync shutdown.
    if (server_ != nullptr)
        return {};

    const std::array routes{
        makeRoute("/api/v2/device", HTTP_GET, &CompanionApi::handle<&CompanionApi::getDevice>),
        makeRoute("/api/v2/storage/repair", HTTP_POST, &CompanionApi::handle<&CompanionApi::repairStorage>),
        makeRoute("/api/v2/library", HTTP_GET, &CompanionApi::handleLibrary),
        makeRoute("/api/v2/library", HTTP_POST, &CompanionApi::handleLibraryInstall),
        makeRoute("/api/v2/library/{id}", HTTP_DELETE, &CompanionApi::handle<&CompanionApi::deleteLibraryItem>),
        makeRoute("/api/v2/library/{id}/position", HTTP_PUT, &CompanionApi::handle<&CompanionApi::putBookPosition>),
        makeRoute("/api/v2/library/{id}/language-fonts", HTTP_PUT,
                  &CompanionApi::handle<&CompanionApi::putBookLanguageFonts>),
        makeRoute("/api/v2/themes", HTTP_GET, &CompanionApi::handle<&CompanionApi::getThemes>),
        makeRoute("/api/v2/themes", HTTP_POST, &CompanionApi::handle<&CompanionApi::postTheme>),
        makeRoute("/api/v2/themes/{id}", HTTP_DELETE, &CompanionApi::handle<&CompanionApi::deleteTheme>),
        makeRoute("/api/v2/fonts", HTTP_GET, &CompanionApi::handle<&CompanionApi::getFonts>),
        makeRoute("/api/v2/fonts", HTTP_POST, &CompanionApi::handle<&CompanionApi::postFont>),
        makeRoute("/api/v2/fonts/{id}", HTTP_DELETE, &CompanionApi::handle<&CompanionApi::deleteFont>),
        makeRoute("/api/v2/locales", HTTP_GET, &CompanionApi::handle<&CompanionApi::getLocales>),
        makeRoute("/api/v2/locales", HTTP_POST, &CompanionApi::handle<&CompanionApi::postLocale>),
        makeRoute("/api/v2/locales/{id}", HTTP_DELETE, &CompanionApi::handle<&CompanionApi::deleteLocale>),
        makeRoute("/api/v2/appearance/theme", HTTP_PUT, &CompanionApi::handle<&CompanionApi::putThemeSelection>),
        makeRoute("/api/v2/appearance/font", HTTP_PUT, &CompanionApi::handle<&CompanionApi::putFontSelection>),
        makeRoute("/api/v2/appearance/locale", HTTP_PUT, &CompanionApi::handle<&CompanionApi::putLocaleSelection>),
        makeRoute("/api/v2/settings", HTTP_GET, &CompanionApi::handleSettings),
        makeRoute("/api/v2/settings/reading", HTTP_PATCH, &CompanionApi::handle<&CompanionApi::patchReadingSettings>),
        makeRoute("/api/v2/settings/display", HTTP_PATCH, &CompanionApi::handle<&CompanionApi::patchDisplaySettings>),
        makeRoute("/api/v2/settings/updates", HTTP_PATCH, &CompanionApi::handle<&CompanionApi::patchUpdateSettings>),
        makeRoute("/api/v2/network", HTTP_GET, &CompanionApi::handle<&CompanionApi::getNetwork>),
        makeRoute("/api/v2/network", HTTP_PUT, &CompanionApi::handle<&CompanionApi::putNetwork>),
        makeRoute("/api/v2/network", HTTP_DELETE, &CompanionApi::handle<&CompanionApi::deleteNetwork>),
        makeRoute("/api/v2/feeds", HTTP_GET, &CompanionApi::handle<&CompanionApi::getFeeds>),
        makeRoute("/api/v2/feeds", HTTP_PUT, &CompanionApi::handle<&CompanionApi::putFeeds>),
        makeRoute("/api/v2/focus-timers", HTTP_GET, &CompanionApi::handle<&CompanionApi::getFocusTimers>),
        makeRoute("/api/v2/focus-timers", HTTP_PUT, &CompanionApi::handle<&CompanionApi::putFocusTimers>),
        makeRoute("/api/v2/*", HTTP_OPTIONS, &CompanionApi::handleOptions),
    };

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.global_user_ctx = this;
    config.max_uri_handlers = static_cast<uint16_t>(routes.size());
    config.stack_size = 12288;
    config.recv_wait_timeout = 15;
    config.send_wait_timeout = 15;
#if defined(BOARD_HAS_PSRAM)
    config.task_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
#endif
    config.uri_match_fn = matchRoute;

    const auto fail = [this](esp_err_t error, std::string_view operation) -> OperationResult {
        ESP_LOGE("companion", "%.*s failed: %s (0x%x)", static_cast<int>(operation.size()), operation.data(),
                 esp_err_to_name(error), error);
        stopServer();
        return std::unexpected(std::string{operation} + ": " + esp_err_to_name(error));
    };

    if (const esp_err_t error = httpd_start(&server_, &config); error != ESP_OK)
        return fail(error, "HTTP server start");

    if (const esp_err_t error = httpd_register_err_handler(server_, HTTPD_404_NOT_FOUND, &CompanionApi::handleNotFound);
        error != ESP_OK) {
        return fail(error, "404 handler registration");
    }

    for (const httpd_uri_t& route: routes) {
        const esp_err_t registerError = httpd_register_uri_handler(server_, &route);
        if (registerError == ESP_OK)
            continue;

        ESP_LOGE("companion", "route registration failed method=%d uri=%s", route.method, route.uri);
        return fail(registerError, "Route registration");
    }

    return {};
}

esp_err_t CompanionApi::handleNotFound(httpd_req_t* request, httpd_err_code_t error) {
    (void) error;
    if (request == nullptr || request->handle == nullptr)
        return ESP_ERR_INVALID_ARG;

    auto* self = static_cast<CompanionApi*>(httpd_get_global_user_ctx(request->handle));
    if (self == nullptr)
        return ESP_ERR_INVALID_STATE;
    if (!self->active())
        return ESP_ERR_INVALID_STATE;
    if (!self->browserOriginAllowed(*request)) {
        return self->sendError(*request,
                               api::httpError(HTTP_CODE_FORBIDDEN, "origin_forbidden",
                                              "This browser origin is not allowed"));
    }

    const api::ConnectionPolicy connection =
        request->content_len == 0 ? api::ConnectionPolicy::KeepAlive : api::ConnectionPolicy::Close;
    return self->sendError(*request, api::httpError(HTTP_CODE_NOT_FOUND, "not_found", "Endpoint not found",
                                                    std::nullopt, connection));
}

esp_err_t CompanionApi::handleOptions(httpd_req_t* request) {
    if (request == nullptr || request->handle == nullptr)
        return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<CompanionApi*>(httpd_get_global_user_ctx(request->handle));
    if (self == nullptr || !self->active())
        return ESP_ERR_INVALID_STATE;
    if (!self->browserOriginAllowed(*request)) {
        return self->sendError(*request,
                               api::httpError(HTTP_CODE_FORBIDDEN, "origin_forbidden",
                                              "This browser origin is not allowed"));
    }

    if (const esp_err_t error = self->setBrowserResponseHeaders(*request); error != ESP_OK)
        return error;
    if (const esp_err_t error = httpd_resp_set_hdr(request, "Access-Control-Allow-Methods",
                                                   "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        error != ESP_OK) {
        return error;
    }
    if (const esp_err_t error = httpd_resp_set_hdr(request, "Access-Control-Allow-Headers", "Content-Type");
        error != ESP_OK) {
        return error;
    }
    if (requestHeader(*request, "Access-Control-Request-Private-Network") == "true") {
        if (const esp_err_t error = httpd_resp_set_hdr(request, "Access-Control-Allow-Private-Network", "true");
            error != ESP_OK) {
            return error;
        }
    }
    return self->sendNoContent(*request);
}

void CompanionApi::stopServer() {
    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
    }
}

void CompanionApi::notifyServerDrained(void* context) {
    xTaskNotifyGive(static_cast<TaskHandle_t>(context));
}

void CompanionApi::drainServer() {
    if (server_ == nullptr)
        return;

    const TaskHandle_t waitingTask = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);
    if (const esp_err_t error = httpd_queue_work(server_, &CompanionApi::notifyServerDrained, waitingTask);
        error != ESP_OK) {
        ESP_LOGW("companion", "could not drain HTTP task: %s", esp_err_to_name(error));
        return;
    }
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000)) == 0)
        ESP_LOGW("companion", "HTTP task drain timed out");
}

esp_err_t CompanionApi::sendJson(httpd_req_t& request, t_http_codes status, std::string_view json,
                                 api::ConnectionPolicy connection) {
    if (const esp_err_t error = setBrowserResponseHeaders(request); error != ESP_OK)
        return error;
    if (const esp_err_t error = httpd_resp_set_hdr(&request, "Cache-Control", "no-store"); error != ESP_OK)
        return error;
    if (connection == api::ConnectionPolicy::Close) {
        if (const esp_err_t error = httpd_resp_set_hdr(&request, "Connection", "close"); error != ESP_OK)
            return error;
    }
    const std::string statusLine = api::httpStatusLine(status);
    if (const esp_err_t error = httpd_resp_set_status(&request, statusLine.c_str()); error != ESP_OK)
        return error;
    if (const esp_err_t error = httpd_resp_set_type(&request, "application/json"); error != ESP_OK)
        return error;

    const esp_err_t sendError = httpd_resp_send(&request, json.data(), static_cast<ssize_t>(json.size()));
    if (sendError != ESP_OK)
        return sendError;

    // A failed handler result makes esp_http_server discard a connection whose body was not consumed.
    return connection == api::ConnectionPolicy::Close ? ESP_FAIL : ESP_OK;
}

esp_err_t CompanionApi::sendError(httpd_req_t& request, api::HttpError error) {
    std::string json;
    if (auto encoded = api::encode(error.error, json); !encoded) {
        ESP_LOGE("companion", "error response encode failed: %s", encoded.error().c_str());
        static constexpr std::string_view fallback =
            R"({"code":"encode_failed","message":"Error response could not be encoded"})";
        return sendJson(request, HTTP_CODE_INTERNAL_SERVER_ERROR, fallback, error.connection);
    }
    return sendJson(request, error.status, json, error.connection);
}

esp_err_t CompanionApi::sendNoContent(httpd_req_t& request) {
    if (const esp_err_t error = setBrowserResponseHeaders(request); error != ESP_OK)
        return error;
    if (const esp_err_t error = httpd_resp_set_hdr(&request, "Cache-Control", "no-store"); error != ESP_OK)
        return error;

    const std::string statusLine = api::httpStatusLine(HTTP_CODE_NO_CONTENT);
    if (const esp_err_t error = httpd_resp_set_status(&request, statusLine.c_str()); error != ESP_OK)
        return error;

    return httpd_resp_send(&request, nullptr, 0);
}

esp_err_t CompanionApi::setBrowserResponseHeaders(httpd_req_t& request) {
    const auto origin = requestHeader(request, "Origin");
    if (!origin || !isAllowedOrigin(*origin))
        return ESP_OK;
    if (const esp_err_t error = httpd_resp_set_hdr(&request, "Access-Control-Allow-Origin", origin->c_str());
        error != ESP_OK) {
        return error;
    }
    return httpd_resp_set_hdr(&request, "Vary", "Origin");
}

bool CompanionApi::browserOriginAllowed(httpd_req_t& request) const {
    const auto origin = requestHeader(request, "Origin");
    return !origin || isAllowedOrigin(*origin);
}

api::Result<std::string> CompanionApi::readBody(httpd_req_t& request, size_t maximum,
                                                std::string_view tooLargeMessage) {
    if (request.content_len > maximum) {
        return std::unexpected(api::httpError(HTTP_CODE_PAYLOAD_TOO_LARGE, "payload_too_large",
                                              std::string{tooLargeMessage}, std::nullopt,
                                              api::ConnectionPolicy::Close));
    }

    std::string body(request.content_len, '\0');
    size_t offset = 0;
    while (offset < body.size()) {
        const int received = companion::bufferedRequest(request) != nullptr
                               ? companion::bufferedRequest(request)->read(
                                     companion::bufferedRequest(request)->readContext,
                                     std::span{reinterpret_cast<uint8_t*>(body.data() + offset), body.size() - offset})
                               : httpd_req_recv(&request, body.data() + offset, body.size() - offset);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            return std::unexpected(api::httpError(HTTP_CODE_REQUEST_TIMEOUT, "request_timeout",
                                                  "Timed out while receiving the request body", std::nullopt,
                                                  api::ConnectionPolicy::Close));
        }
        if (received <= 0) {
            return std::unexpected(api::httpError(HTTP_CODE_BAD_REQUEST, "incomplete_body",
                                                  "Request body was interrupted", std::nullopt,
                                                  api::ConnectionPolicy::Close));
        }
        offset += static_cast<size_t>(received);
    }
    return body;
}

std::optional<std::string> CompanionApi::queryParameter(httpd_req_t& request, std::string_view name) const {
    if (const auto* buffered = companion::bufferedRequest(request)) {
        const auto value = buffered->query.find(name);
        return value == buffered->query.end() ? std::nullopt : std::optional{value->second};
    }
    const size_t length = httpd_req_get_url_query_len(&request);
    if (length == 0)
        return std::nullopt;

    std::string queryString(length + 1, '\0');
    if (httpd_req_get_url_query_str(&request, queryString.data(), queryString.size()) != ESP_OK)
        return std::nullopt;

    std::string value(length + 1, '\0');
    const std::string key{name};
    if (httpd_query_key_value(queryString.c_str(), key.c_str(), value.data(), value.size()) != ESP_OK)
        return std::nullopt;

    const size_t terminator = value.find('\0');
    if (terminator != std::string::npos)
        value.resize(terminator);
    return glz::url_decode(value);
}

api::Result<std::string> CompanionApi::requiredQueryParameter(httpd_req_t& request, std::string_view name,
                                                              std::string_view missingMessage) const {
    auto value = queryParameter(request, name);
    if (!value) {
        return std::unexpected(api::httpError(HTTP_CODE_BAD_REQUEST, "missing_field", std::string{missingMessage},
                                              std::string{name}));
    }
    return std::move(*value);
}

api::Result<std::string> CompanionApi::routeId(const httpd_req_t& request, std::string_view prefix,
                                               std::string_view suffix) const {
    const auto* buffered = companion::bufferedRequest(request);
    const std::string_view path = buffered == nullptr ? requestPath(request) : std::string_view{buffered->path};
    if (!path.starts_with(prefix) || !path.ends_with(suffix) || path.size() <= prefix.size() + suffix.size()) {
        return std::unexpected(api::httpError(HTTP_CODE_BAD_REQUEST, "missing_field", "Resource id is required", "id"));
    }

    const std::string_view encoded = path.substr(prefix.size(), path.size() - prefix.size() - suffix.size());
    std::string id = glz::url_decode(encoded);
    if (id.empty() || id.contains('/')) {
        return std::unexpected(api::httpError(HTTP_CODE_BAD_REQUEST, "invalid_field", "Resource id is invalid", "id"));
    }
    return id;
}
