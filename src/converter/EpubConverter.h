#pragma once

#include <Arduino.h>

#ifndef RSVP_MAX_BOOK_WORDS
#define RSVP_MAX_BOOK_WORDS 0
#endif

class EpubConverter {
public:
    struct Options;

    using ProgressCallback = void (*)(const Options& options, const char* line1, const char* line2,
                                      int progressPercent);

    struct Options {
        Options() :
                maxWords(static_cast<size_t>(RSVP_MAX_BOOK_WORDS)),
                maxExtractBytes(256UL * 1024UL),
                maxContentBytes(8UL * 1024UL * 1024UL),
                progressCallback(nullptr) {}

        size_t maxWords;
        size_t maxExtractBytes;
        size_t maxContentBytes;
        ProgressCallback progressCallback;
        String progressTitle;
        String progressLabel;
    };

    static bool convertIfNeeded(const String& epubPath, const String& rsvpPath, const Options& options = Options());
    static bool isCurrentCache(const String& rsvpPath);
};
