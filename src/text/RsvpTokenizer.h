#pragma once

#include <Arduino.h>
#include <concepts>
#include <string>
#include <string_view>
#include <utility>

#ifndef RSVP_MAX_BOOK_WORDS
#define RSVP_MAX_BOOK_WORDS 0
#endif

#include "text/TextNormalizer.h"
#include "text/UnicodeText.h"
#include "text/Utf8Text.h"

namespace RsvpText {

    constexpr size_t kMaxBookWords = static_cast<size_t>(RSVP_MAX_BOOK_WORDS);
    constexpr size_t kMaxBookLineChars = 4096;
    constexpr uint8_t kCjkPhraseCharacters = 2;

    struct ParseStats {
        NormalizationStats normalization;
        size_t longLineSplits = 0;
        bool memoryLow = false;
    };

    bool hasReadableText(std::string_view text);

    namespace Detail {

        bool isWordBoundary(char c);
        bool isInlineWordHyphen(std::string_view text, size_t index);
        bool endsCjkPhrase(uint32_t codepoint);

    } // namespace Detail

    template<typename TokenConsumer>
        requires std::predicate<TokenConsumer&, const std::string&>
    bool appendNormalizedLineWords(std::string_view normalizedLine, TokenConsumer consumeToken, const size_t& wordCount,
                                   size_t maxWords) {
        std::string currentWord;
        std::string pendingToken;
        currentWord.reserve(32);
        pendingToken.reserve(32);

        auto withinWordLimit = [&]() {
            return maxWords == 0 || wordCount < maxWords;
        };

        auto flushPending = [&]() -> bool {
            if (pendingToken.empty()) {
                return true;
            }
            if (!withinWordLimit()) {
                return false;
            }
            if (!consumeToken(pendingToken)) {
                return false;
            }
            pendingToken.clear();
            return withinWordLimit();
        };

        auto queueToken = [&](std::string_view token) -> bool {
            if (token.empty()) {
                return true;
            }

            if (token == "...") {
                if (!pendingToken.empty()) {
                    pendingToken += "...";
                }
                return true;
            }

            if (token == "-" || token == "\u2014") {
                if (!flushPending()) {
                    return false;
                }
                if (!consumeToken(std::string{token})) {
                    return false;
                }
                return withinWordLimit();
            }

            if (!flushPending()) {
                return false;
            }
            pendingToken.assign(token);
            return true;
        };

        auto finishToken = [&](std::string token) -> bool {
            if (!UnicodeText::isCjkText(token))
                return queueToken(token);

            size_t phraseStart = 0;
            size_t offset = 0;
            uint8_t characters = 0;
            bool breakBeforeCharacter = false;
            std::string_view remaining{token};
            while (!remaining.empty()) {
                const size_t codepointStart = offset;
                const size_t bytesBefore = remaining.size();
                uint32_t codepoint = 0;
                Utf8Text::next(remaining, codepoint);
                offset += bytesBefore - remaining.size();

                if (UnicodeText::isWordCharacter(codepoint)) {
                    if ((characters >= kCjkPhraseCharacters || breakBeforeCharacter)
                        && !queueToken(std::string_view{token}.substr(phraseStart, codepointStart - phraseStart)))
                        return false;
                    if (characters >= kCjkPhraseCharacters || breakBeforeCharacter) {
                        phraseStart = codepointStart;
                        characters = 0;
                        breakBeforeCharacter = false;
                    }
                    ++characters;
                }
                if (Detail::endsCjkPhrase(codepoint))
                    breakBeforeCharacter = true;
            }
            return queueToken(std::string_view{token}.substr(phraseStart));
        };

        auto flushCurrent = [&]() -> bool {
            if (currentWord.empty()) {
                return true;
            }
            const bool ok = finishToken(std::move(currentWord));
            currentWord.clear();
            return ok;
        };

        for (size_t i = 0; i < normalizedLine.length(); ++i) {
            if ((i & 0x7F) == 0) {
                yield();
            }

            const char c = normalizedLine[i];
            if (Detail::isWordBoundary(c)) {
                if (!flushCurrent()) {
                    return false;
                }
                continue;
            }

            if (normalizedLine.substr(i).starts_with("\u2014")) {
                if (!flushCurrent() || !finishToken("\u2014")) {
                    return false;
                }
                i += 2; // The em dash occupies three UTF-8 bytes.
                continue;
            }

            if (c == '-') {
                if (Detail::isInlineWordHyphen(normalizedLine, i)) {
                    currentWord += c;
                    continue;
                }
                if (!flushCurrent() || !finishToken("-")) {
                    return false;
                }
                while (i + 1 < normalizedLine.length() && normalizedLine[i + 1] == '-') {
                    ++i;
                }
                continue;
            }

            if (c == '.' && i + 2 < normalizedLine.length() && normalizedLine[i + 1] == '.'
                && normalizedLine[i + 2] == '.') {
                currentWord += "...";
                i += 2;
                while (i + 1 < normalizedLine.length() && normalizedLine[i + 1] == '.') {
                    ++i;
                }
                if (!flushCurrent()) {
                    return false;
                }
                continue;
            }

            currentWord += c;
        }

        if (!flushCurrent()) {
            return false;
        }

        return flushPending();
    }

    template<typename TokenConsumer>
        requires std::predicate<TokenConsumer&, const std::string&>
    bool appendLineWords(std::string_view line, TokenConsumer consumeToken, const size_t& wordCount,
                         ParseStats* stats) {
        const std::string normalizedLine =
            normalizeDisplayText(line, stats == nullptr ? nullptr : &stats->normalization);
        return appendNormalizedLineWords(normalizedLine, consumeToken, wordCount, kMaxBookWords);
    }

} // namespace RsvpText
