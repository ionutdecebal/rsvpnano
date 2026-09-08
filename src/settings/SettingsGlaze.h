#pragma once

#include <glaze/core/custom.hpp>
#include <glaze/core/meta.hpp>

#include "settings/SettingsModel.h"

template<std::integral T, T Minimum, T Maximum, T Step>
struct glz::meta<settings::BoundedValue<T, Minimum, Maximum, Step>> {
    using Bounded = settings::BoundedValue<T, Minimum, Maximum, Step>;
    static constexpr auto read = [](Bounded& output, T input) {
        output = input;
    };
    static constexpr auto write = [](const Bounded& input) {
        return static_cast<T>(input);
    };
    static constexpr auto value = glz::custom<read, write>;
};

template<>
struct glz::meta<settings::ReadingMode> {
    using enum settings::ReadingMode;
    static constexpr auto value = glz::enumerate(rsvp, page);
};

template<>
struct glz::meta<settings::PauseMode> {
    using enum settings::PauseMode;
    static constexpr auto value = glz::enumerate(sentenceEnd, instant);
};

template<>
struct glz::meta<settings::FooterMetric> {
    using enum settings::FooterMetric;
    static constexpr auto value = glz::enumerate(percentage, chapterTime, bookTime);
};

template<>
struct glz::meta<settings::BatteryLabel> {
    using enum settings::BatteryLabel;
    static constexpr auto value = glz::enumerate(percentage, timeRemaining, voltage);
};

template<>
struct glz::meta<settings::LibraryLayout> {
    using enum settings::LibraryLayout;
    static constexpr auto value = glz::enumerate(shelf, list);
};

template<>
struct glz::meta<settings::ReadingPacing> {
    using enum settings::ReadingPacing;
    static constexpr auto value = glz::enumerate("words", words, "cjk-phrase", cjkPhrase);
};

template<>
struct glz::meta<standby::Kind> {
    using enum standby::Kind;
    static constexpr auto value = glz::enumerate(life, maze, voronoi, screenOff, reaction);
};
