#include "ui/screens/ScreenCommon.h"

#include <algorithm>

namespace screens {
    bool typographySettings(ui::Context& ui, settings::TypographySettings& config, FontCatalog& fonts,
                            Screen& screen) {
        const auto families = fonts.families();
        const auto selected = std::ranges::find_if(families, [&config](const FontCatalog::Family& family) {
            return family.id == config.fontId;
        });
        size_t familyIndex = selected == families.end() ? 0 : selected - families.begin();
        bool changed = false;
        const ui::Rect content = detail::content(ui);
        constexpr int16_t gap = 4;
        constexpr int16_t topHeight = 36;
        constexpr int16_t backWidth = 56;
        constexpr int16_t resetWidth = 72;
        if (ui.button({content.x, content.y, backWidth, topHeight}, "<<"))
            screen = Screen::Settings;
        const int16_t resetX = static_cast<int16_t>(content.x + content.w - resetWidth);
        if (ui.button({resetX, content.y, resetWidth, topHeight}, ui.text(UiText::Reset))) {
            changed = config != settings::TypographySettings{};
            config = {};
            const auto inheritedFamily = std::ranges::find_if(families, [&config](const FontCatalog::Family& family) {
                return family.id == config.fontId;
            });
            familyIndex = inheritedFamily == families.end() ? 0 : inheritedFamily - families.begin();
        }

        if (ui.setting({static_cast<int16_t>(content.x + backWidth + gap), content.y,
                        static_cast<int16_t>(content.w - backWidth - resetWidth - gap * 2), topHeight},
                       ui.text(UiText::Typeface), families[familyIndex].label.c_str(), ui::SettingLayout::Inline)) {
            const size_t next = (familyIndex + 1) % families.size();
            config.fontId = families[next].id;
            familyIndex = next;
            changed = true;
        }

        const int16_t fontRowY = static_cast<int16_t>(content.y + topHeight + gap);
        const int16_t halfWidth = static_cast<int16_t>((content.w - gap) / 2);
        if (ui.setting({content.x, fontRowY, halfWidth, 34}, ui.text(UiText::FontSize),
                       RFont4::sizeLabel(config.fontSizeIndex), ui::SettingLayout::Inline)) {
            config.fontSizeIndex.cycle();
            changed = true;
        }
        changed |= ui.toggle({static_cast<int16_t>(content.x + halfWidth + gap), fontRowY, halfWidth, 34},
                             ui.text(UiText::Focus), config.focusHighlight);

        const int16_t geometryY = static_cast<int16_t>(fontRowY + 38);
        ui.separator({content.x, geometryY, content.w, 10}, ui.text(UiText::GeometrySection));
        ui::Grid geometry{{content.x, static_cast<int16_t>(geometryY + 12), content.w,
                           static_cast<int16_t>(content.y + content.h - geometryY - 12)},
                          2,
                          static_cast<int16_t>((content.y + content.h - geometryY - 16) / 2),
                          4};
        changed |= ui.slider(geometry.next(), ui.text(UiText::Tracking), config.tracking, " px");
        changed |= ui.slider(geometry.next(), ui.text(UiText::Anchor), config.anchor, "%");
        changed |= ui.slider(geometry.next(), ui.text(UiText::GuideWidth), config.guideWidth, " px");
        changed |= ui.slider(geometry.next(), ui.text(UiText::GuideGap), config.guideGap, " px");
        return changed;
    }

} // namespace screens
