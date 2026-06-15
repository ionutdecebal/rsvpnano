#pragma once

#include <Arduino.h>
#include <vector>

namespace BookLibrary {

    struct Listing {
        std::vector<String> paths;
        std::vector<String> titles;
        std::vector<String> authors;
    };

    void clear(Listing& listing);
    void refresh(Listing& listing, bool includeMetadata, bool onDeviceEpubConversionEnabled);
    void printListing(const Listing& listing);
    size_t unsupportedFileCount();

    String pathAt(const Listing& listing, size_t index);
    bool isArticle(const Listing& listing, size_t index);
    String displayName(const Listing& listing, size_t index);
    String authorName(const Listing& listing, size_t index);
    int indexOfPath(const Listing& listing, const String& target);

} // namespace BookLibrary
