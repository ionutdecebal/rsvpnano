#include "converter/EpubConverter.h"

#include "board/BoardStorage.h"
#include <algorithm>

#include "converter/EpubPackage.h"
#include "converter/EpubZip.h"
#include "storage/fs/StoragePaths.h"

namespace {

    constexpr size_t kMaxOpfBytes = 256UL * 1024UL;
    constexpr size_t kMaxContainerBytes = 32UL * 1024UL;
    constexpr const char* kConverterVersion = "stream-v6";

    using EpubPackage::basenameWithoutExtension;
    using EpubPackage::directoryForPath;
    using EpubPackage::findManifestItem;
    using EpubPackage::isContentDocument;
    using EpubPackage::ManifestItem;
    using EpubPackage::parseDcMetadata;
    using EpubPackage::parseManifestItems;
    using EpubPackage::parseRootfilePath;
    using EpubPackage::parseSpineIds;

    struct PackageDocuments {
        String opfXml;
        String opfPath;
        String opfBaseDir;
    };

    struct ConversionPaths {
        String temp;
        String failed;
        String lock;
    };

    void serviceBackground() {
        yield();
        delay(0);
    }

    void reportProgress(const EpubConverter::Options& options, const char* line1, const char* line2,
                        int progressPercent) {
        if (options.progressCallback == nullptr) {
            return;
        }

        progressPercent = std::max(0, std::min(100, progressPercent));
        options.progressCallback(options, line1, line2, progressPercent);
        serviceBackground();
    }

    String wordCountDetail(size_t wordCount) {
        return String(wordCount) + " words";
    }

    String itemProgressDetail(size_t itemIndex, size_t itemCount, size_t wordCount) {
        return String(itemIndex + 1) + "/" + String(itemCount) + " " + String(wordCount) + " words";
    }

    int contentProgressPercent(size_t completedItems, size_t itemCount) {
        return 25 + static_cast<int>((completedItems * 70UL) / itemCount);
    }

    bool readPackageDocuments(EpubZip::Archive& zip, const EpubConverter::Options& options,
                              PackageDocuments& documents) {
        String containerXml;

        reportProgress(options, "Opening EPUB", "Reading metadata", 8);
        Serial.println("[epub] Reading META-INF/container.xml");
        Serial.flush();
        if (!zip.extractToString("META-INF/container.xml", containerXml, kMaxContainerBytes)) {
            Serial.println("[epub] EPUB container.xml not found or unreadable");
            return false;
        }
        Serial.printf("[epub] container.xml loaded: %u chars\n", static_cast<unsigned int>(containerXml.length()));

        documents.opfPath = parseRootfilePath(containerXml);
        if (documents.opfPath.isEmpty()) {
            Serial.println("[epub] EPUB rootfile path not found");
            return false;
        }
        Serial.printf("[epub] Rootfile OPF path: %s\n", documents.opfPath.c_str());

        reportProgress(options, "Opening EPUB", "Reading package", 14);
        Serial.printf("[epub] Reading OPF package: %s\n", documents.opfPath.c_str());
        if (!zip.extractToString(documents.opfPath, documents.opfXml, kMaxOpfBytes)) {
            Serial.printf("[epub] OPF file not readable: %s\n", documents.opfPath.c_str());
            return false;
        }

        Serial.printf("[epub] OPF loaded: %u chars\n", static_cast<unsigned int>(documents.opfXml.length()));
        documents.opfBaseDir = directoryForPath(documents.opfPath);
        return true;
    }

    std::vector<String> contentDocumentsInSpineOrder(const std::vector<ManifestItem>& manifest,
                                                     const std::vector<String>& spineIds) {
        std::vector<String> order;
        order.reserve(spineIds.size());

        std::for_each(spineIds.begin(), spineIds.end(), [&](const String& spineId) {
            serviceBackground();
            const ManifestItem* item = findManifestItem(manifest, spineId);
            if (item != nullptr && isContentDocument(*item)) {
                order.push_back(item->path);
            }
        });

        return order;
    }

    std::vector<String> allContentDocuments(const std::vector<ManifestItem>& manifest) {
        std::vector<String> order;
        order.reserve(manifest.size());
        std::for_each(manifest.begin(), manifest.end(), [&](const ManifestItem& item) {
            if (isContentDocument(item)) {
                order.push_back(item.path);
            }
        });
        return order;
    }

    std::vector<String> buildReadingOrder(const String& opfXml, const String& opfBaseDir,
                                          const EpubConverter::Options& options) {
        const std::vector<ManifestItem> manifest = parseManifestItems(opfXml, opfBaseDir);
        const std::vector<String> spineIds = parseSpineIds(opfXml);

        Serial.printf("[epub] Package parsed: manifest=%u spine=%u base=%s\n",
                      static_cast<unsigned int>(manifest.size()), static_cast<unsigned int>(spineIds.size()),
                      opfBaseDir.c_str());

        reportProgress(options, "Opening EPUB", "Building reading order", 20);
        return [&]() {
            std::vector<String> order = contentDocumentsInSpineOrder(manifest, spineIds);
            return order.empty() ? allContentDocuments(manifest) : order;
        }();
    }

    void writeRsvpHeader(File& output, const String& epubPath, const String& opfXml) {
        const String title = [&]() {
            const String metadataTitle = parseDcMetadata(opfXml, "title");
            return metadataTitle.isEmpty() ? basenameWithoutExtension(epubPath) : metadataTitle;
        }();
        const String author = parseDcMetadata(opfXml, "creator");

        output.println("@rsvp 1");
        output.print("@converter ");
        output.println(kConverterVersion);
        output.print("@title ");
        output.println(title);
        if (!author.isEmpty()) {
            output.print("@author ");
            output.println(author);
        }
        output.print("@source ");
        output.println(epubPath);
        output.println();
    }

    void reportReadingOrderReady(const EpubConverter::Options& options, const std::vector<String>& readingOrder) {
        Serial.printf("[epub] Reading order contains %u content files\n",
                      static_cast<unsigned int>(readingOrder.size()));
        const String foundDetail = String(readingOrder.size()) + " content files";
        reportProgress(options, "Opening EPUB", foundDetail.c_str(), 25);
    }

    void streamReadingOrder(EpubZip::Archive& zip, File& output, const std::vector<String>& readingOrder,
                            const EpubConverter::Options& options, size_t& wordCount) {
        String lastChapterTitle;

        const auto withinWordLimit = [&]() {
            return options.maxWords == 0 || wordCount < options.maxWords;
        };

        const auto reportItemProgress = [&](const char* title, size_t itemIndex) {
            const String detail = itemProgressDetail(itemIndex, readingOrder.size(), wordCount);
            reportProgress(options, title, detail.c_str(), contentProgressPercent(itemIndex, readingOrder.size()));
        };

        for (size_t i = 0; i < readingOrder.size() && withinWordLimit(); ++i) {
            serviceBackground();

            reportItemProgress("Extracting content", i);

            const EpubZip::ContentExtractStatus extractStatus =
                zip.extractContentToRsvp(readingOrder[i], output, wordCount, options.maxWords, lastChapterTitle,
                                         options, i, readingOrder.size());

            reportItemProgress("Parsed content", i + 1);

            if (extractStatus == EpubZip::ContentExtractStatus::Unsupported
                || extractStatus == EpubZip::ContentExtractStatus::Failed) {
                Serial.printf("[epub] Skipping unreadable content file: %s\n", readingOrder[i].c_str());
                continue;
            }

            if (extractStatus == EpubZip::ContentExtractStatus::WordLimitReached) {
                break;
            }
        }
    }

    bool promoteTempFile(const String& tempPath, const String& rsvpPath) {
        Board::Storage::filesystem().remove(rsvpPath);
        if (Board::Storage::filesystem().rename(tempPath, rsvpPath)) {
            return true;
        }

        Serial.printf("[epub] Could not rename %s to %s\n", tempPath.c_str(), rsvpPath.c_str());
        Board::Storage::filesystem().remove(tempPath);
        return false;
    }

    bool convertEpubToRsvp(const String& epubPath, const String& tempPath, const String& rsvpPath,
                           const EpubConverter::Options& options) {
        reportProgress(options, "Opening EPUB", "Reading archive", 0);

        EpubZip::Archive zip;
        if (!zip.open(epubPath)) {
            Serial.printf("[epub] Could not open EPUB archive: %s\n", epubPath.c_str());
            return false;
        }

        const auto failWithClosedZip = [&]() {
            zip.close();
            return false;
        };

        PackageDocuments documents;
        if (!readPackageDocuments(zip, options, documents)) {
            return failWithClosedZip();
        }

        const std::vector<String> readingOrder = buildReadingOrder(documents.opfXml, documents.opfBaseDir, options);
        if (readingOrder.empty()) {
            Serial.println("[epub] No readable XHTML spine items found");
            return failWithClosedZip();
        }
        reportReadingOrderReady(options, readingOrder);

        Board::Storage::filesystem().remove(tempPath);
        File output = Board::Storage::filesystem().open(tempPath, FILE_WRITE);
        if (!output) {
            Serial.printf("[epub] Could not create temporary RSVP file: %s\n", tempPath.c_str());
            return failWithClosedZip();
        }

        writeRsvpHeader(output, epubPath, documents.opfXml);

        size_t wordCount = 0;
        streamReadingOrder(zip, output, readingOrder, options, wordCount);

        const String finishingDetail = wordCountDetail(wordCount);
        reportProgress(options, "Finishing EPUB", finishingDetail.c_str(), 96);
        output.close();
        zip.close();

        if (wordCount == 0) {
            Serial.printf("[epub] No readable words extracted from %s\n", epubPath.c_str());
            Board::Storage::filesystem().remove(tempPath);
            return false;
        }

        if (!promoteTempFile(tempPath, rsvpPath)) {
            return false;
        }

        Serial.printf("[epub] Converted %s -> %s (%u words)\n", epubPath.c_str(), rsvpPath.c_str(),
                      static_cast<unsigned int>(wordCount));
        const String convertedDetail = wordCountDetail(wordCount);
        reportProgress(options, "EPUB converted", convertedDetail.c_str(), 100);
        return true;
    }

    void writeFailureMarker(const String& markerPath, const char* message) {
        Board::Storage::filesystem().remove(markerPath);

        File marker = Board::Storage::filesystem().open(markerPath, FILE_WRITE);
        if (!marker) {
            Serial.printf("[epub] Could not create failure marker: %s\n", markerPath.c_str());
            return;
        }

        marker.println(message == nullptr ? "Conversion failed" : message);
        marker.print("converter=");
        marker.println(kConverterVersion);
        marker.close();
    }

    bool markerWasWrittenByCurrentConverter(File& marker) {
        String content;
        content.reserve(256);
        while (marker.available() && content.length() < 256) {
            content += static_cast<char>(marker.read());
        }

        const String expected = String("converter=") + kConverterVersion;
        return content.indexOf(expected) >= 0;
    }

    bool rsvpWasWrittenByCurrentConverter(File& file) {
        if (!file || file.isDirectory()) {
            return false;
        }

        file.seek(0);
        String line;
        line.reserve(128);
        size_t scannedLines = 0;
        while (file.available() && scannedLines < 12) {
            const char c = static_cast<char>(file.read());
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                line.trim();
                if (line.startsWith("@converter")) {
                    const String expected = String("@converter ") + kConverterVersion;
                    return line == expected;
                }
                if (!line.isEmpty() && !line.startsWith("@")) {
                    break;
                }
                line = "";
                ++scannedLines;
                continue;
            }
            if (line.length() < 128) {
                line += c;
            }
        }

        line.trim();
        if (line.startsWith("@converter")) {
            const String expected = String("@converter ") + kConverterVersion;
            return line == expected;
        }

        return false;
    }

    ConversionPaths conversionPathsFor(const String& rsvpPath) {
        return {
            rsvpPath + StoragePaths::kTempExtension,
            rsvpPath + StoragePaths::kFailedExtension,
            rsvpPath + StoragePaths::kConvertingExtension,
        };
    }

    bool removeStaleCacheOrReuseCurrent(const String& rsvpPath) {
        File existing = Board::Storage::filesystem().open(rsvpPath);
        if (!existing) {
            return false;
        }

        const bool hasCache = !existing.isDirectory() && existing.size() > 0;
        const bool currentCache = hasCache && rsvpWasWrittenByCurrentConverter(existing);
        existing.close();

        if (!hasCache) {
            return false;
        }
        if (currentCache) {
            return true;
        }

        Serial.printf("[epub] Rebuilding stale RSVP cache after converter update: %s\n", rsvpPath.c_str());
        Board::Storage::filesystem().remove(rsvpPath);
        return false;
    }

    bool previousCurrentAttemptRestarted(const String& epubPath, const ConversionPaths& paths,
                                         const EpubConverter::Options& options) {
        File lock = Board::Storage::filesystem().open(paths.lock);
        if (!lock) {
            return false;
        }

        const bool lockMarker = !lock.isDirectory();
        const bool currentLock = lockMarker && markerWasWrittenByCurrentConverter(lock);
        lock.close();

        if (!lockMarker) {
            return false;
        }

        Board::Storage::filesystem().remove(paths.lock);
        Board::Storage::filesystem().remove(paths.temp);
        if (!currentLock) {
            Serial.printf("[epub] Retrying interrupted EPUB after converter update: %s\n", epubPath.c_str());
            return false;
        }

        Serial.printf("[epub] Previous conversion restart detected, skipping: %s\n", epubPath.c_str());
        writeFailureMarker(paths.failed, "Previous conversion restarted before completion.");
        reportProgress(options, "Previous restart", "Skipping this EPUB", 100);
        return true;
    }

    void removeOrphanedTempFile(const String& epubPath, const String& tempPath) {
        File temp = Board::Storage::filesystem().open(tempPath);
        if (!temp) {
            return;
        }

        const bool interruptedTemp = !temp.isDirectory();
        temp.close();
        if (!interruptedTemp) {
            return;
        }

        Serial.printf("[epub] Removing stale temporary conversion file and retrying: %s\n", epubPath.c_str());
        Board::Storage::filesystem().remove(tempPath);
    }

    bool shouldSkipCurrentFailure(const String& epubPath, const String& failedPath) {
        File failed = Board::Storage::filesystem().open(failedPath);
        if (!failed) {
            return false;
        }

        const bool failedMarker = !failed.isDirectory();
        const bool currentFailure = failedMarker && markerWasWrittenByCurrentConverter(failed);
        failed.close();

        if (!failedMarker) {
            return false;
        }
        if (currentFailure) {
            Serial.printf("[epub] Skipping EPUB with failure marker: %s\n", epubPath.c_str());
            return true;
        }

        Serial.printf("[epub] Retrying EPUB after converter update: %s\n", epubPath.c_str());
        Board::Storage::filesystem().remove(failedPath);
        return false;
    }

} // namespace

bool EpubConverter::isCurrentCache(const String& rsvpPath) {
    File existing = Board::Storage::filesystem().open(rsvpPath);
    const bool current = rsvpWasWrittenByCurrentConverter(existing);
    if (existing) {
        existing.close();
    }
    return current;
}

bool EpubConverter::convertIfNeeded(const String& epubPath, const String& rsvpPath, const Options& options) {
    if (removeStaleCacheOrReuseCurrent(rsvpPath)) {
        return true;
    }

    const ConversionPaths paths = conversionPathsFor(rsvpPath);
    if (previousCurrentAttemptRestarted(epubPath, paths, options)) {
        return false;
    }

    removeOrphanedTempFile(epubPath, paths.temp);
    if (shouldSkipCurrentFailure(epubPath, paths.failed)) {
        return false;
    }

    Serial.printf("[epub] Converting on device: %s\n", epubPath.c_str());
    writeFailureMarker(paths.lock, "Conversion in progress. Delete this file only if retrying.");
    const bool converted = convertEpubToRsvp(epubPath, paths.temp, rsvpPath, options);
    Board::Storage::filesystem().remove(paths.lock);
    if (!converted) {
        writeFailureMarker(paths.failed, "Conversion failed. Remove this marker to retry.");
        return false;
    }

    Board::Storage::filesystem().remove(paths.failed);
    return true;
}
