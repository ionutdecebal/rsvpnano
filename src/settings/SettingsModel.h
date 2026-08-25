#pragma once

#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "settings/SettingsRules.h"
#include "screensavers/ScreensaverTypes.h"
#include "text/UnicodeText.h"
namespace settings {

    inline constexpr std::string_view kMathFontTarget = "math";
    inline constexpr std::string_view kDefaultRepositoryOwner = "ionutdecebal";

    enum class ReadingMode : uint8_t {
        rsvp,
        page,
        Count,
    };

    // Persisted enum spellings are their TOML/JSON names.
    enum class PauseMode : uint8_t {
        sentenceEnd,
        instant,
        Count,
    };

    enum class FooterMetric : uint8_t {
        percentage,
        chapterTime,
        bookTime,
        Count,
    };

    enum class BatteryLabel : uint8_t {
        percentage,
        timeRemaining,
        voltage,
        Count,
    };

    struct TypographySettings {
        std::string fontId = "literata";
        BoundedValue<uint8_t, 0, 3> fontSizeIndex{0};
        bool focusHighlight = true;
        BoundedValue<int, -2, 3> tracking{0};
        BoundedValue<uint8_t, 30, 40> anchor{30};
        BoundedValue<uint8_t, 12, 30, 2> guideWidth{30};
        BoundedValue<uint8_t, 2, 8> guideGap{5};

        bool operator==(const TypographySettings&) const = default;
    };

    enum class ReadingPacing : uint8_t {
        words,
        cjkPhrase,
    };

    struct LanguageFont {
        std::string locale;
        std::string fontId;

        bool operator==(const LanguageFont&) const = default;
    };

    struct ReadingOverrides {
        std::vector<LanguageFont> languageFonts;
        std::optional<std::string> locale;
        std::optional<ReadingPacing> pacing;

        bool operator==(const ReadingOverrides&) const = default;
    };

    inline std::string_view fontForText(const ReadingOverrides& overrides, std::string_view locale, uint32_t scripts,
                                        std::string_view fallback) {
        const std::string_view target = (scripts & UnicodeText::ScriptMath) != 0 ? kMathFontTarget : locale;
        const auto selected = std::ranges::find(overrides.languageFonts, target, &LanguageFont::locale);
        return selected == overrides.languageFonts.end() ? fallback : std::string_view{selected->fontId};
    }

    struct PacingSettings {
        BoundedValue<uint16_t, 0, 600, 50> longWordDelayMs{200};
        BoundedValue<uint16_t, 0, 600, 50> complexWordDelayMs{200};
        BoundedValue<uint16_t, 0, 600, 50> punctuationDelayMs{200};

        bool operator==(const PacingSettings&) const = default;
    };

    struct ReadingSettings {
        BoundedValue<uint16_t, 10, 1000, 10> wpm{300};
        ReadingMode mode = ReadingMode::rsvp;
        PauseMode pauseMode = PauseMode::sentenceEnd;
        bool phantomWords = true;
        bool chapterScrollReversed = false;
        FooterMetric footerMetric = FooterMetric::percentage;
        BatteryLabel batteryLabel = BatteryLabel::percentage;
        bool batteryIconVisible = true;
        bool batteryVisibleWhileReading = true;
        bool chapterVisibleWhileReading = false;
        bool progressVisibleWhileReading = false;
        bool leftHanded = false;
        TypographySettings typography;
        PacingSettings pacing;

        bool operator==(const ReadingSettings&) const = default;
    };

    struct InterfaceSettings {
        BoundedValue<uint8_t, 5, 100, 5> brightnessPercent{70};
        std::string locale = "en";
        BoundedValue<uint8_t, 0, 4> standbyTimerIndex{1};
        standby::Kind screensaver = standby::Kind::life;
        std::string selectedThemeId = "default";

        bool operator==(const InterfaceSettings&) const = default;
    };

    struct NetworkSettings {
        std::string ssid;

        bool operator==(const NetworkSettings&) const = default;
    };

    struct UpdateSettings {
        bool checkOnStartup = false;
        std::string repositoryOwner;
        std::string releaseTag;

        bool operator==(const UpdateSettings&) const = default;
    };

    struct DeviceSettings {
        ReadingSettings reading;
        InterfaceSettings interface;
        NetworkSettings network;
        UpdateSettings updates;

        bool operator==(const DeviceSettings&) const = default;
    };

    struct DeviceSecrets {
        std::string wifiPassword;

        bool operator==(const DeviceSecrets&) const = default;
    };

} // namespace settings
