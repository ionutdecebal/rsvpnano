#pragma once

#include <Arduino.h>
#include <vector>

namespace EpubPackage {

    struct ManifestItem {
        String id;
        String path;
        String mediaType;
        String properties;
    };

    struct TocEntry {
        String path;
        String title;
    };

    String toLowerCopy(String value);
    String basenameWithoutExtension(const String& path);
    String normalizeZipName(String path);
    bool isArchiveHintEntry(const String& name);
    String directoryForPath(const String& path);
    bool isContentDocument(const ManifestItem& item);
    String parseRootfilePath(const String& containerXml);
    String parseDcMetadata(const String& opfXml, const char* tagName);
    std::vector<ManifestItem> parseManifestItems(const String& opfXml, const String& opfBaseDir);
    std::vector<String> parseSpineIds(const String& opfXml);
    String findNavDocumentPath(const std::vector<ManifestItem>& items);
    String findNcxDocumentPath(const String& opfXml, const std::vector<ManifestItem>& items);
    std::vector<TocEntry> parseNavTocEntries(const String& navXml, const String& navBaseDir);
    std::vector<TocEntry> parseNcxTocEntries(const String& ncxXml, const String& ncxBaseDir);
    const ManifestItem* findManifestItem(const std::vector<ManifestItem>& items, const String& id);

} // namespace EpubPackage
