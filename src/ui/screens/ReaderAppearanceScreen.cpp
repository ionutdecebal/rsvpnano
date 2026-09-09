#include "ui/screens/ReaderScreen.h"

#include "ui/screens/ReaderAppearanceLayout.h"
#include "ui/screens/ReaderLayout.h"

namespace screens {
    bool ReaderScreen::appearance(ui::Context& ui, Screen& screen) {
        using namespace ui::themes;
        const auto layout = appearanceLayout::make(ui.width(), ui.height());
        const bool typography = appearancePage_ == 0;
        const bool reading = appearancePage_ == 1;
        const bool showPreview = !typography || !layout.pagedDials || appearanceDialPage_ == 0;
        auto& config = settings_;
        auto& type = config.typography;
        bool changed = false;

        const auto families = fonts.families();
        const auto selected = std::ranges::find(families, type.fontId, &FontCatalog::Family::id);
        const size_t family = selected == families.end() ? 0 : selected - families.begin();
        face_ = fonts.loadFace(family, type.fontSizeIndex);
        activateFace(face_);
        typography_ = type;
        // The preview uses the global face; the book must reselect its own override on return.
        loadedWordIndex_ = loadedFamilyIndex_ = renderedWordIndex_ = SIZE_MAX;

        constexpr std::string_view word = "read", before = "the", after = "on";
        text_.prepare(word);
        const auto& raster = face_.raster.get();
        const int16_t inkHeight = raster.wordInkBottom - raster.wordInkTop + 1;
        const int16_t baseline = (ui.height() - inkHeight) / 2 - raster.wordInkTop;
        const int16_t anchor = ui.width() * type.anchor / 100;
        const int focus = focusOffset(word);
        const int16_t focusCenter = text_.textAdvance(word.substr(0, focus), type.tracking)
                                  + (focus ? static_cast<int>(type.tracking) : 0) + text_.glyphAdvance(word[focus]) / 2;
        const int16_t wordWidth = text_.textAdvance(word, type.tracking);
        const int16_t wordX = anchor - focusCenter;
        const int16_t beforeWidth = text_.textAdvance(before, type.tracking);
        const int16_t afterWidth = text_.textAdvance(after, type.tracking);
        const int16_t beforeX = wordX - 24 - beforeWidth, afterX = wordX + wordWidth + 24;

        auto state = ui::Context::signature(type.fontId);
        for (int value:
             {int(type.fontSizeIndex), int(type.tracking), int(type.anchor), int(type.guideWidth), int(type.guideGap),
              int(type.focusHighlight), int(config.phantomWords), int(appearancePage_), int(config.arrowsVisibility)})
            state = ui::Context::combine(state, value);
        const auto footer =
            readerLayout::progressText(ui, reading ? settings::FooterMetric::percentage : config.footerMetric, 42, 132);
        const auto chrome = appearanceLayout::chrome(ui, config.leftHanded, layout.page);
        const ui::Rect preview = typography ? layout.preview : chrome.preview;
        if (showPreview && ui.redraw(preview, state)) {
            drawGuides(ui, anchor, baseline);
            drawWord(word, wordX, baseline, focus, false, ui);
            text_.setTextColor(ui.blend(Foreground, config.phantomWords ? 64 : 28), ui.color(Background));
            text_.drawString(before, beforeX, baseline, type.tracking);
            text_.drawString(after, afterX, baseline, type.tracking);
            if (!typography)
                readerLayout::drawArrows(ui, config, reading, inkHeight + 12, true);
        }

        if (ui.button(layout.back, "<")) {
            screen = Screen::Settings;
            appearancePage_ = appearanceDialPage_ = 0;
        }
        const UiText page = typography ? UiText::Typography : reading ? UiText::Reading : UiText::Paused;
        const std::string pageLabel = layout.pagedDials ? (typography ? "Aa"
                                                           : reading  ? ">"
                                                                      : "II")
                                                        : std::string{ui.text(page)};
        if (ui.button(layout.page, pageLabel, true, ui::Icon::None, 1)) {
            appearancePage_ = (appearancePage_ + 1) % 3;
            ui.invalidate();
        }

        const auto target = [&](int16_t x, int16_t width) {
            return appearanceLayout::wordTarget(x, width, ui.height() / 2, inkHeight, ui.width(), ui.height());
        };
        if (showPreview && ui.tap(target(wordX, wordWidth))) {
            type.focusHighlight = !type.focusHighlight;
            changed = true;
        }
        // Both phantom words operate the same setting, including their faint off-state placeholders.
        const bool previousTapped = showPreview && ui.tap(target(beforeX, beforeWidth));
        const bool nextTapped = showPreview && ui.tap(target(afterX, afterWidth));
        if (previousTapped || nextTapped) {
            config.phantomWords = !config.phantomWords;
            changed = true;
        }

        if (typography) {
            if (ui.button(layout.reset, ui.text(UiText::Reset))) {
                changed |= type != settings::TypographySettings{};
                type = {};
            }
            ui.label(layout.size, RFont4::sizeLabel(type.fontSizeIndex), 2, Muted);
            if (ui.tap(layout.size)) {
                type.fontSizeIndex.cycle();
                changed = true;
            }
            ui.label(layout.font, families.empty() ? std::string_view{} : families[family].label, 2);
            if (ui.tap(layout.font, !families.empty())) {
                type.fontId = families[(family + 1) % families.size()].id;
                changed = true;
            }
            if (layout.pagedDials && ui.button(layout.dialPage, std::to_string(appearanceDialPage_ + 1) + "/3")) {
                appearanceDialPage_ = (appearanceDialPage_ + 1) % 3;
                ui.invalidate();
            }
            const auto rotary = [&](size_t index, UiText label, auto& setting) {
                if (layout.pagedDials && (appearanceDialPage_ == 0 || index / 2 != appearanceDialPage_ - 1))
                    return;
                int value = setting;
                if (ui.rotary(layout.dials[index], value, setting.min(), setting.max(), setting.step(),
                              ui.text(label))) {
                    setting = value;
                    changed = true;
                }
            };
            rotary(0, UiText::Tracking, type.tracking);
            rotary(1, UiText::Anchor, type.anchor);
            rotary(2, UiText::Width, type.guideWidth);
            rotary(3, UiText::Gap, type.guideGap);
        } else {
            const Board::Power::BatteryState battery{{true, 3.9f, 64}, 0, false};
            const auto batteryLabel = readerLayout::batteryText(config.batteryLabel, battery);
            readerLayout::horizontalChrome(ui,
                                           {
                                               .vertical = false,
                                               .chapter = "Chapter 4",
                                               .locale = "en",
                                               .footer = footer,
                                               .batteryLabel = batteryLabel,
                                               .overlay = {},
                                               .reading = reading,

                                               .topState = state,
                                               .bottomState = state,

                                               .ghostHidden = true,
                                           },
                                           config, battery, chrome.reader);
            const auto toggle = [&](ui::Rect target, settings::Visibility& visibility) {
                // Keep each native slot independently tappable, with a 44-pixel minimum target.
                if (target.w < 44) {
                    target.x -= (44 - target.w) / 2;
                    target.w = 44;
                }
                target.y = std::clamp<int>(target.y + target.h / 2 - 22, 0, ui.height() - 44);
                target.h = 44;
                if (ui.tap(target)) {
                    settings::toggleVisibility(visibility, reading);
                    changed = true;
                }
            };
            toggle(chrome.reader.batteryParts.icon, config.batteryIconVisibility);
            toggle(chrome.reader.batteryParts.label, config.batteryLabelVisibility);
            toggle(chrome.reader.chapter, config.chapterVisibility);
            toggle(chrome.reader.progress, config.progressVisibility);
            toggle(chrome.reader.arrows, config.arrowsVisibility);
            const auto batteryType = config.batteryLabel == settings::BatteryLabel::percentage    ? "%"
                                   : config.batteryLabel == settings::BatteryLabel::timeRemaining ? "h"
                                                                                                  : "V";
            if (!reading && ui.button(chrome.batteryFormat, batteryType)) {
                config.batteryLabel = settings::cycleEnum(config.batteryLabel);
                changed = true;
            }
            if (!reading && ui.button(chrome.footerFormat, "%")) {
                config.footerMetric = settings::cycleEnum(config.footerMetric);
                changed = true;
            }
        }
        return changed;
    }
} // namespace screens
