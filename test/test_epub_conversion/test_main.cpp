#include <Arduino.h>
#include <FS.h>
#include <unity.h>

#include <span>
#include <string>
#include <vector>

#include "conversion/epub/EpubContentParser.h"
#include "conversion/epub/EpubPackage.h"
#include "conversion/rsvp/RsvpWriter.h"

void setUp() {}
void tearDown() {}

namespace {

    std::string deterministicWriterFixture() {
        File output;
        RsvpWriter writer(output,
                          {.source = "fixture.txt",
                           .title = "Fixture",
                           .author = "Author",
                           .converter = "fixture-v1",
                           .language = "en"},
                          3);
        writer.setLanguage("en");
        writer.setDirection("ltr");
        writer.setDirection("ltr");
        writer.writeChapter("One");
        writer.writeChapter("One");
        writer.writeText("one two three four");
        writer.finish();
        return output.contents();
    }

    void test_rsvp_writer_is_deterministic_and_idempotent() {
        const std::string first = deterministicWriterFixture();
        TEST_ASSERT_EQUAL_STRING(first.c_str(), deterministicWriterFixture().c_str());
        TEST_ASSERT_TRUE(first.starts_with("@rsvp 1\n@title Fixture\n@source fixture.txt\n@author Author\n"
                                          "@converter fixture-v1\n@language en\n"));
        TEST_ASSERT_TRUE(first.contains("@direction ltr\n@chapter One\none two three\n"));
        TEST_ASSERT_EQUAL(std::string::npos, first.find("@direction ltr", first.find("@direction ltr") + 1));
        TEST_ASSERT_EQUAL(std::string::npos, first.find("@chapter One", first.find("@chapter One") + 1));
    }

    void test_package_parses_nav_metadata_and_encoded_manifest_paths() {
        constexpr std::string_view opf = R"(<package version="3.0"><metadata><dc:title>Fixture</dc:title><dc:language>ja</dc:language></metadata><manifest>
            <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
            <item id="chapter" href="Text/Chapter%20One.xhtml" media-type="application/xhtml+xml"/>
            </manifest><spine><itemref idref="chapter"/></spine></package>)";

        const auto manifest = EpubPackage::parseManifestItems(opf, "OEBPS/");
        TEST_ASSERT_EQUAL_STRING("3.0", EpubPackage::parsePackageVersion(opf).c_str());
        TEST_ASSERT_EQUAL_STRING("ja", EpubPackage::parseDcMetadata(opf, "language").c_str());
        TEST_ASSERT_EQUAL_UINT32(2, manifest.size());
        TEST_ASSERT_EQUAL_STRING("nav", manifest[0].properties.c_str());
        TEST_ASSERT_EQUAL_STRING("OEBPS/Text/Chapter One.xhtml", manifest[1].path.c_str());
    }

    void test_nav_toc_flattens_nested_entries_and_decodes_fragments() {
        constexpr std::string_view nav = R"(<html><body><nav epub:type="landmarks"><a href="cover.xhtml">Start</a></nav>
            <nav epub:type="toc"><ol>
            <li><a href="one.xhtml#one">Part One</a><ol>
                <li><a href="Text/Chapter%20One.xhtml#section%201">Chapter One A</a></li>
            </ol></li><li><a href="two.xhtml#two">Part Two</a></li>
            </ol></nav></body></html>)";

        const auto entries = EpubPackage::parseNavTocEntries(nav, "OEBPS/nav.xhtml", "Fixture");
        TEST_ASSERT_EQUAL_UINT32(3, entries.size());
        TEST_ASSERT_EQUAL_STRING("Part One", entries[0].title.c_str());
        TEST_ASSERT_EQUAL_STRING("OEBPS/Text/Chapter One.xhtml", entries[1].path.c_str());
        TEST_ASSERT_EQUAL_STRING("section 1", entries[1].fragment.c_str());
        TEST_ASSERT_EQUAL_STRING("Part Two", entries[2].title.c_str());
    }

    void test_ncx_toc_ignores_non_content_labels() {
        constexpr std::string_view ncx = R"(<ncx><navMap>
            <navPoint><navLabel><text>Fixture</text></navLabel><content src="title.xhtml"/></navPoint>
            <navPoint><navLabel><text>Contents</text></navLabel><content src="toc.xhtml"/></navPoint>
            <navPoint><navLabel><text>I. The Arrival</text></navLabel><content src="content.xhtml#chapter-1"/></navPoint>
            </navMap></ncx>)";

        const auto entries = EpubPackage::parseNcxTocEntries(ncx, "OEBPS/toc.ncx", "Fixture");
        TEST_ASSERT_EQUAL_UINT32(1, entries.size());
        TEST_ASSERT_EQUAL_STRING("I. The Arrival", entries[0].title.c_str());
        TEST_ASSERT_EQUAL_STRING("OEBPS/content.xhtml", entries[0].path.c_str());
        TEST_ASSERT_EQUAL_STRING("chapter-1", entries[0].fragment.c_str());
    }

    void test_parser_uses_ordered_toc_labels_and_paragraph_markers() {
        File output;
        const std::vector<EpubPackage::TocEntry> toc = {
            {"content.xhtml", "I. The Arrival", "chapter-1"},
            {"content.xhtml", "II. Father and Son", "chapter-2"},
        };
        constexpr std::string_view markup = R"(<body><h1>Book Title</h1><h2 id="chapter-1">I</h2>
            <p>The harbour was bright.</p><h2 id="chapter-2">II</h2><p>The door opened.</p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Book Title"});
        EpubContent::Parser parser(writer, toc, true, "content", "Book Title");
        TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(parser.finish());
        TEST_ASSERT_TRUE(writer.finish());

        TEST_ASSERT_EQUAL_UINT32(2, writer.chapterCount());
        TEST_ASSERT_TRUE(output.contents().ends_with(
            "@chapter I. The Arrival\n\n@chapter II. Father and Son\nThe harbour was bright.\n\n@para\nThe door opened.\n"));
    }

    void test_parser_preserves_punctuation_only_inline_fragments_without_counting_them_as_words() {
        File output;
        constexpr std::string_view markup = R"(<body><h1>Letter</h1><p>Dear Reader <span>,</span></p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Letter"});
        EpubContent::Parser parser(writer, {}, false, "Letter", "Letter");
        TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(parser.finish());
        TEST_ASSERT_TRUE(writer.finish());

        TEST_ASSERT_EQUAL_UINT32(2, writer.wordCount());
        TEST_ASSERT_TRUE(output.contents().ends_with("@chapter Letter\nDear Reader ,\n"));
    }

    void test_parser_preserves_nested_language_and_direction_changes() {
        File output;
        constexpr std::string_view markup =
            R"(<body lang="ja"><p>日本語 <span xml:lang="en" dir="ltr"><span>English</span></span> 続き</p><p lang="ar" dir="rtl">مرحبا 123</p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Fixture", .language = "ja"});
        EpubContent::Parser parser(writer, {}, false, "Fixture", "Fixture");
        TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(parser.finish());
        TEST_ASSERT_TRUE(writer.finish());

        TEST_ASSERT_TRUE(output.contents().contains("日本語\n@language en\n@direction ltr\nEnglish\n"));
        TEST_ASSERT_TRUE(output.contents().contains("@language ja\n@direction auto\n続き\n"));
        TEST_ASSERT_TRUE(output.contents().contains("@language ar\n@direction rtl\n"));
        TEST_ASSERT_TRUE(output.contents().contains("مرحبا 123\n@language ja\n@direction auto\n"));
    }

    void test_parser_emits_explicit_vertical_writing_mode_once() {
        File output;
        constexpr std::string_view markup =
            R"(<head><style>html { -epub-writing-mode: vertical-rl; }</style></head><body><p style="writing-mode: vertical-rl">日本語</p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Fixture", .language = "ja"});
        EpubContent::Parser firstParser(writer, {}, false, "Fixture", "Fixture");
        TEST_ASSERT_TRUE(firstParser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(firstParser.finish());
        EpubContent::Parser secondParser(writer, {}, false, "Second", "Fixture");
        TEST_ASSERT_TRUE(secondParser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(secondParser.finish());
        TEST_ASSERT_TRUE(writer.finish());
        const size_t first = output.contents().find("@writing-mode vertical-rl");
        TEST_ASSERT_NOT_EQUAL(std::string::npos, first);
        TEST_ASSERT_EQUAL(std::string::npos, output.contents().find("@writing-mode vertical-rl", first + 1));
    }

    // Buffered text is flushed once it grows past 220 bytes. When the byte that crosses the
    // threshold is the word boundary itself, that boundary has to survive the flush, otherwise the
    // next word is glued onto the buffered remainder.
    //
    // "Dear Reader " is 12 bytes, so 18 of them fill the buffer to 216; the following "Dear" ends
    // it at exactly 220 and the separator after it is the byte that triggers the flush.
    void test_parser_keeps_word_boundary_across_buffered_text_flush() {
        const std::string padding = [] {
            std::string filled;
            for (int i = 0; i < 18; ++i)
                filled += "Dear Reader ";
            return filled;
        }();

        // A literal space and a "&nbsp;" entity are the same boundary: the entity decodes to a
        // plain space before it ever reaches the buffer.
        for (const std::string_view separator : {" ", "&nbsp;"}) {
            File output;
            const std::string markup =
                "<body><p>" + padding + "Dear" + std::string{separator} + "Reader follows</p></body>";

            RsvpWriter writer(output, {.source = "fixture.epub", .title = "Fixture"});
            EpubContent::Parser parser(writer, {}, false, "Fixture", "Fixture");
            TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
            TEST_ASSERT_TRUE(parser.finish());
            TEST_ASSERT_TRUE(writer.finish());

            TEST_ASSERT_EQUAL(std::string::npos, output.contents().find("DearReader"));
            TEST_ASSERT_TRUE(output.contents().contains("Dear\nReader follows\n"));
        }
    }

} // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rsvp_writer_is_deterministic_and_idempotent);
    RUN_TEST(test_package_parses_nav_metadata_and_encoded_manifest_paths);
    RUN_TEST(test_nav_toc_flattens_nested_entries_and_decodes_fragments);
    RUN_TEST(test_ncx_toc_ignores_non_content_labels);
    RUN_TEST(test_parser_uses_ordered_toc_labels_and_paragraph_markers);
    RUN_TEST(test_parser_preserves_punctuation_only_inline_fragments_without_counting_them_as_words);
    RUN_TEST(test_parser_preserves_nested_language_and_direction_changes);
    RUN_TEST(test_parser_emits_explicit_vertical_writing_mode_once);
    RUN_TEST(test_parser_keeps_word_boundary_across_buffered_text_flush);
    return UNITY_END();
}
