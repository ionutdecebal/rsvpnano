#include "converter/EpubPackage.h"

#include <algorithm>
#include <array>

#include "converter/EpubContentWriter.h"
#include "text/AsciiText.h"

namespace EpubPackage {
    namespace {

        constexpr bool isAttributeNameBoundary(char c) {
            return AsciiText::isWhitespace(c) || c == '<' || c == '/';
        }

        constexpr bool isTagNameBoundary(char c) {
            return AsciiText::isWhitespace(c) || c == '/' || c == '>';
        }

        int skipAsciiWhitespace(const String& text, int position) {
            while (static_cast<size_t>(position) < text.length() && AsciiText::isWhitespace(text[position])) {
                ++position;
            }
            return position;
        }

        String percentDecodePath(const String& path) {
            String decoded;
            decoded.reserve(path.length());

            for (size_t i = 0; i < path.length(); ++i) {
                if (path[i] == '%' && i + 2 < path.length()) {
                    const int high = AsciiText::hexValue(path[i + 1]);
                    const int low = AsciiText::hexValue(path[i + 2]);
                    if (high >= 0 && low >= 0) {
                        decoded += static_cast<char>((high << 4) | low);
                        i += 2;
                        continue;
                    }
                }
                decoded += path[i];
            }

            return decoded;
        }

        String collapseZipPath(const String& path) {
            std::vector<String> parts;
            size_t start = 0;

            while (start <= path.length()) {
                int separator = path.indexOf('/', start);
                if (separator < 0) {
                    separator = path.length();
                }

                String part = path.substring(start, separator);
                if (part == "..") {
                    if (!parts.empty()) {
                        parts.pop_back();
                    }
                } else if (!part.isEmpty() && part != ".") {
                    parts.push_back(part);
                }

                if (static_cast<size_t>(separator) >= path.length()) {
                    break;
                }
                start = static_cast<size_t>(separator) + 1;
            }

            String collapsed;
            collapsed.reserve(path.length());
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) {
                    collapsed += '/';
                }
                collapsed += parts[i];
            }
            return collapsed;
        }

        String resolveZipPath(const String& baseDirectory, const String& href) {
            String path = href;

            int fragment = path.indexOf('#');
            if (fragment >= 0) {
                path = path.substring(0, fragment);
            }
            int query = path.indexOf('?');
            if (query >= 0) {
                path = path.substring(0, query);
            }

            path = percentDecodePath(path);
            path = normalizeZipName(path);
            if (!href.startsWith("/")) {
                path = baseDirectory + path;
            }

            return collapseZipPath(path);
        }

        String attributeValue(const String& tag, const char* name) {
            const String key(name);
            int position = 0;

            const auto readAttributeValue = [&](int valueStart) {
                const char quote = tag[valueStart];
                if (quote == '"' || quote == '\'') {
                    const int end = tag.indexOf(quote, valueStart + 1);
                    return end < 0 ? String("") : tag.substring(valueStart + 1, end);
                }

                int end = valueStart;
                while (static_cast<size_t>(end) < tag.length() && !AsciiText::isWhitespace(tag[end])
                       && tag[end] != '>') {
                    ++end;
                }
                return tag.substring(valueStart, end);
            };

            while (position >= 0 && static_cast<size_t>(position) < tag.length()) {
                position = tag.indexOf(key, position);
                if (position < 0) {
                    return "";
                }

                const bool boundaryBefore = position == 0 || isAttributeNameBoundary(tag[position - 1]);
                int afterName = position + key.length();
                const bool boundaryAfter = static_cast<size_t>(afterName) >= tag.length()
                                        || AsciiText::isWhitespace(tag[afterName]) || tag[afterName] == '=';
                if (!boundaryBefore || !boundaryAfter) {
                    position = afterName;
                    continue;
                }

                afterName = skipAsciiWhitespace(tag, afterName);
                if (static_cast<size_t>(afterName) >= tag.length() || tag[afterName] != '=') {
                    position = afterName;
                    continue;
                }
                afterName = skipAsciiWhitespace(tag, afterName + 1);
                if (static_cast<size_t>(afterName) >= tag.length()) {
                    return "";
                }

                return readAttributeValue(afterName);
            }

            return "";
        }

    } // namespace

    String toLowerCopy(String value) {
        value.toLowerCase();
        return value;
    }

    String basenameWithoutExtension(const String& path) {
        const int separator = path.lastIndexOf('/');
        String name = separator >= 0 ? path.substring(separator + 1) : path;
        const int dot = name.lastIndexOf('.');
        if (dot > 0) {
            name = name.substring(0, dot);
        }
        name.trim();
        return name.isEmpty() ? String("Untitled") : name;
    }

    String normalizeZipName(String path) {
        path.replace('\\', '/');
        while (path.startsWith("/")) {
            path.remove(0, 1);
        }
        return path;
    }

    bool isArchiveHintEntry(const String& name) {
        static constexpr std::array<const char*, 5> kArchiveHintExtensions = {{
            ".opf",
            ".ncx",
            ".xhtml",
            ".html",
            ".htm",
        }};

        const String lowered = toLowerCopy(name);
        return lowered.indexOf("container") >= 0
            || std::any_of(kArchiveHintExtensions.begin(), kArchiveHintExtensions.end(), [&](const char* extension) {
                   return lowered.endsWith(extension);
               });
    }

    String directoryForPath(const String& path) {
        const int separator = path.lastIndexOf('/');
        if (separator < 0) {
            return "";
        }
        return path.substring(0, separator + 1);
    }

    bool isContentDocument(const ManifestItem& item) {
        static constexpr std::array<const char*, 3> kContentExtensions = {{
            ".xhtml",
            ".html",
            ".htm",
        }};

        const String mediaType = toLowerCopy(item.mediaType);
        const String path = toLowerCopy(item.path);
        return mediaType == "application/xhtml+xml" || mediaType == "text/html"
            || std::any_of(kContentExtensions.begin(), kContentExtensions.end(), [&](const char* extension) {
                   return path.endsWith(extension);
               });
    }

    String parseRootfilePath(const String& containerXml) {
        int position = 0;
        while (position >= 0) {
            position = containerXml.indexOf("<rootfile", position);
            if (position < 0) {
                break;
            }

            const int end = containerXml.indexOf('>', position);
            if (end < 0) {
                break;
            }

            const String tag = containerXml.substring(position, end + 1);
            const String path = attributeValue(tag, "full-path");
            if (!path.isEmpty()) {
                return normalizeZipName(path);
            }

            position = end + 1;
        }

        return "";
    }

    String parseDcMetadata(const String& opfXml, const char* tagName) {
        const String openTag = String("<dc:") + tagName;
        const String closeTag = String("</dc:") + tagName;
        int position = 0;
        while (position >= 0) {
            position = opfXml.indexOf(openTag, position);
            if (position < 0) {
                break;
            }

            const int openEnd = opfXml.indexOf('>', position);
            if (openEnd < 0) {
                break;
            }
            const int closeStart = opfXml.indexOf(closeTag, openEnd + 1);
            if (closeStart < 0) {
                break;
            }

            const String value = EpubContent::plainTextFromXmlFragment(opfXml.substring(openEnd + 1, closeStart));
            if (!value.isEmpty()) {
                return value;
            }

            position = closeStart + 1;
        }

        return "";
    }

    std::vector<ManifestItem> parseManifestItems(const String& opfXml, const String& opfBaseDir) {
        std::vector<ManifestItem> items;
        int position = 0;

        while (position >= 0) {
            position = opfXml.indexOf("<item", position);
            if (position < 0) {
                break;
            }

            const int afterName = position + 5;
            if (static_cast<size_t>(afterName) < opfXml.length() && !isTagNameBoundary(opfXml[afterName])) {
                position = afterName;
                continue;
            }

            const int end = opfXml.indexOf('>', position);
            if (end < 0) {
                break;
            }

            const String tag = opfXml.substring(position, end + 1);
            ManifestItem item;
            item.id = attributeValue(tag, "id");
            item.path = resolveZipPath(opfBaseDir, attributeValue(tag, "href"));
            item.mediaType = attributeValue(tag, "media-type");

            if (!item.id.isEmpty() && !item.path.isEmpty()) {
                items.push_back(item);
            }

            position = end + 1;
        }

        return items;
    }

    std::vector<String> parseSpineIds(const String& opfXml) {
        std::vector<String> ids;
        int position = 0;

        while (position >= 0) {
            position = opfXml.indexOf("<itemref", position);
            if (position < 0) {
                break;
            }

            const int end = opfXml.indexOf('>', position);
            if (end < 0) {
                break;
            }

            const String tag = opfXml.substring(position, end + 1);
            const String idref = attributeValue(tag, "idref");
            if (!idref.isEmpty()) {
                ids.push_back(idref);
            }

            position = end + 1;
        }

        return ids;
    }

    const ManifestItem* findManifestItem(const std::vector<ManifestItem>& items, const String& id) {
        const auto item = std::find_if(items.begin(), items.end(), [&](const ManifestItem& candidate) {
            return candidate.id == id;
        });
        return item == items.end() ? nullptr : &(*item);
    }

} // namespace EpubPackage
