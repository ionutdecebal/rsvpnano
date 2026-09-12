#include "ui/Ui.h"

#include <algorithm>
#include <array>

#include "text/Utf8Text.h"

namespace ui {
    namespace {

        constexpr std::array<std::string_view, 10> kTopLetters = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
        constexpr std::array<std::string_view, 9> kMiddleLetters = {"a", "s", "d", "f", "g", "h", "j", "k", "l"};
        constexpr std::array<std::string_view, 7> kBottomLetters = {"z", "x", "c", "v", "b", "n", "m"};
        constexpr std::array<std::string_view, 10> kNumbers = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
        constexpr std::array<std::string_view, 9> kNumberSymbols = {"@", "#", "$", "%", "&", "*", "(", ")", "-"};
        constexpr std::array<std::string_view, 10> kNumberExtras = {"+", "=", "_", "/", "\\", ":", ";", "?", "!", "."};
        constexpr std::array<std::string_view, 10> kTopSymbols = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")"};
        constexpr std::array<std::string_view, 9> kMiddleSymbols = {"[", "]", "{", "}", "<", ">", "+", "=", "~"};
        constexpr std::array<std::string_view, 10> kBottomSymbols = {"\\", "|", ";", ":", "'",
                                                                     "\"", ",", "/", "`", "?"};

    } // namespace

    KeyboardAction Context::keyboard(Rect rect, std::string& value, size_t maxLength, KeyboardState& state,
                                     std::string_view label, bool masked) {
        constexpr int16_t gap = 1;
        const int16_t inputHeight = std::min<int16_t>(36, std::max<int16_t>(32, rect.h / 5));
        const int16_t rowHeight = std::max<int16_t>(1, static_cast<int16_t>((rect.h - inputHeight - gap * 4) / 4));
        const int16_t firstRowY = static_cast<int16_t>(rect.y + inputHeight + gap);
        const int16_t keyWidth = std::max<int16_t>(1, static_cast<int16_t>((rect.w - gap * 9) / 10));

        std::array<char, 64> hidden{};
        const size_t hiddenLength = std::min(value.size(), hidden.size());
        std::ranges::fill_n(hidden.begin(), hiddenLength, '*');
        std::string_view shownValue =
            masked && !state.passwordVisible ? std::string_view{hidden.data(), hiddenLength} : std::string_view{value};
        const int16_t clearWidth = inputHeight;
        const int16_t revealWidth = masked ? std::clamp<int16_t>(rect.w / 7, 72, 96) : 0;
        const int16_t valueWidth = static_cast<int16_t>(rect.w - clearWidth - gap - (masked ? revealWidth + gap : 0));
        const size_t visibleCharacters = static_cast<size_t>(std::max<int16_t>(1, (valueWidth - 12) / 12));
        shownValue = Utf8Text::suffix(shownValue, visibleCharacters);
        const Rect input = paintBounds({rect.x, rect.y, valueWidth, inputHeight});
        if (claim(Kind::KeyboardInput, input, signature(shownValue, signature(label))).changed) {
            const auto caption = label.empty()
                                   ? TextLayout{}
                                   : prepareText({static_cast<int16_t>(input.x + 6), static_cast<int16_t>(input.y + 2),
                                                  static_cast<int16_t>(input.w - 12), 8},
                                                 label, 1);
            const auto contents = prepareText({static_cast<int16_t>(input.x + 6),
                                               static_cast<int16_t>(input.y + (label.empty() ? 0 : 11)),
                                               static_cast<int16_t>(input.w - 12),
                                               static_cast<int16_t>(input.h - (label.empty() ? 0 : 11))},
                                              shownValue.empty() ? std::string_view{"_"} : shownValue, 2);
            paint(input, [&](Arduino_GFX& output, Rect local) {
                const uint16_t surface = color(ui::themes::ColorRole::SurfaceMuted);
                output.fillRoundRect(local.x, local.y, local.w, local.h, 5, surface);
                output.drawRoundRect(local.x, local.y, local.w, local.h, 5, color(ui::themes::ColorRole::Outline));
                const int16_t dx = local.x - input.x, dy = local.y - input.y;
                if (!label.empty())
                    drawText(output, caption, color(ui::themes::ColorRole::Muted), dx, dy);
                drawText(output, contents, color(ui::themes::ColorRole::Accent), dx, dy);
            });
        }
        const int16_t clearX = static_cast<int16_t>(rect.x + valueWidth + gap);
        if (button({clearX, rect.y, clearWidth, inputHeight}, "X", !value.empty()))
            value.clear();
        if (masked
            && button({static_cast<int16_t>(clearX + clearWidth + gap), rect.y, revealWidth, inputHeight},
                      text(state.passwordVisible ? UiText::Hide : UiText::Show))) {
            state.passwordVisible = !state.passwordVisible;
        }

        const auto append = [&](std::string_view key) {
            if (key.empty() || value.size() + key.size() > maxLength)
                return;
            if (state.shifted && key.size() == 1 && key.front() >= 'a' && key.front() <= 'z')
                value.push_back(static_cast<char>(key.front() - 'a' + 'A'));
            else
                value.append(key);
            if (state.shifted)
                state.shifted = false;
        };

        const auto eraseLast = [&] {
            if (value.empty())
                return;
            value.erase(Utf8Text::lastCodepointStart(value));
        };

        const auto drawCharacters = [&]<size_t N>(const std::array<std::string_view, N>& keys, int16_t y,
                                                  uint8_t firstColumn = 0) {
            for (size_t index = 0; index < N; ++index) {
                const std::string_view key = keys[index];
                char uppercase[2] = {key.empty() ? '\0' : key.front(), '\0'};
                if (state.shifted && uppercase[0] >= 'a' && uppercase[0] <= 'z')
                    uppercase[0] = static_cast<char>(uppercase[0] - 'a' + 'A');
                const std::string_view shown = state.shifted && key.size() == 1 ? std::string_view{uppercase, 1} : key;
                const int16_t x = static_cast<int16_t>(rect.x + (firstColumn + index) * (keyWidth + gap));
                if (button({x, y, keyWidth, rowHeight}, shown))
                    append(key);
            }
        };

        const int16_t secondRowY = static_cast<int16_t>(firstRowY + rowHeight + gap);
        const int16_t thirdRowY = static_cast<int16_t>(secondRowY + rowHeight + gap);
        const int16_t backX = static_cast<int16_t>(rect.x + 9 * (keyWidth + gap));
        switch (state.mode) {
        case KeyboardMode::Letters:
            drawCharacters(kTopLetters, firstRowY);
            drawCharacters(kMiddleLetters, secondRowY);
            if (button({backX, secondRowY, keyWidth, rowHeight}, "<-"))
                eraseLast();
            if (button({rect.x, thirdRowY, keyWidth, rowHeight}, state.shifted ? "a" : "A"))
                state.shifted = !state.shifted;
            drawCharacters(kBottomLetters, thirdRowY, 1);
            if (button({static_cast<int16_t>(rect.x + 8 * (keyWidth + gap)), thirdRowY, keyWidth, rowHeight}, "-"))
                append("-");
            if (button({backX, thirdRowY, keyWidth, rowHeight}, "'"))
                append("'");
            break;
        case KeyboardMode::Numbers:
            drawCharacters(kNumbers, firstRowY);
            drawCharacters(kNumberSymbols, secondRowY);
            if (button({backX, secondRowY, keyWidth, rowHeight}, "<-"))
                eraseLast();
            drawCharacters(kNumberExtras, thirdRowY);
            break;
        case KeyboardMode::Symbols:
            drawCharacters(kTopSymbols, firstRowY);
            drawCharacters(kMiddleSymbols, secondRowY);
            if (button({backX, secondRowY, keyWidth, rowHeight}, "<-"))
                eraseLast();
            drawCharacters(kBottomSymbols, thirdRowY);
            break;
        }

        const int16_t controlsY = static_cast<int16_t>(thirdRowY + rowHeight + gap);
        const int16_t availableWidth = static_cast<int16_t>(rect.w - gap * 3);
        const int16_t modeWidth = static_cast<int16_t>(availableWidth * 3 / 18);
        const int16_t spaceWidth = static_cast<int16_t>(availableWidth * 7 / 18);
        const int16_t backWidth = static_cast<int16_t>(availableWidth * 4 / 18);
        const int16_t okWidth = static_cast<int16_t>(availableWidth - modeWidth - spaceWidth - backWidth);
        ui::Row controls{{rect.x, controlsY, rect.w, rowHeight}, gap};
        constexpr std::array<std::string_view, 3> modeLabels = {"ABC", "123", "#+="};
        if (button(controls.next(modeWidth), modeLabels[static_cast<uint8_t>(state.mode)])) {
            state.mode = static_cast<KeyboardMode>((static_cast<uint8_t>(state.mode) + 1U) % modeLabels.size());
            state.shifted = false;
        }
        if (button(controls.next(spaceWidth), text(UiText::Space)))
            append(" ");
        const bool cancel = button(controls.next(backWidth), "<<");
        const bool submit = button(controls.next(okWidth), "OK");
        return submit ? KeyboardAction::Submit : cancel ? KeyboardAction::Cancel : KeyboardAction::None;
    }

} // namespace ui
