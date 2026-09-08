#include <unity.h>
#include <vector>

#include "reader/ReadingLoop.h"
#include "text/RsvpTokenizer.h"
#include "text/UnicodeText.h"

static settings::ReadingSettings testSettings;
static std::vector<std::string> testWords;

void setUp() {
    testSettings = {};
}

void tearDown() {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ReadingSession makeReader(uint16_t wpm, std::vector<std::string> words) {
    testSettings.wpm = wpm;
    testWords = std::move(words);
    ReadingSession r;
    ReadingLoop::setWords(r, testWords, 0);
    return r;
}

// Duration of the first word when the second word is the contextual next.
static uint32_t duration(uint16_t wpm, const char* word, const char* next) {
    ReadingSession r = makeReader(wpm, {word, next});
    return ReadingLoop::currentWordDurationMs(r, testSettings);
}

// ---------------------------------------------------------------------------
// WPM / interval
// ---------------------------------------------------------------------------

void test_wpm_base_interval(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "a", "b"));
    TEST_ASSERT_EQUAL(100u, duration(600, "a", "b"));
}

void test_wpm_clamped_low(void) {
    testSettings.wpm = 5;
    TEST_ASSERT_EQUAL(10u, testSettings.wpm);

    testSettings.wpm = 50;
    TEST_ASSERT_EQUAL(50u, testSettings.wpm);
}

void test_wpm_clamped_high(void) {
    testSettings.wpm = 9999;
    TEST_ASSERT_EQUAL(1000u, testSettings.wpm);
}

void test_adjust_wpm_steps_by_25(void) {
    testSettings.wpm = 300;
    ReadingLoop::adjustWpm(testSettings, 1);
    TEST_ASSERT_EQUAL(325u, testSettings.wpm);
    ReadingLoop::adjustWpm(testSettings, -1);
    TEST_ASSERT_EQUAL(300u, testSettings.wpm);
}

void test_adjust_wpm_steps_by_10_below_100(void) {
    testSettings.wpm = 50;
    ReadingLoop::adjustWpm(testSettings, 1);
    TEST_ASSERT_EQUAL(60u, testSettings.wpm);
    ReadingLoop::adjustWpm(testSettings, -1);
    TEST_ASSERT_EQUAL(50u, testSettings.wpm);
}

void test_adjust_wpm_crosses_100_cleanly(void) {
    testSettings.wpm = 90;
    ReadingLoop::adjustWpm(testSettings, 1);
    TEST_ASSERT_EQUAL(100u, testSettings.wpm);

    ReadingLoop::adjustWpm(testSettings, -1);
    TEST_ASSERT_EQUAL(90u, testSettings.wpm);

    testSettings.wpm = 125;
    ReadingLoop::adjustWpm(testSettings, -1);
    TEST_ASSERT_EQUAL(100u, testSettings.wpm);
}

void test_adjust_wpm_clamped_at_bounds(void) {
    testSettings.wpm = 1000;
    ReadingLoop::adjustWpm(testSettings, 1);
    TEST_ASSERT_EQUAL(1000u, testSettings.wpm);

    testSettings.wpm = 10;
    ReadingLoop::adjustWpm(testSettings, -1);
    TEST_ASSERT_EQUAL(10u, testSettings.wpm);
}

// ---------------------------------------------------------------------------
// Duration: no bonus
// ---------------------------------------------------------------------------

void test_short_word_no_bonus(void) {
    // "a" → 1 syllable, 1 readable char, no punctuation → base only
    TEST_ASSERT_EQUAL(200u, duration(300, "a", "b"));
}

// ---------------------------------------------------------------------------
// Duration: punctuation pauses
// ---------------------------------------------------------------------------

void test_comma_pause(void) {
    // "hi," → comma → +45%  → 200 + 90 = 290
    TEST_ASSERT_EQUAL(290u, duration(300, "hi,", "there"));
}

void test_sentence_pause(void) {
    // "done." next "The" (uppercase) → sentence +135% → 200 + 270 = 470
    TEST_ASSERT_EQUAL(470u, duration(300, "done.", "The"));
}

void test_strong_sentence_pause(void) {
    // "yes!" → strong sentence +150% → 200 + 300 = 500
    TEST_ASSERT_EQUAL(500u, duration(300, "yes!", "The"));
}

void test_sentence_pause_preserved_with_closing_quote(void) {
    TEST_ASSERT_EQUAL(470u, duration(300, "\"done.\"", "The"));
}

void test_sentence_pause_preserved_with_closing_parenthesis(void) {
    TEST_ASSERT_EQUAL(470u, duration(300, "(done.)", "The"));
}

void test_clause_pause_semicolon(void) {
    // "thus;" → clause +80% → 200 + 160 = 360
    TEST_ASSERT_EQUAL(360u, duration(300, "thus;", "the"));
}

void test_dash_pause(void) {
    // "well-" → trailing '-', but '-' between word chars is a joiner not a trailing dash.
    // "so-" has trailing '-' with no char after: lastMeaningfulChar='-' → dashPause 60%
    // 200 + 120 = 320
    TEST_ASSERT_EQUAL(320u, duration(300, "so-", "the"));
}

void test_standalone_dash_pause(void) {
    // Parser-level hyphen splitting can produce "-" as its own displayed token.
    TEST_ASSERT_EQUAL(320u, duration(300, "-", "the"));
    TEST_ASSERT_EQUAL(320u, duration(300, "\u2014", "the"));
}

void test_em_dash_splits_words_and_preserves_hyphenated_compounds(void) {
    for (const auto line: {"competence\u2014enough well-known", "competence \u2014 enough well-known",
                           "competence&mdash;enough well-known"}) {
        std::vector<std::string> tokens;
        size_t count = 0;
        TEST_ASSERT_TRUE(RsvpText::appendLineWords(
            RsvpText::decodeMarkupEntities(line),
            [&](const std::string& token) {
                tokens.push_back(token);
                ++count;
                return true;
            },
            count, nullptr));
        const std::vector<std::string> expected{"competence", "\u2014", "enough", "well-known"};
        TEST_ASSERT_TRUE(tokens == expected);
    }
}

void test_ellipsis_pause(void) {
    // "and..." → ellipsis +110% → 200 + 220 = 420
    TEST_ASSERT_EQUAL(420u, duration(300, "and...", "then"));
}

// ---------------------------------------------------------------------------
// Duration: abbreviation suppresses sentence pause
// ---------------------------------------------------------------------------

void test_known_abbreviation_no_pause(void) {
    // "Mr." is in kKnownAbbreviations → no punctuation bonus
    // readable=2, syllable=1, no other bonus → 200
    TEST_ASSERT_EQUAL(200u, duration(300, "Mr.", "Smith"));
}

void test_dotted_initialism_no_pause(void) {
    // "U.S." → isDottedInitialism → no punctuation pause, but allCaps(+14%) + techConnector(+8%) = 22%
    // 200 + 44 = 244
    TEST_ASSERT_EQUAL(244u, duration(300, "U.S.", "The"));
}

void test_short_word_period_no_pause(void) {
    // "it." next "was" (lowercase) → readable=2 ≤ 4 and next starts lowercase → abbreviation → no pause
    // syllables: i(vowel,1), t. groups=1. No bonus.
    TEST_ASSERT_EQUAL(200u, duration(300, "it.", "was"));
}

void test_accented_lowercase_next_word_suppresses_sentence_pause(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\xC3\xA9lan"));
}

void test_extended_latin_lowercase_next_word_suppresses_sentence_pause(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\xC5\x93uvre"));
}

void test_extended_latin_uppercase_next_word_keeps_sentence_pause(void) {
    TEST_ASSERT_EQUAL(470u, duration(300, "done.", "\xC5\x92uvre"));
}

void test_baltic_lowercase_next_word_suppresses_sentence_pause(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\xC4\x81trums"));
}

void test_czech_lowercase_next_word_suppresses_sentence_pause(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\xC4\x9Bra"));
}

void test_cyrillic_case_controls_sentence_pause(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\xD1\x91\xD0\xBB\xD0\xBA\xD0\xB0"));
    TEST_ASSERT_EQUAL(470u, duration(300, "done.", "\xD0\x81\xD0\xBB\xD0\xBA\xD0\xB0"));
}

void test_sentence_pause_not_suppressed_for_long_word(void) {
    // "chapter." next "The" (uppercase) → readable=7 > 4, not a known abbreviation → sentence pause
    // length bonus: readable=7, tier1 extra=1 → 1*6=6%. syllable: c,h,a(1),p,t,e(2),r. lettersOnly="chapter"
    // ends with 'r' → no silent-e decrement. groups=2 ≤ 2, no syllable bonus.
    // total = 6% + 135% = 141% → 200 + 282 = 482
    TEST_ASSERT_EQUAL(482u, duration(300, "chapter.", "The"));
}

// ---------------------------------------------------------------------------
// Duration: length bonus
// ---------------------------------------------------------------------------

void test_long_word_length_bonus(void) {
    // "strength" → readable=8. tier1 extra=8-6=2 → 12%. No joiner. No tech connectors.
    // syllables: s,t,r,e(1),n,g,t,h. lettersOnly="strength", ends 'h'. groups=1 ≤2, no bonus.
    // complexity: 1 syllable, no digits, not allCaps (mix of upper/lower? no, "strength" is all lower).
    // uppercaseCount=0, digitCount=0, letterCount=8. No allCaps, no mixed.
    // techConnectorCount=0. No dense connector. Complexity=0.
    // No punctuation.
    // total = 12% → 200 + 24 = 224
    TEST_ASSERT_EQUAL(224u, duration(300, "strength", "and"));
}

void test_accented_latin_word_counts_as_readable(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "caf\xC3\xA9", "et"));
}

void test_extended_latin_word_counts_as_readable(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "\xC5\x82odz", "ma"));
}

void test_baltic_custom_vowel_affects_syllable_bonus(void) {
    TEST_ASSERT_EQUAL(220u, duration(300, "\xC4\x81kula", "ir"));
}

void test_czech_extended_word_counts_as_readable(void) {
    TEST_ASSERT_EQUAL(200u, duration(300, "b\xC4\x9Bh", "a"));
}

void test_hungarian_double_acute_vowel_affects_syllable_bonus(void) {
    TEST_ASSERT_EQUAL(220u, duration(300, "\xC5\x91voda", "van"));
}

void test_sami_custom_letter_counts_as_readable(void) {
    TEST_ASSERT_EQUAL(200u, duration(300,
                                     "\xC5\xA7"
                                     "ahti",
                                     "ja"));
}

void test_unicode_classification_covers_supported_scripts(void) {
    TEST_ASSERT_TRUE(UnicodeText::isLowercaseLetter(0x0153));
    TEST_ASSERT_TRUE(UnicodeText::isUppercaseLetter(0x0152));
    TEST_ASSERT_TRUE(UnicodeText::isVowel(0x0101));
    TEST_ASSERT_TRUE(UnicodeText::isLetter(0x0167));
    TEST_ASSERT_EQUAL_HEX32(0x0451, UnicodeText::toLowercase(0x0401));
    TEST_ASSERT_TRUE(UnicodeText::isVowel(0x0451));
    TEST_ASSERT_TRUE(UnicodeText::isLetter(0x65E5));
    TEST_ASSERT_TRUE(UnicodeText::isLetter(0x3042));
    TEST_ASSERT_TRUE(UnicodeText::isLetter(0x05D0));
    TEST_ASSERT_TRUE(UnicodeText::isLetter(0x0627));
    TEST_ASSERT_FALSE(UnicodeText::isLetter(0x060C));
    TEST_ASSERT_FALSE(UnicodeText::isLetter(0x064E));
    TEST_ASSERT_EQUAL_HEX32(UnicodeText::ScriptHan, UnicodeText::scriptMask(0x65E5));
    TEST_ASSERT_EQUAL_HEX32(UnicodeText::CapabilityBidi | UnicodeText::CapabilityShaping,
                            UnicodeText::capabilityMask(0x0627));
    TEST_ASSERT_EQUAL_HEX32(UnicodeText::CapabilityMathSymbols, UnicodeText::capabilityMask(0x2211));
    TEST_ASSERT_TRUE(UnicodeText::isUppercaseLetter(0x10400));
    TEST_ASSERT_TRUE(UnicodeText::isLowercaseLetter(0x10428));
    TEST_ASSERT_TRUE(UnicodeText::isDigit(0x0966));
}

void test_cjk_phrases_use_characters_per_minute(void) {
    ReadingSession r = makeReader(300, {"吾輩", "猫"});
    testSettings.pacing.punctuationDelayMs = 0;
    TEST_ASSERT_EQUAL(settings::ReadingPacing::cjkPhrase, ReadingLoop::pacingMode(r));
    TEST_ASSERT_EQUAL(400u, ReadingLoop::currentWordDurationMs(r, testSettings));

    r.state.overrides.pacing = settings::ReadingPacing::words;
    TEST_ASSERT_EQUAL(settings::ReadingPacing::words, ReadingLoop::pacingMode(r));
    TEST_ASSERT_EQUAL(200u, ReadingLoop::currentWordDurationMs(r, testSettings));
}

void test_cjk_tokenizer_emits_small_punctuation_aware_phrases(void) {
    std::vector<std::string> tokens;
    size_t count = 0;
    TEST_ASSERT_TRUE(RsvpText::appendLineWords(
        "吾輩は猫である。名前はまだ無い。",
        [&](const std::string& token) {
            tokens.push_back(token);
            ++count;
            return true;
        },
        count, nullptr));

    const std::vector<std::string> expected = {"吾輩", "は猫", "であ", "る。", "名前", "はま", "だ無", "い。"};
    TEST_ASSERT_TRUE(tokens == expected);

    ReadingSession r = makeReader(300, std::move(tokens));
    TEST_ASSERT_EQUAL_STRING("吾輩は猫である。名前はまだ無い。", ReadingLoop::paragraphAt(r, 0).text.c_str());
}

void test_very_long_word_extra_tier(void) {
    // "information" → readable=11. tier1 extra=5→30%, tier2 extra=1→9%. length=39%.
    // syllables: i(1),n,f,o(2),r,m,a(3),t,i(4),o(prev=vowel skip),n. groups=4.
    // syllableBonus: (4-2)*10=20%. No digits, no allCaps, no connectors.
    // total = 39+20 = 59% → 200 + 118 = 318
    TEST_ASSERT_EQUAL(318u, duration(300, "information", "is"));
}

// ---------------------------------------------------------------------------
// Duration: compound/technical word bonus
// ---------------------------------------------------------------------------

void test_compound_word_bonus(void) {
    // "well-known" → readable=9 (w,e,l,l,k,n,o,w,n). joinerCount=1 ('-' between 'l' and 'k').
    // tier1 extra=9-6=3 → 18%. joiner: +14%. readable<10, no longCompound.
    // techConnectorCount=1 == joinerCount → no extra tech bonus.
    // length bonus = min(170, 18+14) = 32%.
    // syllables: e(1) after w; '-' resets prev; o(2) after k. groups=2. ≤2, no bonus.
    // No allCaps, no dense connector. complexity=0%.
    // total = 32% → 200 + 64 = 264
    TEST_ASSERT_EQUAL(264u, duration(300, "well-known", "and"));
}

// ---------------------------------------------------------------------------
// Duration: all-caps complexity
// ---------------------------------------------------------------------------

void test_all_caps_complexity(void) {
    // "NASA" → uppercase=4, letters=4, uppercase==letters → allCaps +14%.
    // syllables: N,A(1),S,A(2). letterCount=4, lettersOnly="nasa" ends 'a'. groups=2. ≤2, no syllable bonus.
    // readable=4, no length bonus. No digits. No tech connectors.
    // complexity = 14%.
    // No punctuation.
    // total = 14% → 200 + 28 = 228
    TEST_ASSERT_EQUAL(228u, duration(300, "NASA", "sent"));
}

// ---------------------------------------------------------------------------
// Duration: pacing scale affects bonus magnitude
// ---------------------------------------------------------------------------

void test_punctuation_delay_halved(void) {
    // "done." next "The", punctuation delay halved to 100 ms.
    // sentencePause=135, scaled: (135*50)/100 = 67. total=67% → 200+134=334
    ReadingSession r = makeReader(300, {"done.", "The"});
    testSettings.pacing.punctuationDelayMs = 100;
    TEST_ASSERT_EQUAL(335u, ReadingLoop::currentWordDurationMs(r, testSettings));
}

void test_length_delay_quartered(void) {
    // Quartering the long-word delay produces the former 25% scale behavior.
    // "strength" length bonus=12%, scaled by 25 → (12*25)/100=3%.
    // total=3% → 200+6=206
    ReadingSession r = makeReader(300, {"strength", "and"});
    testSettings.pacing.longWordDelayMs = 50;
    TEST_ASSERT_EQUAL(206u, ReadingLoop::currentWordDurationMs(r, testSettings));
}

// ---------------------------------------------------------------------------
// Seek / scrub
// ---------------------------------------------------------------------------

void test_seek_to_sets_index_and_word(void) {
    ReadingSession r = makeReader(300, {"zero", "one", "two", "three", "four"});
    ReadingLoop::seekTo(r, 2);
    TEST_ASSERT_EQUAL(2u, r.state.wordIndex);
    TEST_ASSERT_EQUAL_STRING("two", r.currentWord.c_str());
}

void test_seek_to_clamps_at_end(void) {
    ReadingSession r = makeReader(300, {"a", "b", "c"});
    ReadingLoop::seekTo(r, 99);
    TEST_ASSERT_EQUAL(2u, r.state.wordIndex);
    TEST_ASSERT_EQUAL_STRING("c", r.currentWord.c_str());
}

void test_scrub_forward(void) {
    ReadingSession r = makeReader(300, {"zero", "one", "two", "three", "four"});
    ReadingLoop::seekTo(r, 1);
    ReadingLoop::seekRelative(r, r.state.wordIndex, 3);
    TEST_ASSERT_EQUAL(4u, r.state.wordIndex);
}

void test_scrub_backward(void) {
    ReadingSession r = makeReader(300, {"zero", "one", "two", "three", "four"});
    ReadingLoop::seekTo(r, 3);
    ReadingLoop::seekRelative(r, r.state.wordIndex, -2);
    TEST_ASSERT_EQUAL(1u, r.state.wordIndex);
}

void test_scrub_clamped_at_start(void) {
    ReadingSession r = makeReader(300, {"a", "b", "c"});
    ReadingLoop::seekTo(r, 1);
    ReadingLoop::seekRelative(r, r.state.wordIndex, -99);
    TEST_ASSERT_EQUAL(0u, r.state.wordIndex);
}

void test_scrub_clamped_at_end(void) {
    ReadingSession r = makeReader(300, {"a", "b", "c"});
    ReadingLoop::seekTo(r, 1);
    ReadingLoop::seekRelative(r, r.state.wordIndex, 99);
    TEST_ASSERT_EQUAL(2u, r.state.wordIndex);
}

void test_seek_relative_via_base_index(void) {
    ReadingSession r = makeReader(300, {"a", "b", "c", "d", "e"});
    // seekRelative from base=0 +3 → index 3
    ReadingLoop::seekRelative(r, 0, 3);
    TEST_ASSERT_EQUAL(3u, r.state.wordIndex);
    TEST_ASSERT_EQUAL_STRING("d", r.currentWord.c_str());
}

void test_seek_paragraph_moves_between_sections(void) {
    ReadingSession r = makeReader(300, {"zero", "one", "two", "three", "four", "five"});
    r.metadata.paragraphStarts = {0, 2, 5};
    ReadingLoop::seekTo(r, 3);
    TEST_ASSERT_TRUE(ReadingLoop::seekParagraph(r, 1));
    TEST_ASSERT_EQUAL(5u, r.state.wordIndex);
    TEST_ASSERT_TRUE(ReadingLoop::seekParagraph(r, -1));
    TEST_ASSERT_EQUAL(2u, r.state.wordIndex);
}

void test_seek_paragraph_clamps_at_book_edges(void) {
    ReadingSession r = makeReader(300, {"zero", "one", "two", "three"});
    r.metadata.paragraphStarts = {0, 2};
    TEST_ASSERT_FALSE(ReadingLoop::seekParagraph(r, -1));
    TEST_ASSERT_EQUAL(0u, r.state.wordIndex);
    TEST_ASSERT_TRUE(ReadingLoop::seekParagraph(r, 99));
    TEST_ASSERT_EQUAL(2u, r.state.wordIndex);
    TEST_ASSERT_FALSE(ReadingLoop::seekParagraph(r, 1));
}

void test_rewind_sentence_moves_to_current_sentence_start(void) {
    ReadingSession r = makeReader(300, {"One", "two.", "Three", "four", "five.", "Six"});
    ReadingLoop::seekTo(r, 3);
    ReadingLoop::rewindSentence(r);
    TEST_ASSERT_EQUAL(2u, r.state.wordIndex);
    TEST_ASSERT_EQUAL_STRING("Three", r.currentWord.c_str());
}

void test_rewind_sentence_at_sentence_start_moves_to_previous_sentence(void) {
    ReadingSession r = makeReader(300, {"One", "two.", "Three", "four", "five.", "Six"});
    ReadingLoop::seekTo(r, 2);
    ReadingLoop::rewindSentence(r);
    TEST_ASSERT_EQUAL(0u, r.state.wordIndex);
    TEST_ASSERT_EQUAL_STRING("One", r.currentWord.c_str());
}

void test_rewind_sentence_clamps_at_book_start(void) {
    ReadingSession r = makeReader(300, {"One", "two.", "Three"});
    ReadingLoop::seekTo(r, 0);
    ReadingLoop::rewindSentence(r);
    TEST_ASSERT_EQUAL(0u, r.state.wordIndex);
    TEST_ASSERT_EQUAL_STRING("One", r.currentWord.c_str());
}

void test_rewind_sentence_ignores_abbreviation_periods(void) {
    ReadingSession r = makeReader(300, {"Mr.", "Smith", "arrived.", "Then", "left."});
    ReadingLoop::seekTo(r, 4);
    ReadingLoop::rewindSentence(r);
    TEST_ASSERT_EQUAL(3u, r.state.wordIndex);
    TEST_ASSERT_EQUAL_STRING("Then", r.currentWord.c_str());
}

// ---------------------------------------------------------------------------
// Word count / word access
// ---------------------------------------------------------------------------

void test_word_at_returns_correct_word(void) {
    ReadingSession r = makeReader(300, {"alpha", "beta", "gamma"});
    TEST_ASSERT_TRUE(ReadingLoop::wordAt(r, 0) == "alpha");
    TEST_ASSERT_TRUE(ReadingLoop::wordAt(r, 1) == "beta");
    TEST_ASSERT_TRUE(ReadingLoop::wordAt(r, 2) == "gamma");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_wpm_base_interval);
    RUN_TEST(test_cjk_phrases_use_characters_per_minute);
    RUN_TEST(test_cjk_tokenizer_emits_small_punctuation_aware_phrases);
    RUN_TEST(test_wpm_clamped_low);
    RUN_TEST(test_wpm_clamped_high);
    RUN_TEST(test_adjust_wpm_steps_by_25);
    RUN_TEST(test_adjust_wpm_steps_by_10_below_100);
    RUN_TEST(test_adjust_wpm_crosses_100_cleanly);
    RUN_TEST(test_adjust_wpm_clamped_at_bounds);

    RUN_TEST(test_short_word_no_bonus);

    RUN_TEST(test_comma_pause);
    RUN_TEST(test_sentence_pause);
    RUN_TEST(test_strong_sentence_pause);
    RUN_TEST(test_sentence_pause_preserved_with_closing_quote);
    RUN_TEST(test_sentence_pause_preserved_with_closing_parenthesis);
    RUN_TEST(test_clause_pause_semicolon);
    RUN_TEST(test_dash_pause);
    RUN_TEST(test_standalone_dash_pause);
    RUN_TEST(test_em_dash_splits_words_and_preserves_hyphenated_compounds);
    RUN_TEST(test_ellipsis_pause);

    RUN_TEST(test_known_abbreviation_no_pause);
    RUN_TEST(test_dotted_initialism_no_pause);
    RUN_TEST(test_short_word_period_no_pause);
    RUN_TEST(test_accented_lowercase_next_word_suppresses_sentence_pause);
    RUN_TEST(test_extended_latin_lowercase_next_word_suppresses_sentence_pause);
    RUN_TEST(test_extended_latin_uppercase_next_word_keeps_sentence_pause);
    RUN_TEST(test_baltic_lowercase_next_word_suppresses_sentence_pause);
    RUN_TEST(test_czech_lowercase_next_word_suppresses_sentence_pause);
    RUN_TEST(test_cyrillic_case_controls_sentence_pause);
    RUN_TEST(test_sentence_pause_not_suppressed_for_long_word);

    RUN_TEST(test_long_word_length_bonus);
    RUN_TEST(test_accented_latin_word_counts_as_readable);
    RUN_TEST(test_extended_latin_word_counts_as_readable);
    RUN_TEST(test_baltic_custom_vowel_affects_syllable_bonus);
    RUN_TEST(test_czech_extended_word_counts_as_readable);
    RUN_TEST(test_hungarian_double_acute_vowel_affects_syllable_bonus);
    RUN_TEST(test_sami_custom_letter_counts_as_readable);
    RUN_TEST(test_unicode_classification_covers_supported_scripts);
    RUN_TEST(test_very_long_word_extra_tier);
    RUN_TEST(test_compound_word_bonus);
    RUN_TEST(test_all_caps_complexity);

    RUN_TEST(test_punctuation_delay_halved);
    RUN_TEST(test_length_delay_quartered);

    RUN_TEST(test_seek_to_sets_index_and_word);
    RUN_TEST(test_seek_to_clamps_at_end);
    RUN_TEST(test_scrub_forward);
    RUN_TEST(test_scrub_backward);
    RUN_TEST(test_scrub_clamped_at_start);
    RUN_TEST(test_scrub_clamped_at_end);
    RUN_TEST(test_seek_relative_via_base_index);
    RUN_TEST(test_seek_paragraph_moves_between_sections);
    RUN_TEST(test_seek_paragraph_clamps_at_book_edges);
    RUN_TEST(test_rewind_sentence_moves_to_current_sentence_start);
    RUN_TEST(test_rewind_sentence_at_sentence_start_moves_to_previous_sentence);
    RUN_TEST(test_rewind_sentence_clamps_at_book_start);
    RUN_TEST(test_rewind_sentence_ignores_abbreviation_periods);

    RUN_TEST(test_word_at_returns_correct_word);

    return UNITY_END();
}
