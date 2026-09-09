#pragma once

#include <unity.h>
#include "ui/screens/ReaderAppearanceLayout.h"
#include "ui/screens/ReaderLayout.h"

namespace appearanceChecks {
    inline void layout(int16_t width, int16_t height) {
        const auto layout = screens::appearanceLayout::make(width, height);
        const auto inside = [&](ui::Rect rect) {
            TEST_ASSERT_TRUE(rect.w > 0 && rect.h > 0 && rect.x >= 0 && rect.y >= 0);
            TEST_ASSERT_TRUE(rect.x + rect.w <= width && rect.y + rect.h <= height);
        };
        for (const auto rect: {layout.back, layout.page, layout.reset, layout.font, layout.size, layout.preview})
            inside(rect);
        for (const auto rect: layout.dials) {
            inside(rect);
            if (!layout.pagedDials)
                TEST_ASSERT_TRUE(rect.x >= layout.preview.x + layout.preview.w || rect.y + rect.h <= layout.preview.y
                                 || rect.y >= layout.preview.y + layout.preview.h);
        }
        for (const bool leftHanded: {false, true}) {
            const auto chrome = screens::readerLayout::horizontalChrome(width, height, leftHanded);
            for (const auto rect:
                 {chrome.chapter, chrome.progress, chrome.batteryIcon, chrome.batteryLabel, chrome.arrows})
                inside(rect);
        }
    }

    inline ui::TouchContact contact;
    inline ui::TouchSampleResult poll(ui::TouchContact& out) {
        out = contact;
        return ui::TouchSampleResult::Contact;
    }

    inline void fourRotaries() {
        Arduino_GFX gfx(320, 172);
        ui::Context ui(gfx);
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        ui.setTouchSource({.surface = {320, 172}, .poll = &poll});
        constexpr std::array<ui::Rect, 4> rects{
            {{0, 0, 100, 82}, {104, 0, 100, 82}, {0, 86, 100, 82}, {104, 86, 100, 82}}};
        uint32_t now = 100;
        std::array<int, 4> values{20, 30, 40, 50};
        const auto frame = [&](ui::TouchContact sample) {
            contact = sample;
            ui.pollTouch(now += 20);
            ui.beginFrame(1);
            for (int i = 0; i < 4; ++i)
                ui.rotary(rects[i], values[i], 0, 100, 1, "Width");
            ui.endFrame();
        };
        for (int selected = 0; selected < 4; ++selected) {
            const auto before = values;
            const auto rect = rects[selected];
            const uint16_t x = rect.x + rect.w / 2, y = rect.y + rect.h / 2;
            frame({true, x, y});
            frame({true, static_cast<uint16_t>(x + 32), y});
            frame({false, static_cast<uint16_t>(x + 32), y});
            for (int i = 0; i < 4; ++i)
                TEST_ASSERT_EQUAL(before[i] + (i == selected ? 4 : 0), values[i]);
        }
    }

    inline void wordTargets() {
        const auto first = screens::appearanceLayout::wordTarget(100, 112, 86, 53, 640, 172);
        const auto shifted = screens::appearanceLayout::wordTarget(164, 112, 86, 53, 640, 172);
        TEST_ASSERT_EQUAL(64, shifted.x - first.x);
        TEST_ASSERT_EQUAL(first.y, shifted.y);
        TEST_ASSERT_TRUE(first.y >= 44 && first.y + first.h <= 128);
        const auto clipped = screens::appearanceLayout::wordTarget(-30, 80, 86, 53, 640, 172);
        TEST_ASSERT_EQUAL(0, clipped.x);
        const auto small = screens::appearanceLayout::wordTarget(120, 22, 86, 15, 640, 172);
        TEST_ASSERT_TRUE(small.w >= 44 && small.h >= 44);
    }
} // namespace appearanceChecks
