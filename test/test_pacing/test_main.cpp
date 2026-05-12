#include <unity.h>

#include "reader/ReadingLoop.h"
#include "text/LatinText.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ReadingLoop makeReader(uint16_t wpm, std::vector<String> words) {
  ReadingLoop r;
  r.setWpm(wpm);
  r.setWords(std::move(words), 0);
  return r;
}

// Duration of the first word when the second word is the contextual next.
static uint32_t duration(uint16_t wpm, const char *word, const char *next) {
  ReadingLoop r = makeReader(wpm, {String(word), String(next)});
  return r.currentWordDurationMs();
}

// ---------------------------------------------------------------------------
// WPM / interval
// ---------------------------------------------------------------------------

void test_wpm_base_interval(void) {
  ReadingLoop r;
  r.setWpm(300);
  TEST_ASSERT_EQUAL(200u, r.wordIntervalMs());  // 60000 / 300

  r.setWpm(600);
  TEST_ASSERT_EQUAL(100u, r.wordIntervalMs());
}

void test_wpm_clamped_low(void) {
  ReadingLoop r;
  r.setWpm(50);
  TEST_ASSERT_EQUAL(100u, r.wpm());
}

void test_wpm_clamped_high(void) {
  ReadingLoop r;
  r.setWpm(9999);
  TEST_ASSERT_EQUAL(1000u, r.wpm());
}

void test_adjust_wpm_steps_by_25(void) {
  ReadingLoop r;
  r.setWpm(300);
  r.adjustWpm(1);
  TEST_ASSERT_EQUAL(325u, r.wpm());
  r.adjustWpm(-1);
  TEST_ASSERT_EQUAL(300u, r.wpm());
}

void test_adjust_wpm_clamped_at_bounds(void) {
  ReadingLoop r;
  r.setWpm(1000);
  r.adjustWpm(1);
  TEST_ASSERT_EQUAL(1000u, r.wpm());

  r.setWpm(100);
  r.adjustWpm(-1);
  TEST_ASSERT_EQUAL(100u, r.wpm());
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
  // "done." next "e-acute-lan" (stored as Latin-1 \xE9 after normalization) should suppress sentence pause.
  TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\xE9lan"));
}

void test_extended_latin_lowercase_next_word_suppresses_sentence_pause(void) {
  // "done." next "oe-ligature-uvre" (stored in the custom slot map as \x81) should also suppress
  // a false sentence pause.
  TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\x81uvre"));
}

void test_extended_latin_uppercase_next_word_keeps_sentence_pause(void) {
  // "done." next "OE-ligature-uvre" (custom slot \x80) should still read as a sentence break.
  TEST_ASSERT_EQUAL(470u, duration(300, "done.", "\x80uvre"));
}

void test_baltic_lowercase_next_word_suppresses_sentence_pause(void) {
  // "done." next "a-macron-trums" (custom slot \xA2) should also count as a lowercase start.
  TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\xA2trums"));
}

void test_czech_lowercase_next_word_suppresses_sentence_pause(void) {
  // "done." next "e-caron-ra" (custom slot \x04) should also count as a lowercase start.
  TEST_ASSERT_EQUAL(200u, duration(300, "done.", "\x04""ra"));
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
  TEST_ASSERT_EQUAL(200u, duration(300, "caf\xE9", "et"));
}

void test_extended_latin_word_counts_as_readable(void) {
  TEST_ASSERT_EQUAL(200u, duration(300, "\x83""odz", "ma"));
}

void test_baltic_custom_vowel_affects_syllable_bonus(void) {
  // "a-macron-kula" has three vowel groups (a-macron, u, a) and should pick up a 10% complexity bonus.
  TEST_ASSERT_EQUAL(220u, duration(300, "\xA2kula", "ir"));
}

void test_czech_extended_word_counts_as_readable(void) {
  TEST_ASSERT_EQUAL(200u, duration(300, "b\x04h", "a"));
}

void test_hungarian_double_acute_vowel_affects_syllable_bonus(void) {
  // "o-double-acute-voda" has three vowel groups (o-double-acute, o, a) and should pick up 10%.
  TEST_ASSERT_EQUAL(220u, duration(300, "\x13voda", "van"));
}

void test_sami_custom_letter_counts_as_readable(void) {
  TEST_ASSERT_EQUAL(200u, duration(300, "\xF7""ahti", "ja"));
}

void test_ascii_fallback_maps_accented_latin_to_base_letter(void) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>('e'), LatinText::fallbackAsciiByte(0xE9));
}

void test_ascii_fallback_maps_hungarian_double_acute_to_base_letter(void) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>('o'), LatinText::fallbackAsciiByte(0x13));
}

void test_ascii_fallback_maps_spanish_enye_to_base_letter(void) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>('n'), LatinText::fallbackAsciiByte(0xF1));
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

void test_punctuation_scale_halved(void) {
  // "done." next "The", punctuationScale=50
  // sentencePause=135, scaled: (135*50)/100 = 67. total=67% → 200+134=334
  ReadingLoop r = makeReader(300, {"done.", "The"});
  ReadingLoop::PacingConfig cfg;
  cfg.longWordScalePercent = 100;
  cfg.complexWordScalePercent = 100;
  cfg.punctuationScalePercent = 50;
  r.setPacingConfig(cfg);
  TEST_ASSERT_EQUAL(334u, r.currentWordDurationMs());
}

void test_length_scale_zero_equivalent(void) {
  // scale clamped at 25 minimum, so longWordScale=0 → treated as 25
  // "strength" length bonus=12%, scaled by 25 → (12*25)/100=3%.
  // total=3% → 200+6=206
  ReadingLoop r = makeReader(300, {"strength", "and"});
  ReadingLoop::PacingConfig cfg;
  cfg.longWordScalePercent = 0;
  cfg.complexWordScalePercent = 100;
  cfg.punctuationScalePercent = 100;
  r.setPacingConfig(cfg);
  TEST_ASSERT_EQUAL(206u, r.currentWordDurationMs());
}

// ---------------------------------------------------------------------------
// Seek / scrub
// ---------------------------------------------------------------------------

void test_seek_to_sets_index_and_word(void) {
  ReadingLoop r = makeReader(300, {"zero", "one", "two", "three", "four"});
  r.seekTo(2);
  TEST_ASSERT_EQUAL(2u, r.currentIndex());
  TEST_ASSERT_EQUAL_STRING("two", r.currentWord().c_str());
}

void test_seek_to_clamps_at_end(void) {
  ReadingLoop r = makeReader(300, {"a", "b", "c"});
  r.seekTo(99);
  TEST_ASSERT_EQUAL(2u, r.currentIndex());
  TEST_ASSERT_EQUAL_STRING("c", r.currentWord().c_str());
}

void test_scrub_forward(void) {
  ReadingLoop r = makeReader(300, {"zero", "one", "two", "three", "four"});
  r.seekTo(1);
  r.scrub(3);
  TEST_ASSERT_EQUAL(4u, r.currentIndex());
}

void test_scrub_backward(void) {
  ReadingLoop r = makeReader(300, {"zero", "one", "two", "three", "four"});
  r.seekTo(3);
  r.scrub(-2);
  TEST_ASSERT_EQUAL(1u, r.currentIndex());
}

void test_scrub_clamped_at_start(void) {
  ReadingLoop r = makeReader(300, {"a", "b", "c"});
  r.seekTo(1);
  r.scrub(-99);
  TEST_ASSERT_EQUAL(0u, r.currentIndex());
}

void test_scrub_clamped_at_end(void) {
  ReadingLoop r = makeReader(300, {"a", "b", "c"});
  r.seekTo(1);
  r.scrub(99);
  TEST_ASSERT_EQUAL(2u, r.currentIndex());
}

void test_seek_relative_via_base_index(void) {
  ReadingLoop r = makeReader(300, {"a", "b", "c", "d", "e"});
  // seekRelative from base=0 +3 → index 3
  r.seekRelative(0, 3);
  TEST_ASSERT_EQUAL(3u, r.currentIndex());
  TEST_ASSERT_EQUAL_STRING("d", r.currentWord().c_str());
}

void test_rewind_sentence_moves_to_current_sentence_start(void) {
  ReadingLoop r = makeReader(300, {"One", "two.", "Three", "four", "five.", "Six"});
  r.seekTo(3);
  r.rewindSentence();
  TEST_ASSERT_EQUAL(2u, r.currentIndex());
  TEST_ASSERT_EQUAL_STRING("Three", r.currentWord().c_str());
}

void test_rewind_sentence_at_sentence_start_moves_to_previous_sentence(void) {
  ReadingLoop r = makeReader(300, {"One", "two.", "Three", "four", "five.", "Six"});
  r.seekTo(2);
  r.rewindSentence();
  TEST_ASSERT_EQUAL(0u, r.currentIndex());
  TEST_ASSERT_EQUAL_STRING("One", r.currentWord().c_str());
}

void test_rewind_sentence_clamps_at_book_start(void) {
  ReadingLoop r = makeReader(300, {"One", "two.", "Three"});
  r.seekTo(0);
  r.rewindSentence();
  TEST_ASSERT_EQUAL(0u, r.currentIndex());
  TEST_ASSERT_EQUAL_STRING("One", r.currentWord().c_str());
}

void test_rewind_sentence_ignores_abbreviation_periods(void) {
  ReadingLoop r = makeReader(300, {"Mr.", "Smith", "arrived.", "Then", "left."});
  r.seekTo(4);
  r.rewindSentence();
  TEST_ASSERT_EQUAL(3u, r.currentIndex());
  TEST_ASSERT_EQUAL_STRING("Then", r.currentWord().c_str());
}

// ---------------------------------------------------------------------------
// Word count / word access
// ---------------------------------------------------------------------------

void test_word_at_returns_correct_word(void) {
  ReadingLoop r = makeReader(300, {"alpha", "beta", "gamma"});
  TEST_ASSERT_EQUAL_STRING("alpha", r.wordAt(0).c_str());
  TEST_ASSERT_EQUAL_STRING("beta", r.wordAt(1).c_str());
  TEST_ASSERT_EQUAL_STRING("gamma", r.wordAt(2).c_str());
}

// ---------------------------------------------------------------------------
// wordPacingBonusMsAt: WPM-independent per-word bonus used by the time-estimate cache
// ---------------------------------------------------------------------------

void test_word_pacing_bonus_at_sum_matches_expected(void) {
  // Phrase mixes a comma pause, a sentence pause, and a long word so the bonus sum is non-zero.
  ReadingLoop r = makeReader(300, {"This", "is,", "honestly,", "information.", "Then"});

  uint32_t bonusSum = 0;
  for (size_t i = 0; i < 5; ++i) {
    bonusSum += r.wordPacingBonusMsAt(i);
  }

  // Per-word bonuses (base interval is 200 ms at 300 WPM):
  //   "This"          → 0
  //   "is,"           → comma(45% of 200) = 90
  //   "honestly,"     → length 12% + complexity 10% + comma 45% = 67% of 200 = 134.
  //   "information."  → length 39% + complexity 20% + sentence 135% (next "Then" uppercase) = 194%
  //                     of 200 = 388.
  //   "Then"          → 0.
  // Expected = 0 + 90 + 134 + 388 + 0 = 612.
  TEST_ASSERT_EQUAL_UINT32(612u, bonusSum);
}

void test_word_pacing_bonus_at_is_invariant_to_wpm(void) {
  // The pacing bonus is the WPM-independent part of the per-word duration. Changing WPM must not
  // change the bonus, which lets us cache it once per book.
  ReadingLoop r = makeReader(300, {"This", "is,", "honestly,", "information.", "Then"});
  uint32_t baselineBonuses[5];
  for (size_t i = 0; i < 5; ++i) {
    baselineBonuses[i] = r.wordPacingBonusMsAt(i);
  }

  r.setWpm(600);
  for (size_t i = 0; i < 5; ++i) {
    TEST_ASSERT_EQUAL_UINT32(baselineBonuses[i], r.wordPacingBonusMsAt(i));
  }

  r.setWpm(150);
  for (size_t i = 0; i < 5; ++i) {
    TEST_ASSERT_EQUAL_UINT32(baselineBonuses[i], r.wordPacingBonusMsAt(i));
  }
}

void test_word_pacing_bonus_plus_interval_equals_current_duration(void) {
  // Sanity check that base + bonus reconstructs the runtime per-word duration.
  ReadingLoop r = makeReader(300, {"This", "is,", "fine.", "Then"});
  for (size_t i = 0; i < 4; ++i) {
    r.seekTo(i);
    TEST_ASSERT_EQUAL_UINT32(r.wordIntervalMs() + r.wordPacingBonusMsAt(i),
                             r.currentWordDurationMs());
  }
}

void test_word_pacing_bonus_at_out_of_range_returns_zero(void) {
  ReadingLoop r = makeReader(300, {"a", "b"});
  TEST_ASSERT_EQUAL_UINT32(0u, r.wordPacingBonusMsAt(2));
  TEST_ASSERT_EQUAL_UINT32(0u, r.wordPacingBonusMsAt(99));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_wpm_base_interval);
  RUN_TEST(test_wpm_clamped_low);
  RUN_TEST(test_wpm_clamped_high);
  RUN_TEST(test_adjust_wpm_steps_by_25);
  RUN_TEST(test_adjust_wpm_clamped_at_bounds);

  RUN_TEST(test_short_word_no_bonus);

  RUN_TEST(test_comma_pause);
  RUN_TEST(test_sentence_pause);
  RUN_TEST(test_strong_sentence_pause);
  RUN_TEST(test_sentence_pause_preserved_with_closing_quote);
  RUN_TEST(test_sentence_pause_preserved_with_closing_parenthesis);
  RUN_TEST(test_clause_pause_semicolon);
  RUN_TEST(test_dash_pause);
  RUN_TEST(test_ellipsis_pause);

  RUN_TEST(test_known_abbreviation_no_pause);
  RUN_TEST(test_dotted_initialism_no_pause);
  RUN_TEST(test_short_word_period_no_pause);
  RUN_TEST(test_accented_lowercase_next_word_suppresses_sentence_pause);
  RUN_TEST(test_extended_latin_lowercase_next_word_suppresses_sentence_pause);
  RUN_TEST(test_extended_latin_uppercase_next_word_keeps_sentence_pause);
  RUN_TEST(test_baltic_lowercase_next_word_suppresses_sentence_pause);
  RUN_TEST(test_czech_lowercase_next_word_suppresses_sentence_pause);
  RUN_TEST(test_sentence_pause_not_suppressed_for_long_word);

  RUN_TEST(test_long_word_length_bonus);
  RUN_TEST(test_accented_latin_word_counts_as_readable);
  RUN_TEST(test_extended_latin_word_counts_as_readable);
  RUN_TEST(test_baltic_custom_vowel_affects_syllable_bonus);
  RUN_TEST(test_czech_extended_word_counts_as_readable);
  RUN_TEST(test_hungarian_double_acute_vowel_affects_syllable_bonus);
  RUN_TEST(test_sami_custom_letter_counts_as_readable);
  RUN_TEST(test_ascii_fallback_maps_accented_latin_to_base_letter);
  RUN_TEST(test_ascii_fallback_maps_hungarian_double_acute_to_base_letter);
  RUN_TEST(test_ascii_fallback_maps_spanish_enye_to_base_letter);
  RUN_TEST(test_very_long_word_extra_tier);
  RUN_TEST(test_compound_word_bonus);
  RUN_TEST(test_all_caps_complexity);

  RUN_TEST(test_punctuation_scale_halved);
  RUN_TEST(test_length_scale_zero_equivalent);

  RUN_TEST(test_seek_to_sets_index_and_word);
  RUN_TEST(test_seek_to_clamps_at_end);
  RUN_TEST(test_scrub_forward);
  RUN_TEST(test_scrub_backward);
  RUN_TEST(test_scrub_clamped_at_start);
  RUN_TEST(test_scrub_clamped_at_end);
  RUN_TEST(test_seek_relative_via_base_index);
  RUN_TEST(test_rewind_sentence_moves_to_current_sentence_start);
  RUN_TEST(test_rewind_sentence_at_sentence_start_moves_to_previous_sentence);
  RUN_TEST(test_rewind_sentence_clamps_at_book_start);
  RUN_TEST(test_rewind_sentence_ignores_abbreviation_periods);

  RUN_TEST(test_word_at_returns_correct_word);

  RUN_TEST(test_word_pacing_bonus_at_sum_matches_expected);
  RUN_TEST(test_word_pacing_bonus_at_is_invariant_to_wpm);
  RUN_TEST(test_word_pacing_bonus_plus_interval_equals_current_duration);
  RUN_TEST(test_word_pacing_bonus_at_out_of_range_returns_zero);

  return UNITY_END();
}
