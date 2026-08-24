#include "companion/http/CompanionApi.h"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "board/BoardStorage.h"
#include "companion/serial/CompanionBufferedRequest.h"
#include "companion/http/CompanionUpload.h"
#include "logging/Logger.h"
#include "reader/ReadingLoop.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "storage/index/IndexedBook.h"
#include "storage/index/ReadingProgress.h"
#include "text/AsciiText.h"

namespace {

    namespace api = companion::api;
    constexpr size_t kMaxBookUploadBytes = 256UL * 1024UL * 1024UL;

    [[nodiscard]] api::HttpError bookInstallError(std::error_code error) {
        if (error == std::errc::file_exists) {
            return api::httpError(HTTP_CODE_CONFLICT, "already_exists", "A library item with that name already exists",
                                  "name");
        }
        return api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "storage_error", "Book could not be installed");
    }

    std::expected<void, std::error_code> appendBookUpload(void* context, std::span<const uint8_t> bytes) {
        return static_cast<IndexedBook::Builder*>(context)->append(bytes);
    }

} // namespace

esp_err_t CompanionApi::handleLibrary(httpd_req_t* request) {
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
    return self->sendLibrary(*request);
}

esp_err_t CompanionApi::handleLibraryInstall(httpd_req_t* request) {
    if (request == nullptr || request->handle == nullptr)
        return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<CompanionApi*>(httpd_get_global_user_ctx(request->handle));
    if (self == nullptr || !self->active())
        return ESP_ERR_INVALID_STATE;
    if (!self->browserOriginAllowed(*request))
        return self->sendError(*request, api::httpError(HTTP_CODE_FORBIDDEN, "origin_forbidden", "This browser origin is not allowed"));

    const std::lock_guard operationLock{self->operationsMutex_};
    auto response = self->installLibraryItem(*request);
    if (!response)
        return self->sendError(*request, std::move(response.error()));
    return self->sendJson(*request, HTTP_CODE_CREATED, *response);
}

esp_err_t CompanionApi::sendLibrary(httpd_req_t& request) {
    if (const esp_err_t error = setBrowserResponseHeaders(request); error != ESP_OK)
        return error;
    if (const esp_err_t error = httpd_resp_set_hdr(&request, "Cache-Control", "no-store"); error != ESP_OK)
        return error;
    const std::string status = api::httpStatusLine(HTTP_CODE_OK);
    if (const esp_err_t error = httpd_resp_set_status(&request, status.c_str()); error != ESP_OK)
        return error;
    if (const esp_err_t error = httpd_resp_set_type(&request, "application/json"); error != ESP_OK)
        return error;
    if (const esp_err_t error = httpd_resp_send_chunk(&request, "[", 1); error != ESP_OK)
        return error;

    size_t sent = 0;
    for (size_t index = 0; index < storage_.books().size(); ++index) {
        auto json = encodeBook(index);
        if (!json) {
            ESP_LOGE("companion", "library item %u encode failed", static_cast<unsigned>(index));
            return ESP_FAIL;
        }
        if (sent != 0) {
            if (const esp_err_t error = httpd_resp_send_chunk(&request, ",", 1); error != ESP_OK)
                return error;
        }
        if (const esp_err_t error = httpd_resp_send_chunk(&request, json->data(), static_cast<ssize_t>(json->size()));
            error != ESP_OK) {
            return error;
        }
        ++sent;
    }
    if (const esp_err_t error = httpd_resp_send_chunk(&request, "]", 1); error != ESP_OK)
        return error;
    ESP_LOGI("companion", "library response items=%u", static_cast<unsigned>(sent));
    return httpd_resp_send_chunk(&request, nullptr, 0);
}

companion::api::Result<std::string> CompanionApi::encodeBook(size_t index, const BookMetadata* availableMetadata,
                                                             const reading::BookIdentity* availableIdentity) {
    const BookLibrary::Entry* entry = storage_.book(index);
    if (entry == nullptr || entry->bytes > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "storage_error",
                                              "Book could not be read"));
    }

    BookMetadata storedMetadata;
    const BookMetadata* metadata = availableMetadata == nullptr ? &storedMetadata : availableMetadata;
    const reading::BookIdentity* identity = availableIdentity;
    std::optional<reading::BookIdentity> storedIdentity;
    std::optional<reading::State> storedReading;
    const reading::State* reading = nullptr;
    const ReadingSession& session = readerScreen_.session;
    if (session.sourcePath() == entry->path) {
        metadata = &session.metadata;
        reading = &session.state;
        identity = &readerScreen_.store.identity();
    } else if (identity == nullptr) {
        storedIdentity = storage_.readBookMetadata(index, storedMetadata);
        identity = storedIdentity ? &*storedIdentity : nullptr;
    }
    if (reading == nullptr && identity != nullptr) {
        if (auto state = ReadingProgress::readBookState(entry->path, *identity)) {
            storedReading = std::move(*state);
            reading = &*storedReading;
        }
    }

    const std::string id = BookLibrary::id(*entry);
    const std::string_view name = BookLibrary::relativeName(*entry);
    auto scripts = UnicodeText::scriptTags(metadata->scriptMask);
    auto languages = api::bookLanguages(*metadata);
    const auto bookMetadata = glz::obj{"title",       entry->title,      "author",
                                       entry->author, "wordCount",       identity == nullptr ? 0U : identity->wordCount,
                                       "locale",      metadata->locale,  "scripts",
                                       scripts,       "languages",       languages,
                                       "chapters",    metadata->chapters};
    if (reading != nullptr) {
        const auto response =
            glz::obj{"id",
                     id,
                     "name",
                     name,
                     "bytes",
                     entry->bytes,
                     "metadata",
                     bookMetadata,
                     "reading",
                     glz::obj{"wordIndex", reading->wordIndex, "languageFonts", reading->overrides.languageFonts}};
        return encodeResponse(response);
    }
    const auto response =
        glz::obj{"id", id, "name", name, "bytes", entry->bytes, "metadata", bookMetadata, "reading", nullptr};
    return encodeResponse(response);
}

companion::api::Result<std::string> CompanionApi::installLibraryItem(httpd_req_t& request) {
    if (request.content_len == 0) {
        return std::unexpected(companion::api::httpError(HTTP_CODE_BAD_REQUEST, "missing_upload",
                                                         "Book file is required", "file"));
    }
    if (request.content_len > kMaxBookUploadBytes) {
        return std::unexpected(companion::api::httpError(HTTP_CODE_PAYLOAD_TOO_LARGE, "payload_too_large",
                                                         "Book is too large", "file",
                                                         companion::api::ConnectionPolicy::Close));
    }

    auto requestedName = requiredQueryParameter(request, "name", "Book filename is required");
    if (!requestedName)
        return std::unexpected(companion::api::closeConnection(std::move(requestedName.error())));

    std::string filename = StoragePaths::sanitizeFilename(*requestedName);
    if (filename.empty()) {
        return std::unexpected(companion::api::httpError(HTTP_CODE_BAD_REQUEST, "invalid_field",
                                                         "Book filename is invalid", "name",
                                                         companion::api::ConnectionPolicy::Close));
    }
    if (StoragePaths::hasEpubExtension(filename)) {
        return std::unexpected(companion::api::httpError(
            HTTP_CODE_BAD_REQUEST, "invalid_field", "Convert EPUB books to RSVP before uploading", "name",
            companion::api::ConnectionPolicy::Close));
    }
    if (!StoragePaths::hasRsvpExtension(filename) && !StoragePaths::hasTextExtension(filename)) {
        filename += ".rsvp";
    }

    std::string category = queryParameter(request, "category").value_or("");
    std::ranges::transform(category, category.begin(), AsciiText::toLower);
    const char* targetDirectory =
        category == "article" ? StoragePaths::kArticleFilesPath : StoragePaths::kBookFilesPath;

    for (const char* directory:
         {StoragePaths::kLibraryPath, StoragePaths::kBookFilesPath, StoragePaths::kArticleFilesPath}) {
        if (auto created = StorageFiles::ensureDirectory(directory); !created) {
            return std::unexpected(companion::api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "storage_error",
                                                             "Library folder unavailable: " + created.error().message(),
                                                             std::nullopt, companion::api::ConnectionPolicy::Close));
        }
    }

    const std::string finalPath = std::string{targetDirectory} + "/" + filename;
    IndexedBook::Builder builder{Board::Storage::filesystem(), finalPath, static_cast<uint32_t>(request.content_len),
                                 StoragePaths::hasRsvpExtension(filename)};
    if (auto begun = builder.begin(); !begun) {
        Logger::failure("companion", "begin book index", finalPath.c_str(), begun.error());
        return std::unexpected(companion::api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "index_error",
                                                         builder.failure(), "file",
                                                         companion::api::ConnectionPolicy::Close));
    }
    auto upload = companion::TemporaryUpload::receive(request, Board::Storage::filesystem(), finalPath + ".tmp",
                                                      kMaxBookUploadBytes, "Book", appendBookUpload, &builder);
    if (!upload)
        return std::unexpected(std::move(upload.error()));
    if (auto finished = builder.finish(); !finished) {
        Logger::failure("companion", "finish book index", finalPath.c_str(), finished.error());
        return std::unexpected(companion::api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "index_error",
                                                         builder.failure(), "file",
                                                         companion::api::ConnectionPolicy::Close));
    }

    auto installed = storage_.installBook(upload->path(), finalPath);
    if (!installed) {
        Logger::failure("companion", "install book", upload->path().c_str(), finalPath.c_str(), installed.error());
        return std::unexpected(bookInstallError(installed.error()));
    }
    if (auto committed = builder.commit(); !committed) {
        Logger::failure("companion", "commit book index", finalPath.c_str(), committed.error());
        if (auto rollback = storage_.removeBook(finalPath); !rollback)
            Logger::failure("companion", "rollback book install", finalPath.c_str(), rollback.error());
        return std::unexpected(companion::api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "index_error",
                                                         builder.failure(), "file"));
    }

    libraryScreen_.invalidate();
    const int index = storage_.findBook(finalPath);
    if (index < 0) {
        if (auto rollback = storage_.removeBook(finalPath); !rollback) {
            Logger::failure("companion", "rollback book install", finalPath.c_str(), rollback.error());
        }
        libraryScreen_.invalidate();
        return std::unexpected(companion::api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "storage_error",
                                                         "Book is missing from the library"));
    }
    BookMetadata metadata = builder.takeMetadata();
    auto response = encodeBook(static_cast<size_t>(index), &metadata, &builder.header().identity);
    const BookLibrary::Entry* book = storage_.book(static_cast<size_t>(index));
    if (response && book != nullptr) {
        const std::string location = "/api/v2/library/" + BookLibrary::id(*book);
        if (auto* buffered = companion::bufferedRequest(request)) {
            buffered->location = location;
            return response;
        }
        if (httpd_resp_set_hdr(&request, "Location", location.c_str()) == ESP_OK)
            return response;
    }

    if (auto rollback = storage_.removeBook(finalPath); !rollback)
        Logger::failure("companion", "rollback book response", finalPath.c_str(), rollback.error());
    libraryScreen_.invalidate();
    if (!response)
        return response;
    return std::unexpected(companion::api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "storage_error",
                                                     "Book response could not be prepared"));
}

companion::api::Result<> CompanionApi::deleteLibraryItem(httpd_req_t& request) {
    auto id = routeId(request, "/api/v2/library/");
    if (!id)
        return std::unexpected(std::move(id.error()));

    const auto index = findBookIndex(*id);
    if (!index) {
        return std::unexpected(companion::api::httpError(HTTP_CODE_NOT_FOUND, "book_not_found", "Book not found",
                                                         "id"));
    }
    std::string path = storage_.books()[*index].path;
    if (readerScreen_.session.sourcePath() == path) {
        return std::unexpected(companion::api::httpError(HTTP_CODE_CONFLICT, "resource_in_use",
                                                         "Close the active book before removing it", "id"));
    }

    std::string bookPath = std::move(path);
    return storage_.removeBook(bookPath)
        .transform_error([&bookPath](std::error_code error) {
            Logger::failure("companion", "delete book", bookPath.c_str(), error);
            return companion::api::httpError(HTTP_CODE_INTERNAL_SERVER_ERROR, "storage_error",
                                             "Book could not be deleted");
        })
        .transform([this, bookPath = std::move(bookPath)] {
            libraryScreen_.invalidate();
            ESP_LOGD("companion", "deleted %s", bookPath.c_str());
        });
}

std::optional<size_t> CompanionApi::findBookIndex(std::string_view id) const {
    for (size_t index = 0; index < storage_.books().size(); ++index) {
        if (BookLibrary::id(storage_.books()[index]) == id)
            return index;
    }
    return std::nullopt;
}
