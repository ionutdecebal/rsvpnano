#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>

#include "reader/ReadingSession.h"
#include "reader/ReadingState.h"
#include "library/IndexedBookStore.h"

class StorageManager;

namespace ReadingProgress {

    inline constexpr uint32_t kNoSavedWordIndex = UINT32_MAX;

    std::expected<uint32_t, std::error_code> readBookStatePosition(std::string_view bookPath,
                                                                   const reading::BookIdentity& identity);
    std::expected<reading::State, std::error_code> readBookState(std::string_view bookPath,
                                                                 const reading::BookIdentity& identity);
    std::expected<void, std::error_code> writeBookStatePosition(std::string_view bookPath,
                                                                const reading::BookIdentity& identity,
                                                                uint32_t wordIndex);
    std::expected<void, std::error_code> writeBookLanguageFonts(std::string_view bookPath,
                                                                const reading::BookIdentity& identity,
                                                                std::vector<settings::LanguageFont> languageFonts);
    std::expected<void, std::error_code> writeBookOverrides(std::string_view bookPath,
                                                             const reading::BookIdentity& identity,
                                                             const settings::ReadingOverrides& overrides);
    void save(ReadingSession& session, Preferences& preferences, bool force, uint32_t nowMs);
    void cache(ReadingSession& session, Preferences& preferences, uint32_t wordIndex);
    void mirror(const ReadingSession& session, const IndexedBookStore& store);
    uint32_t restore(ReadingSession& session, const IndexedBookStore& store);
    bool saveChapterTransition(ReadingSession& session, Preferences& preferences, const IndexedBookStore& store,
                               size_t previousWordIndex, size_t currentWordIndex, uint32_t nowMs);
    std::string_view title(const ReadingSession& session, const StorageManager& storage);
    uint8_t percent(uint32_t wordIndex, uint32_t wordCount);

} // namespace ReadingProgress
