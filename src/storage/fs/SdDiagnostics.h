#pragma once

#include <Arduino.h>

namespace SdDiagnostics {

    using StatusCallback =
        void (*)(void* context, const char* title, const char* line1, const char* line2, int progressPercent);

    struct DiagnosticResult {
        bool mounted = false;
        bool booksDirectory = false;
        bool bookFilesDirectory = false;
        bool articleFilesDirectory = false;
        bool configDirectory = false;
        bool writable = false;
        bool booksWritable = false;
        bool articlesWritable = false;
        bool configWritable = false;
        bool foldersRepaired = false;
        size_t bookCount = 0;
        size_t unsupportedCount = 0;
        uint64_t sizeMb = 0;
        int frequencyKhz = 0;
        String cardType;
        String summary;
        String detail;
    };

    DiagnosticResult diagnoseCard(bool& mounted, StatusCallback statusCallback, void* statusContext);
    bool mountCard(bool& mounted, int* mountedFrequencyKhz = nullptr);
    void probeWritableFolders(DiagnosticResult& result, StatusCallback statusCallback, void* statusContext);
    bool repairFolderLayout(bool mounted);

} // namespace SdDiagnostics
