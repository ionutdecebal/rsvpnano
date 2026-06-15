#include "converter/EpubContentWriter.h"

#include <algorithm>
#include <array>

#include "text/AsciiText.h"
#include "text/LatinText.h"
#include "text/RsvpTokenizer.h"

namespace EpubContent {
    namespace {

        constexpr size_t kMaxEntityChars = 16;
        constexpr size_t kMaxTagChars = 512;
        constexpr size_t kOutputWrapWidth = 96;
        constexpr size_t kBufferedTextFlushThreshold = 220;

        struct NamedEntity {
            const char* name;
            uint8_t value;
        };

        struct TagInfo {
            String name;
            bool closing = false;
            bool selfClosing = false;
        };

        constexpr bool isAsciiTagNameChar(char c) {
            return AsciiText::isAlphaNumeric(c) || c == ':' || c == '-' || c == '_';
        }

        void serviceBackground() {
            yield();
            delay(0);
        }

        char entityCodepointByte(uint32_t codepoint) {
            uint8_t storedByte = 0;
            if (LatinText::storageByteForCodepoint(codepoint, storedByte)) {
                return static_cast<char>(storedByte);
            }
            return ' ';
        }

        char entityPunctuationChar(uint32_t codepoint) {
            if (codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
                return static_cast<char>(codepoint - 0xFEE0);
            }

            switch (codepoint) {
            case 0x00A0:
                return ' ';
            case 0x00AB:
            case 0x00BB:
            case 0x201C:
            case 0x201D:
            case 0x201E:
            case 0x201F:
            case 0x2033:
            case 0x2036:
            case 0x300C:
            case 0x300D:
            case 0x300E:
            case 0x300F:
                return '"';
            case 0x2018:
            case 0x2019:
            case 0x201A:
            case 0x201B:
            case 0x2032:
            case 0x2035:
            case 0x2039:
            case 0x203A:
                return '\'';
            case 0x2010:
            case 0x2011:
            case 0x2012:
            case 0x2013:
            case 0x2014:
            case 0x2015:
            case 0x2043:
            case 0x2212:
                return '-';
            case 0x2022:
            case 0x00B7:
            case 0x2219:
                return '*';
            case 0x2026:
                return '.';
            case 0x207D:
            case 0x208D:
            case 0x2768:
            case 0x276A:
                return '(';
            case 0x207E:
            case 0x208E:
            case 0x2769:
            case 0x276B:
                return ')';
            case 0x2045:
            case 0x2308:
            case 0x230A:
            case 0x3010:
            case 0x3014:
            case 0x3016:
            case 0x3018:
            case 0x301A:
                return '[';
            case 0x2046:
            case 0x2309:
            case 0x230B:
            case 0x3011:
            case 0x3015:
            case 0x3017:
            case 0x3019:
            case 0x301B:
                return ']';
            case 0x2774:
            case 0x2776:
                return '{';
            case 0x2775:
            case 0x2777:
                return '}';
            case 0x2329:
            case 0x27E8:
            case 0x3008:
            case 0x300A:
                return '<';
            case 0x232A:
            case 0x27E9:
            case 0x3009:
            case 0x300B:
                return '>';
            case 0xFF0C:
                return ',';
            case 0xFF0E:
                return '.';
            case 0xFF1A:
                return ':';
            case 0xFF1B:
                return ';';
            case 0xFF01:
                return '!';
            case 0xFF1F:
                return '?';
            default:
                return '\0';
            }
        }

        bool parseNumericEntityCodepoint(const String& entity, uint32_t& value) {
            if (!entity.startsWith("#")) {
                return false;
            }

            value = 0;
            int start = 1;
            int base = 10;
            if (entity.length() > 2 && (entity[1] == 'x' || entity[1] == 'X')) {
                start = 2;
                base = 16;
            }
            if (static_cast<size_t>(start) >= entity.length()) {
                return false;
            }

            for (size_t i = start; i < entity.length(); ++i) {
                const int digit = base == 16 ? AsciiText::hexValue(entity[i])
                                             : (AsciiText::isDigit(entity[i]) ? entity[i] - '0' : -1);
                if (digit < 0 || digit >= base) {
                    return false;
                }
                value = value * base + static_cast<uint32_t>(digit);
            }

            return true;
        }

        bool isSentenceDashCodepoint(uint32_t codepoint) {
            return codepoint == 0x2012 || codepoint == 0x2013 || codepoint == 0x2014 || codepoint == 0x2015;
        }

        char decodedEntityChar(const String& entity) {
            const auto findEntity = [&](const NamedEntity* entries, size_t count) -> char {
                const NamedEntity* end = entries + count;
                const NamedEntity* found = std::find_if(entries, end, [&](const NamedEntity& entry) {
                    return entity == entry.name;
                });
                return found == end ? '\0' : static_cast<char>(found->value);
            };

            static constexpr std::array<NamedEntity, 33> kNamedEntities = {{
                {"amp", '&'},    {"lt", '<'},      {"gt", '>'},     {"quot", '"'},   {"apos", '\''},   {"nbsp", ' '},
                {"iexcl", 0x16}, {"iquest", 0x17}, {"ldquo", '"'},  {"rdquo", '"'},  {"bdquo", '"'},   {"lsquo", '\''},
                {"rsquo", '\''}, {"sbquo", '\''},  {"laquo", '"'},  {"raquo", '"'},  {"lsaquo", '\''}, {"rsaquo", '\''},
                {"lpar", '('},   {"rpar", ')'},    {"lbrack", '['}, {"rbrack", ']'}, {"lcub", '{'},    {"rcub", '}'},
                {"ndash", '-'},  {"mdash", '-'},   {"hyphen", '-'}, {"minus", '-'},  {"hellip", '.'},  {"middot", '*'},
                {"bull", '*'},   {"times", 'x'},   {"divide", '/'},
            }};
            const char namedEntity = findEntity(kNamedEntities.data(), kNamedEntities.size());
            if (namedEntity != '\0') {
                return namedEntity;
            }

            static constexpr std::array<NamedEntity, 62> kLatin1Entities = {{
                {"Agrave", 0xC0}, {"Aacute", 0xC1}, {"Acirc", 0xC2},  {"Atilde", 0xC3}, {"Auml", 0xC4},
                {"Aring", 0xC5},  {"AElig", 0xC6},  {"Ccedil", 0xC7}, {"Egrave", 0xC8}, {"Eacute", 0xC9},
                {"Ecirc", 0xCA},  {"Euml", 0xCB},   {"Igrave", 0xCC}, {"Iacute", 0xCD}, {"Icirc", 0xCE},
                {"Iuml", 0xCF},   {"ETH", 0xD0},    {"Ntilde", 0xD1}, {"Ograve", 0xD2}, {"Oacute", 0xD3},
                {"Ocirc", 0xD4},  {"Otilde", 0xD5}, {"Ouml", 0xD6},   {"Oslash", 0xD8}, {"Ugrave", 0xD9},
                {"Uacute", 0xDA}, {"Ucirc", 0xDB},  {"Uuml", 0xDC},   {"Yacute", 0xDD}, {"THORN", 0xDE},
                {"szlig", 0xDF},  {"agrave", 0xE0}, {"aacute", 0xE1}, {"acirc", 0xE2},  {"atilde", 0xE3},
                {"auml", 0xE4},   {"aring", 0xE5},  {"aelig", 0xE6},  {"ccedil", 0xE7}, {"egrave", 0xE8},
                {"eacute", 0xE9}, {"ecirc", 0xEA},  {"euml", 0xEB},   {"igrave", 0xEC}, {"iacute", 0xED},
                {"icirc", 0xEE},  {"iuml", 0xEF},   {"eth", 0xF0},    {"ntilde", 0xF1}, {"ograve", 0xF2},
                {"oacute", 0xF3}, {"ocirc", 0xF4},  {"otilde", 0xF5}, {"ouml", 0xF6},   {"oslash", 0xF8},
                {"ugrave", 0xF9}, {"uacute", 0xFA}, {"ucirc", 0xFB},  {"uuml", 0xFC},   {"yacute", 0xFD},
                {"thorn", 0xFE},  {"yuml", 0xFF},
            }};
            const char latin1Entity = findEntity(kLatin1Entities.data(), kLatin1Entities.size());
            if (latin1Entity != '\0') {
                return latin1Entity;
            }

            static constexpr std::array<NamedEntity, 58> kCustomEntities = {{
                {"Dcaron", 0x01}, {"dcaron", 0x02},       {"Ecaron", 0x03}, {"ecaron", 0x04},
                {"Ncaron", 0x05}, {"ncaron", 0x06},       {"Rcaron", 0x07}, {"rcaron", 0x08},
                {"Tcaron", 0x0E}, {"tcaron", 0x0F},       {"Uring", 0x10},  {"uring", 0x11},
                {"Odblac", 0x12}, {"odblac", 0x13},       {"Udblac", 0x14}, {"udblac", 0x15},
                {"OElig", 0x80},  {"oelig", 0x81},        {"Scaron", 0x86}, {"scaron", 0x87},
                {"Zcaron", 0x88}, {"zcaron", 0x89},       {"Amacr", 0xA1},  {"amacr", 0xA2},
                {"Emacr", 0xA3},  {"emacr", 0xA4},        {"Gcedil", 0xA5}, {"Gcommaaccent", 0xA5},
                {"gcedil", 0xA6}, {"gcommaaccent", 0xA6}, {"Imacr", 0xA7},  {"imacr", 0xA8},
                {"Kcedil", 0xA9}, {"Kcommaaccent", 0xA9}, {"kcedil", 0xAA}, {"kcommaaccent", 0xAA},
                {"Lcedil", 0xAB}, {"Lcommaaccent", 0xAB}, {"lcedil", 0xAC}, {"lcommaaccent", 0xAC},
                {"Ncedil", 0xAE}, {"Ncommaaccent", 0xAE}, {"ncedil", 0xAF}, {"ncommaaccent", 0xAF},
                {"Edot", 0xB0},   {"edot", 0xB1},         {"Iogon", 0xB6},  {"iogon", 0xB7},
                {"Uogon", 0xB8},  {"uogon", 0xB9},        {"Umacr", 0xBA},  {"umacr", 0xBB},
                {"Dstrok", 0xBC}, {"dstrok", 0xBD},       {"ENG", 0xBE},    {"eng", 0xBF},
                {"Tstrok", 0xD7}, {"tstrok", 0xF7},
            }};
            const char customEntity = findEntity(kCustomEntities.data(), kCustomEntities.size());
            if (customEntity != '\0') {
                return customEntity;
            }

            uint32_t value = 0;
            if (parseNumericEntityCodepoint(entity, value)) {
                const char mapped = entityCodepointByte(value);
                if (mapped != ' ') {
                    return mapped;
                }
                const char punctuation = entityPunctuationChar(value);
                if (punctuation != '\0') {
                    return punctuation;
                }
            }

            return ' ';
        }

        String decodedEntityText(const String& entity) {
            if (entity == "ndash" || entity == "mdash") {
                return " - ";
            }
            if (entity == "hellip") {
                return "...";
            }

            uint32_t value = 0;
            if (parseNumericEntityCodepoint(entity, value)) {
                if (isSentenceDashCodepoint(value)) {
                    return " - ";
                }
                if (value == 0x2026) {
                    return "...";
                }
            }

            String decoded;
            decoded += decodedEntityChar(entity);
            return decoded;
        }

        void appendNormalizedChar(String& target, char c) {
            if (AsciiText::isWhitespace(c)) {
                if (!target.isEmpty() && target[target.length() - 1] != ' ') {
                    target += ' ';
                }
                return;
            }

            target += c;
        }

        bool hasReadableOrRhythmText(const String& token) {
            if (RsvpText::isRhythmToken(token)) {
                return true;
            }
            const char* text = token.c_str();
            return std::any_of(text, text + token.length(), [](char c) {
                return RsvpText::isReadableTokenChar(c);
            });
        }

        bool flushWordAlignedPrefix(File& output, String& line, size_t& wordCount, size_t maxWords) {
            line.trim();
            if (line.isEmpty()) {
                line = "";
                return true;
            }

            int split = static_cast<int>(line.length()) - 1;
            while (split >= 0 && !AsciiText::isWhitespace(line[split])) {
                --split;
            }

            if (split < 0) {
                return true;
            }

            String prefix = line.substring(0, split);
            String remainder = line.substring(split + 1);
            prefix.trim();
            remainder.trim();

            if (prefix.isEmpty()) {
                line = remainder;
                return true;
            }

            const bool keepGoing = writeBodyLine(output, prefix, wordCount, maxWords);
            line = remainder;
            return keepGoing;
        }

        TagInfo parseTagInfo(const String& tag) {
            TagInfo info;

            size_t position = 1;
            while (position < tag.length() && AsciiText::isWhitespace(tag[position])) {
                ++position;
            }
            if (position < tag.length() && tag[position] == '/') {
                info.closing = true;
                ++position;
            }
            while (position < tag.length() && AsciiText::isWhitespace(tag[position])) {
                ++position;
            }

            const size_t start = position;
            while (position < tag.length() && isAsciiTagNameChar(tag[position])) {
                ++position;
            }

            info.name = tag.substring(start, position);
            info.name.toLowerCase();

            for (int i = static_cast<int>(tag.length()) - 1; i >= 0; --i) {
                if (AsciiText::isWhitespace(tag[i]) || tag[i] == '>') {
                    continue;
                }
                info.selfClosing = tag[i] == '/';
                break;
            }

            return info;
        }

        bool isSkipTag(const String& name) {
            static constexpr std::array<const char*, 6> kSkippedTags = {{
                "head",
                "script",
                "style",
                "svg",
                "math",
                "nav",
            }};
            return std::any_of(kSkippedTags.begin(), kSkippedTags.end(), [&](const char* tag) {
                return name == tag;
            });
        }

        bool isHeadingTag(const String& name) {
            return name.length() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6';
        }

        bool isBlockTag(const String& name) {
            static constexpr std::array<const char*, 11> kBlockTags = {{
                "p",
                "div",
                "section",
                "article",
                "blockquote",
                "li",
                "tr",
                "br",
                "hr",
                "dd",
                "dt",
            }};
            return std::any_of(kBlockTags.begin(), kBlockTags.end(), [&](const char* tag) {
                return name == tag;
            });
        }

        bool writeChapterMarker(File& output, const String& title, String& lastChapterTitle) {
            String cleaned = RsvpText::normalizeDisplayText(title);
            cleaned.trim();
            if (cleaned.isEmpty() || cleaned == lastChapterTitle) {
                return true;
            }

            output.print("@chapter ");
            output.println(cleaned);
            lastChapterTitle = cleaned;
            return true;
        }

    } // namespace

    String plainTextFromXmlFragment(const String& fragment) {
        String text;
        text.reserve(std::min<size_t>(fragment.length(), 160));

        for (size_t i = 0; i < fragment.length(); ++i) {
            const char c = fragment[i];
            if (c == '<') {
                const int tagEnd = fragment.indexOf('>', i + 1);
                if (tagEnd < 0) {
                    break;
                }
                i = tagEnd;
                appendNormalizedChar(text, ' ');
                continue;
            }

            if (c == '&') {
                const int entityEnd = fragment.indexOf(';', i + 1);
                const int entityLength = entityEnd - static_cast<int>(i) - 1;
                if (entityEnd > 0 && entityLength >= 0 && entityLength <= static_cast<int>(kMaxEntityChars)) {
                    const String decoded = decodedEntityText(fragment.substring(i + 1, entityEnd));
                    std::for_each(decoded.c_str(), decoded.c_str() + decoded.length(), [&](char decodedChar) {
                        appendNormalizedChar(text, decodedChar);
                    });
                    i = entityEnd;
                    continue;
                }
            }

            appendNormalizedChar(text, c);
        }

        text.trim();
        return RsvpText::normalizeDisplayText(text);
    }

    bool writeBodyLine(File& output, const String& line, size_t& wordCount, size_t maxWords) {
        const String normalizedLine = RsvpText::normalizeDisplayText(line);
        String outputLine;

        auto flushOutputLine = [&]() {
            if (outputLine.isEmpty()) {
                return;
            }
            if (outputLine.startsWith("@")) {
                output.print('@');
            }
            output.println(outputLine);
            outputLine = "";
        };

        auto consumeRsvpToken = [&](const String& value) {
            if (value.isEmpty() || !hasReadableOrRhythmText(value)) {
                return true;
            }

            if (outputLine.length() + value.length() + 1 > kOutputWrapWidth) {
                flushOutputLine();
            }

            if (!outputLine.isEmpty()) {
                outputLine += ' ';
            }
            outputLine += value;
            ++wordCount;
            return true;
        };

        const bool keepGoing =
            RsvpText::appendNormalizedLineWords(normalizedLine, consumeRsvpToken, wordCount, maxWords);

        flushOutputLine();
        return keepGoing;
    }

    RsvpContentWriter::RsvpContentWriter(File& output, size_t& wordCount, size_t maxWords, String& lastChapterTitle) :
            output_(output),
            wordCount_(wordCount),
            maxWords_(maxWords),
            lastChapterTitle_(lastChapterTitle) {
        line_.reserve(160);
        heading_.reserve(80);
        tag_.reserve(96);
        entity_.reserve(16);
    }

    bool RsvpContentWriter::write(const uint8_t* data, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            if ((i & 0x3FF) == 0) {
                serviceBackground();
            }
            if (!processChar(static_cast<char>(data[i]))) {
                return false;
            }
        }

        return true;
    }

    bool RsvpContentWriter::finish() {
        mode_ = Mode::Text;
        return flushLine();
    }

    bool RsvpContentWriter::reachedWordLimit() const {
        return reachedWordLimit_;
    }

    bool RsvpContentWriter::flushLine() {
        line_.trim();
        if (line_.isEmpty()) {
            return true;
        }

        const bool keepGoing = writeBodyLine(output_, line_, wordCount_, maxWords_);
        line_ = "";
        if (!keepGoing) {
            reachedWordLimit_ = true;
        }
        return keepGoing;
    }

    void RsvpContentWriter::appendToActiveText(char c) {
        if (inHeading_) {
            appendNormalizedChar(heading_, c);
            return;
        }

        appendNormalizedChar(line_, c);
    }

    bool RsvpContentWriter::processDecodedText(char c) {
        if (skipDepth_ > 0) {
            return true;
        }

        appendToActiveText(c);
        if (!inHeading_ && line_.length() > kBufferedTextFlushThreshold) {
            return flushWordAlignedPrefix(output_, line_, wordCount_, maxWords_);
        }

        return true;
    }

    bool RsvpContentWriter::processTextChar(char c) {
        if (c == '<') {
            tag_ = "<";
            mode_ = Mode::Tag;
            return true;
        }

        if (c == '&') {
            if (skipDepth_ > 0) {
                return true;
            }
            entity_ = "";
            mode_ = Mode::Entity;
            return true;
        }

        return processDecodedText(c);
    }

    bool RsvpContentWriter::processTag(const String& tag) {
        const TagInfo tagInfo = parseTagInfo(tag);

        if (tagInfo.name.isEmpty() || tag.startsWith("<!") || tag.startsWith("<?")) {
            return true;
        }

        const bool skipTag = isSkipTag(tagInfo.name);

        // Skip ignored tag subtrees without letting their text reach the output buffer.
        {
            if (skipDepth_ > 0) {
                if (!tagInfo.closing && skipTag && !tagInfo.selfClosing) {
                    ++skipDepth_;
                } else if (tagInfo.closing && skipTag) {
                    --skipDepth_;
                }
                return true;
            }

            if (skipTag && !tagInfo.closing && !tagInfo.selfClosing) {
                if (!flushLine()) {
                    return false;
                }
                skipDepth_ = 1;
                return true;
            }
        }

        // Headings become RSVP chapter markers instead of body words.
        {
            if (isHeadingTag(tagInfo.name)) {
                if (tagInfo.closing) {
                    inHeading_ = false;
                    const String cleanedHeading = plainTextFromXmlFragment(heading_);
                    if (!writeChapterMarker(output_, cleanedHeading, lastChapterTitle_)) {
                        return false;
                    }
                    heading_ = "";
                } else if (!tagInfo.selfClosing) {
                    if (!flushLine()) {
                        return false;
                    }
                    inHeading_ = true;
                    heading_ = "";
                }
                return true;
            }
        }

        // Block tags either flush buffered words or introduce a spacing boundary.
        {
            const bool blockTag = isBlockTag(tagInfo.name);
            if (blockTag && (tagInfo.closing || tagInfo.name == "br" || tagInfo.name == "hr" || tagInfo.name == "li")) {
                return flushLine();
            }
            if (blockTag) {
                appendNormalizedChar(line_, ' ');
            }
        }

        return true;
    }

    bool RsvpContentWriter::processEntityChar(char c) {
        if (c == ';') {
            mode_ = Mode::Text;
            const String decoded = decodedEntityText(entity_);
            const bool processed = std::all_of(decoded.c_str(), decoded.c_str() + decoded.length(), [&](char decodedChar) {
                return processDecodedText(decodedChar);
            });
            if (!processed) {
                return false;
            }
            return true;
        }

        if (c == '<') {
            mode_ = Mode::Text;
            if (!processDecodedText(' ')) {
                return false;
            }
            return processTextChar(c);
        }

        if (entity_.length() >= kMaxEntityChars || AsciiText::isWhitespace(c)) {
            mode_ = Mode::Text;
            return processDecodedText(' ');
        }

        entity_ += c;
        return true;
    }

    bool RsvpContentWriter::processCommentChar(char c) {
        commentTail_ += c;
        if (commentTail_.length() > 3) {
            commentTail_.remove(0, commentTail_.length() - 3);
        }

        if (commentTail_ == "-->") {
            commentTail_ = "";
            mode_ = Mode::Text;
        }

        return true;
    }

    bool RsvpContentWriter::processChar(char c) {
        switch (mode_) {
        case Mode::Text:
            return processTextChar(c);
        case Mode::Entity:
            return processEntityChar(c);
        case Mode::Comment:
            return processCommentChar(c);
        case Mode::Tag:
            tag_ += c;
            if (tag_ == "<!--") {
                tag_ = "";
                commentTail_ = "";
                mode_ = Mode::Comment;
                return true;
            }
            if (tag_.length() > kMaxTagChars) {
                tag_ = "";
                mode_ = Mode::Text;
                return processDecodedText(' ');
            }
            if (c == '>') {
                const String completedTag = tag_;
                tag_ = "";
                mode_ = Mode::Text;
                return processTag(completedTag);
            }
            return true;
        }

        return true;
    }

} // namespace EpubContent
