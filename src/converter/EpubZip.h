#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "converter/EpubConverter.h"

namespace EpubZip {

    enum class ContentExtractStatus {
        Complete,
        WordLimitReached,
        Unsupported,
        Failed,
    };

    struct ZipEntry {
        String name;
        uint16_t method = 0;
        uint16_t flags = 0;
        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;
        uint32_t localHeaderOffset = 0;
    };

    class Archive {
    public:
        bool open(const String& path);
        void close();
        const ZipEntry* find(const String& name) const;

        bool extractToString(const String& name, String& output, size_t maxBytes);
        ContentExtractStatus extractContentToRsvp(const String& name, File& output, size_t& wordCount, size_t maxWords,
                                                  String& lastChapterTitle, const EpubConverter::Options& options,
                                                  size_t itemIndex, size_t itemCount);

    private:
        void logArchiveHints(const char* reason) const;
        bool readCentralDirectory();
        bool extractToString(const ZipEntry& entry, String& output, size_t maxBytes);
        ContentExtractStatus extractContentToRsvp(const ZipEntry& entry, File& output, size_t& wordCount,
                                                  size_t maxWords, String& lastChapterTitle,
                                                  const EpubConverter::Options& options, size_t itemIndex,
                                                  size_t itemCount);

        String archivePath_;
        File file_;
        std::vector<ZipEntry> entries_;
    };

} // namespace EpubZip
