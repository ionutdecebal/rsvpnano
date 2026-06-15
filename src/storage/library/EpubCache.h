#pragma once

#include <Arduino.h>

namespace EpubCache {

    using StatusCallback = void (*)(void* context, const char* title, const char* line1, const char* line2,
                                    int progressPercent);

    bool rsvpIsCurrent(const String& rsvpPath);
    bool hasCurrentCache(const String& epubPath);
    String libraryLabel(const String& epubPath);
    bool ensureConverted(const String& epubPath, String& rsvpPath, StatusCallback statusCallback, void* statusContext);

} // namespace EpubCache
