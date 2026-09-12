#include "ui/Ui.h"

#include <algorithm>

namespace ui {
    void Context::drawIcon(Arduino_GFX& output, Rect rect, Icon icon, uint16_t ink, uint16_t surface) {
        switch (icon) {
        case Icon::Bookmark:
            drawBookmarkIcon(output, rect, ink, surface);
            break;
        case Icon::Books:
            drawBooksIcon(output, rect, ink);
            break;
        case Icon::Edit:
            drawEditIcon(output, rect, ink);
            break;
        case Icon::Device:
            drawDeviceIcon(output, rect, ink);
            break;
        case Icon::Language:
            drawLanguageIcon(output, rect, ink);
            break;
        case Icon::Hourglass:
            drawHourglassIcon(output, rect, ink);
            break;
        case Icon::Power:
            drawPowerIcon(output, rect, ink, surface);
            break;
        default:
            break;
        }
    }

    void Context::drawBookmarkIcon(Arduino_GFX& output, Rect rect, uint16_t ink, uint16_t surface) {
        const int16_t cx = static_cast<int16_t>(rect.x + rect.w / 2);
        const int16_t width = std::max<int16_t>(1, std::min<int16_t>(13, static_cast<int16_t>(rect.w - 6)));
        const int16_t height = std::max<int16_t>(1, std::min<int16_t>(30, static_cast<int16_t>(rect.h - 4)));
        const int16_t x = static_cast<int16_t>(cx - width / 2);
        const int16_t y = static_cast<int16_t>(rect.y + 1);
        output.fillRect(x, y, width, height, ink);
        for (int16_t row = 0; row <= std::min<int16_t>(6, height - 1); ++row) {
            const int16_t half = std::min<int16_t>(row, width / 2);
            output.drawFastHLine(static_cast<int16_t>(cx - half), static_cast<int16_t>(y + height - 7 + row),
                                 static_cast<int16_t>(half * 2 + 1), surface);
        }
    }

    void Context::drawBooksIcon(Arduino_GFX& output, Rect rect, uint16_t ink) {
        const int16_t x = static_cast<int16_t>(rect.x + rect.w / 2 - 9);
        const int16_t y = static_cast<int16_t>(rect.y + rect.h / 2 - 9);
        output.drawRect(x, y, 5, 18, ink);
        output.drawRect(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 2), 5, 16, ink);
        output.drawRect(static_cast<int16_t>(x + 12), static_cast<int16_t>(y - 1), 6, 19, ink);
    }

    void Context::drawEditIcon(Arduino_GFX& output, Rect rect, uint16_t ink) {
        const int16_t x = static_cast<int16_t>(rect.x + rect.w / 2 - 9);
        const int16_t y = static_cast<int16_t>(rect.y + rect.h / 2 - 9);
        output.drawRect(x, y, 14, 18, ink);
        output.drawLine(static_cast<int16_t>(x + 5), static_cast<int16_t>(y + 13), static_cast<int16_t>(x + 18), y,
                        ink);
        output.drawLine(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 16), static_cast<int16_t>(x + 19),
                        static_cast<int16_t>(y + 3), ink);
    }

    void Context::drawDeviceIcon(Arduino_GFX& output, Rect rect, uint16_t ink) {
        const int16_t cx = static_cast<int16_t>(rect.x + rect.w / 2);
        const int16_t x = static_cast<int16_t>(cx - 8);
        const int16_t y = static_cast<int16_t>(rect.y + rect.h / 2 - 10);
        output.drawRoundRect(x, y, 16, 20, 3, ink);
        output.fillRect(static_cast<int16_t>(cx - 2), static_cast<int16_t>(y + 16), 4, 1, ink);
    }

    void Context::drawLanguageIcon(Arduino_GFX& output, Rect rect, uint16_t ink) {
        const int16_t cx = static_cast<int16_t>(rect.x + rect.w / 2);
        const int16_t cy = static_cast<int16_t>(rect.y + rect.h / 2);
        const int16_t left = static_cast<int16_t>(cx - 10);
        const int16_t top = static_cast<int16_t>(cy - 8);
        output.drawRoundRect(left, top, 13, 13, 2, ink);
        output.drawRoundRect(static_cast<int16_t>(left + 7), static_cast<int16_t>(top + 5), 13, 13, 2, ink);
        output.drawLine(static_cast<int16_t>(left + 3), static_cast<int16_t>(top + 10), static_cast<int16_t>(left + 6),
                        static_cast<int16_t>(top + 3), ink);
        output.drawLine(static_cast<int16_t>(left + 6), static_cast<int16_t>(top + 3), static_cast<int16_t>(left + 9),
                        static_cast<int16_t>(top + 10), ink);
        output.drawFastHLine(static_cast<int16_t>(left + 4), static_cast<int16_t>(top + 7), 5, ink);
        output.drawFastHLine(static_cast<int16_t>(left + 10), static_cast<int16_t>(top + 10), 7, ink);
        output.drawFastVLine(static_cast<int16_t>(left + 13), static_cast<int16_t>(top + 8), 7, ink);
    }

    void Context::drawHourglassIcon(Arduino_GFX& output, Rect rect, uint16_t ink) {
        const int16_t cx = static_cast<int16_t>(rect.x + rect.w / 2);
        const int16_t cy = static_cast<int16_t>(rect.y + rect.h / 2);
        const int16_t x = static_cast<int16_t>(cx - 8);
        const int16_t y = static_cast<int16_t>(cy - 9);
        output.drawLine(x, y, static_cast<int16_t>(x + 16), y, ink);
        output.drawLine(x, static_cast<int16_t>(y + 18), static_cast<int16_t>(x + 16), static_cast<int16_t>(y + 18),
                        ink);
        output.drawLine(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 1), static_cast<int16_t>(x + 14),
                        static_cast<int16_t>(y + 17), ink);
        output.drawLine(static_cast<int16_t>(x + 14), static_cast<int16_t>(y + 1), static_cast<int16_t>(x + 2),
                        static_cast<int16_t>(y + 17), ink);
        output.fillRect(static_cast<int16_t>(cx - 3), static_cast<int16_t>(cy + 5), 6, 2, ink);
    }

    void Context::drawPowerIcon(Arduino_GFX& output, Rect rect, uint16_t ink, uint16_t surface) {
        const int16_t cx = static_cast<int16_t>(rect.x + rect.w / 2);
        const int16_t cy = static_cast<int16_t>(rect.y + rect.h / 2);
        output.drawCircle(cx, static_cast<int16_t>(cy + 1), 8, ink);
        output.fillRect(static_cast<int16_t>(cx - 3), static_cast<int16_t>(cy - 9), 7, 10, surface);
        output.drawFastVLine(cx, static_cast<int16_t>(cy - 9), 9, ink);
    }
    void Context::drawBatteryIcon(Arduino_GFX& output, Rect rect, uint8_t percent, bool charging, uint16_t ink,
                                  uint16_t surface) {
        constexpr uint16_t kBatteryGood = ui::themes::rgb565(126, 176, 92);
        constexpr uint16_t kBatteryMedium = ui::themes::rgb565(214, 163, 58);
        constexpr uint16_t kBatteryLow = ui::themes::rgb565(200, 82, 82);
        if (rect.w <= 0 || rect.h <= 0)
            return;

        constexpr int16_t capWidth = 3;
        constexpr int16_t capHeight = 5;

        percent = std::min<uint8_t>(percent, 100);

        const int16_t bodyWidth = std::max<int16_t>(0, static_cast<int16_t>(rect.w - capWidth));
        if (bodyWidth <= 0)
            return;

        const uint16_t fillColor = charging || percent > 35 ? kBatteryGood
                                 : percent <= 18            ? kBatteryLow
                                                            : kBatteryMedium;

        output.drawRect(rect.x, rect.y, bodyWidth, rect.h, ink);

        output.fillRect(static_cast<int16_t>(rect.x + bodyWidth),
                        static_cast<int16_t>(rect.y + (rect.h - capHeight) / 2), capWidth, capHeight, ink);

        const int16_t innerWidth = std::max<int16_t>(0, static_cast<int16_t>(bodyWidth - 4));
        const int16_t fill = charging ? innerWidth : static_cast<int16_t>(innerWidth * percent / 100);

        if (fill > 0) {
            output.fillRect(static_cast<int16_t>(rect.x + 2), static_cast<int16_t>(rect.y + 2), fill,
                            static_cast<int16_t>(rect.h - 4), fillColor);
        }

        if (charging) {
            output.drawLine(static_cast<int16_t>(rect.x + 15), static_cast<int16_t>(rect.y + 2),
                            static_cast<int16_t>(rect.x + 11), static_cast<int16_t>(rect.y + 7), surface);
            output.drawLine(static_cast<int16_t>(rect.x + 11), static_cast<int16_t>(rect.y + 7),
                            static_cast<int16_t>(rect.x + 16), static_cast<int16_t>(rect.y + 7), surface);
            output.drawLine(static_cast<int16_t>(rect.x + 16), static_cast<int16_t>(rect.y + 7),
                            static_cast<int16_t>(rect.x + 12), static_cast<int16_t>(rect.y + 12), surface);
        }

        markDrawn();
    }
} // namespace ui
