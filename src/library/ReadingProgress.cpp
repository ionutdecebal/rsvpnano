#include "library/ReadingProgress.h"
#include <esp_log.h>
#include "logging/Logger.h"

#include <FS.h>
#include <glaze/toml.hpp>

#include <algorithm>

#include "board/BoardStorage.h"
#include "reader/ReadingLoop.h"
#include "settings/SettingsGlaze.h"
#include "library/StorageManager.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"

namespace ReadingProgress {
    using reading::BookIdentity;

    namespace {

        constexpr size_t kMaxBookStateBytes = 2048;
        constexpr uint32_t kSaveIntervalMs = 15000;

        bool isValidIdentity(const BookIdentity& identity) {
            return identity.sourceSize > 0 && identity.wordCount > 0;
        }

        bool matches(const reading::StoredState& state, const BookIdentity& identity) {
            return state.identity == identity;
        }

        std::expected<reading::StoredState, std::error_code> loadBookState(std::string_view bookPath,
                                                                           const BookIdentity& identity) {
            if (bookPath.empty() || !isValidIdentity(identity))
                return std::unexpected(std::make_error_code(std::errc::invalid_argument));

            const std::string statePath = StoragePaths::bookStatePathFor(bookPath);
            File file = Board::Storage::filesystem().open(statePath.c_str(), FILE_READ);
            if (!file || file.isDirectory()) {
                if (file)
                    file.close();
                return std::unexpected(std::make_error_code(file ? std::errc::invalid_argument
                                                                 : std::errc::no_such_file_or_directory));
            }
            const size_t size = file.size();
            if (size == 0 || size > kMaxBookStateBytes) {
                file.close();
                return std::unexpected(std::make_error_code(size > kMaxBookStateBytes ? std::errc::value_too_large
                                                                                      : std::errc::invalid_argument));
            }
            std::string input(size, '\0');
            const size_t read = file.read(reinterpret_cast<uint8_t*>(input.data()), input.size());
            file.close();
            if (read != input.size())
                return std::unexpected(std::make_error_code(std::errc::io_error));

            reading::StoredState candidate;
            if (const glz::error_ctx error =
                    glz::read<glz::opts{.format = glz::TOML, .error_on_unknown_keys = false}>(candidate, input)) {
                return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }
            if (!matches(candidate, identity))
                return std::unexpected(std::make_error_code(std::errc::state_not_recoverable));
            candidate.reading.wordIndex = std::min(candidate.reading.wordIndex, identity.wordCount - 1);
            return candidate;
        }

        bool canReplaceBookState(std::error_code error) {
            return error == std::errc::no_such_file_or_directory || error == std::errc::invalid_argument
                || error == std::errc::value_too_large || error == std::errc::state_not_recoverable;
        }

        std::expected<reading::StoredState, std::error_code> loadWritableBookState(std::string_view bookPath,
                                                                                   const BookIdentity& identity) {
            return loadBookState(bookPath, identity)
                .or_else([](std::error_code error) -> std::expected<reading::StoredState, std::error_code> {
                    if (canReplaceBookState(error))
                        return reading::StoredState{};
                    return std::unexpected(error);
                });
        }

        std::expected<void, std::error_code> writeBookState(std::string_view bookPath,
                                                            const reading::StoredState& state, bool atomic = false) {
            std::string output;
            if (glz::write_toml(state, output))
                return std::unexpected(std::make_error_code(std::errc::io_error));

            const std::string sidecarPath = StoragePaths::bookStatePathFor(bookPath);
            if (atomic) {
                const auto temporary = sidecarPath + ".tmp", backup = sidecarPath + ".bak";
                return StorageFiles::writeFileAtomic(Board::Storage::filesystem(), sidecarPath.c_str(),
                                                      temporary.c_str(), backup.c_str(), output);
            }
            File file = Board::Storage::filesystem().open(sidecarPath.c_str(), FILE_WRITE);
            if (!file || file.isDirectory()) {
                if (file)
                    file.close();
                return std::unexpected(std::make_error_code(std::errc::io_error));
            }
            const size_t written = file.write(reinterpret_cast<const uint8_t*>(output.data()), output.size());
            file.close();
            if (written != output.size()) {
                return std::unexpected(std::make_error_code(std::errc::io_error));
            }
            return {};
        }

        std::expected<void, std::error_code> writeSessionSidecar(const ReadingSession& session,
                                                                 const IndexedBookStore& store) {
            const uint32_t wordCount = store.identity().wordCount;
            if (!session.stored() || wordCount == 0)
                return std::unexpected(std::make_error_code(std::errc::invalid_argument));

            reading::StoredState state{
                .identity = store.identity(),
                .reading = session.state,
            };
            state.reading.wordIndex = std::min(state.reading.wordIndex, wordCount - 1);
            return writeBookState(session.sourcePath(), state);
        }

        std::expected<uint32_t, std::error_code> readSessionSidecar(ReadingSession& session,
                                                                    const IndexedBookStore& store) {
            if (!session.stored())
                return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            return loadBookState(session.sourcePath(), store.identity())
                .transform([&session](reading::StoredState state) {
                    session.state = std::move(state.reading);
                    return session.state.wordIndex;
                });
        }

    } // namespace

    std::expected<uint32_t, std::error_code> readBookStatePosition(std::string_view bookPath,
                                                                   const BookIdentity& identity) {
        return loadBookState(bookPath, identity).transform([](const reading::StoredState& state) {
            return state.reading.wordIndex;
        });
    }

    std::expected<reading::State, std::error_code> readBookState(std::string_view bookPath,
                                                                 const BookIdentity& identity) {
        return loadBookState(bookPath, identity).transform([](reading::StoredState state) {
            return std::move(state.reading);
        });
    }

    std::expected<void, std::error_code> writeBookStatePosition(std::string_view bookPath, const BookIdentity& identity,
                                                                uint32_t wordIndex) {
        if (bookPath.empty() || !isValidIdentity(identity))
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));

        return loadWritableBookState(bookPath, identity)
            .and_then([&](reading::StoredState state) {
                state.identity = identity;
                state.reading.wordIndex = std::min(wordIndex, identity.wordCount - 1);
                return writeBookState(bookPath, std::move(state));
            })
            .transform([&] {
                ESP_LOGI("storage-progress", "mirrored position word=%u count=%u state=%s",
                         static_cast<unsigned int>(wordIndex), static_cast<unsigned int>(identity.wordCount),
                         StoragePaths::bookStatePathFor(bookPath).c_str());
            });
    }

    std::expected<void, std::error_code> writeBookOverrides(std::string_view bookPath, const BookIdentity& identity,
                                                             const settings::ReadingOverrides& overrides) {
        if (bookPath.empty() || !isValidIdentity(identity))
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        return loadWritableBookState(bookPath, identity).and_then([&](reading::StoredState state) {
            state.identity = identity;
            state.reading.overrides = overrides;
            return writeBookState(bookPath, state, true);
        });
    }

    std::expected<void, std::error_code> writeBookLanguageFonts(std::string_view bookPath, const BookIdentity& identity,
                                                                std::vector<settings::LanguageFont> languageFonts) {
        if (bookPath.empty() || !isValidIdentity(identity))
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        return loadWritableBookState(bookPath, identity).and_then([&](reading::StoredState state) {
            state.identity = identity;
            state.reading.overrides.languageFonts = std::move(languageFonts);
            return writeBookState(bookPath, std::move(state));
        });
    }

    uint8_t percent(uint32_t wordIndex, uint32_t wordCount) {
        if (wordCount <= 1)
            return 0;
        return static_cast<uint8_t>(std::min<uint32_t>((wordIndex * 100UL) / (wordCount - 1), 100));
    }

    void save(ReadingSession& session, Preferences& preferences, bool force, uint32_t nowMs) {
        if (!session.stored() || (!force && nowMs - session.lastSaveMs < kSaveIntervalMs))
            return;
        const size_t wordIndex = session.state.wordIndex;
        if (!force && wordIndex == session.lastSavedWordIndex)
            return;
        session.lastSaveMs = nowMs;
        cache(session, preferences, static_cast<uint32_t>(wordIndex));
        ESP_LOGI("storage-progress", "saved position word=%u path=%s", static_cast<unsigned int>(wordIndex),
                 session.sourcePath().data());
    }

    void cache(ReadingSession& session, Preferences& preferences, uint32_t wordIndex) {
        if (!session.stored())
            return;
        const uint32_t wordCount = session.bookStore->identity().wordCount;
        if (wordCount > 0)
            wordIndex = std::min<uint32_t>(wordIndex, static_cast<uint32_t>(wordCount - 1));
        preferences.putString("book", session.sourcePath().data());
        session.lastSavedWordIndex = wordIndex;
    }

    void mirror(const ReadingSession& session, const IndexedBookStore& store) {
        if (!session.stored())
            return;
        auto written = writeSessionSidecar(session, store);
        if (!written)
            Logger::failure("storage-progress", "mirror", StoragePaths::bookStatePathFor(session.sourcePath()).c_str(),
                            written.error());
    }

    uint32_t restore(ReadingSession& session, const IndexedBookStore& store) {
        auto wordIndex = readSessionSidecar(session, store);
        if (wordIndex)
            return *wordIndex;
        if (wordIndex.error() != std::errc::no_such_file_or_directory
            && wordIndex.error() != std::errc::state_not_recoverable)
            Logger::failure("storage-progress", "restore", StoragePaths::bookStatePathFor(session.sourcePath()).c_str(),
                            wordIndex.error());
        return kNoSavedWordIndex;
    }

    bool saveChapterTransition(ReadingSession& session, Preferences& preferences, const IndexedBookStore& store,
                               size_t previousWordIndex, size_t currentWordIndex, uint32_t nowMs) {
        if (!session.stored())
            return false;
        const auto chapter = std::ranges::find_if(session.metadata.chapters, [&](const ChapterMarker& candidate) {
            return candidate.wordIndex != 0 && candidate.wordIndex > previousWordIndex
                && candidate.wordIndex <= currentWordIndex;
        });
        if (chapter == session.metadata.chapters.end())
            return false;
        ReadingLoop::seekTo(session, chapter->wordIndex);
        save(session, preferences, true, nowMs);
        mirror(session, store);
        return true;
    }

    std::string_view title(const ReadingSession& session, const StorageManager& storage) {
        if (!session.stored())
            return "Demo";
        const int index = storage.findBook(session.sourcePath());
        const BookLibrary::Entry* book = index < 0 ? nullptr : storage.book(static_cast<size_t>(index));
        return book == nullptr ? std::string_view{} : BookLibrary::displayName(*book);
    }

} // namespace ReadingProgress
