#pragma once

#include <Arduino.h>
#include <FS.h>

namespace EpubContent {

    String plainTextFromXmlFragment(const String& fragment);
    bool writeBodyLine(File& output, const String& line, size_t& wordCount, size_t maxWords);
    bool writeChapterDirective(File& output, const String& title, String& lastChapterTitle);

    class RsvpContentWriter {
    public:
        RsvpContentWriter(File& output, size_t& wordCount, size_t maxWords, String& lastChapterTitle,
                          bool allowHeadingChapters = true);

        bool write(const uint8_t* data, size_t length);
        bool finish();
        bool reachedWordLimit() const;

    private:
        enum class Mode {
            Text,
            Tag,
            Entity,
            Comment,
        };

        bool flushLine();
        void appendToActiveText(char c);
        bool processDecodedText(char c);
        bool processTextChar(char c);
        bool processTag(const String& tag);
        bool processEntityChar(char c);
        bool processCommentChar(char c);
        bool processChar(char c);

        File& output_;
        size_t& wordCount_;
        const size_t maxWords_;
        String& lastChapterTitle_;
        String line_;
        String heading_;
        String tag_;
        String entity_;
        String commentTail_;
        Mode mode_ = Mode::Text;
        bool inHeading_ = false;
        const bool allowHeadingChapters_;
        bool reachedWordLimit_ = false;
        int skipDepth_ = 0;
    };

} // namespace EpubContent
