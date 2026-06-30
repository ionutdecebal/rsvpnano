#include "storage/fs/StoragePaths.h"

#include <cstring>

namespace StoragePaths {
    namespace {

        bool hasExtension(const String& path, const char* extension) {
            String lowered = path;
            lowered.toLowerCase();
            return lowered.endsWith(extension);
        }

    } // namespace

    bool hasTextExtension(const String& path) {
        return hasExtension(path, kTextExtension);
    }

    bool hasRsvpExtension(const String& path) {
        return hasExtension(path, kRsvpExtension);
    }

    bool hasEpubExtension(const String& path) {
        return hasExtension(path, kEpubExtension);
    }

    String parentDirectoryForPath(const String& path) {
        const int separator = path.lastIndexOf('/');
        if (separator <= 0) {
            return "/";
        }
        return path.substring(0, separator);
    }

    String siblingPathWithExtension(const String& path, const char* extension) {
        String siblingPath = path;
        const int dot = siblingPath.lastIndexOf('.');
        if (dot > 0) {
            siblingPath = siblingPath.substring(0, dot);
        }
        siblingPath += extension;
        return siblingPath;
    }

    String epubSiblingPathForRsvp(const String& rsvpPath) {
        return siblingPathWithExtension(rsvpPath, kEpubExtension);
    }

    String displayNameForPath(const String& path) {
        const int separator = path.lastIndexOf('/');
        if (separator < 0) {
            return path;
        }
        return path.substring(separator + 1);
    }

    String displayNameWithoutExtension(const String& path) {
        String name = displayNameForPath(path);
        String lowered = name;
        lowered.toLowerCase();
        if (lowered.endsWith(kTextExtension)) {
            name.remove(name.length() - std::strlen(kTextExtension));
        } else if (lowered.endsWith(kRsvpExtension)) {
            name.remove(name.length() - std::strlen(kRsvpExtension));
        } else if (lowered.endsWith(kEpubExtension)) {
            name.remove(name.length() - std::strlen(kEpubExtension));
        }
        return name;
    }

    String rsvpCachePathForEpub(const String& epubPath) {
        return siblingPathWithExtension(epubPath, kRsvpExtension);
    }

    String indexedIndexPathFor(const String& path) {
        return path + kIndexExtension;
    }

    String indexedDataPathFor(const String& path) {
        return path + kDataExtension;
    }

    String progressSidecarPathFor(const String& path) {
        return siblingPathWithExtension(path, kProgressExtension);
    }

    String indexedTempPathFor(const String& path) {
        return path + kTempExtension;
    }

    bool isHiddenOrSidecarPath(const String& path) {
        const String name = displayNameForPath(path);
        if (name.length() == 0) {
            return true;
        }

        String lowered = name;
        lowered.toLowerCase();
        if (lowered.startsWith(".")) {
            return true;
        }

        if (lowered.endsWith(kIndexExtension) || lowered.endsWith(kDataExtension)
            || lowered.endsWith(kProgressExtension) || lowered.endsWith(kTempExtension)) {
            return true;
        }

        return lowered == "thumbs.db" || lowered == "desktop.ini";
    }

} // namespace StoragePaths
