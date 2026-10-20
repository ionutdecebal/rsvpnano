#include "ui/screens/ScreenCommon.h"

#include "localization/LocaleCatalog.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"

namespace screens {
    namespace {

        std::string_view nextLocale(const locales::Catalog& catalog, std::string_view current) {
            if(current=="en")return "de";
            if(current=="de")return "ro";
            bool returnNext = current == "ro";
            for (const auto& pack: catalog) {
                if(pack.locale=="en"||pack.locale=="de"||pack.locale=="ro")continue;
                if (returnNext)
                    return pack.locale;
                returnNext = pack.locale == current;
            }
            return Localization::kDefaultLocale;
        }

    } // namespace

    bool InterfaceScreen::draw(ui::Context& ui, settings::InterfaceSettings& config,
                               std::span<const uint32_t> standbyDurations, void (*setBrightness)(uint8_t),
                               Screen& screen) {
        bool changed = false;
        if (ui.modernPresentation()) {
            using ui::modern::button;
            using ui::modern::Style;
            if (modern::header(ui, modern::copy(ui,modern::Copy::Display), screen, Screen::Settings) == Action::Resume) screen = Screen::Reader;
            if (button(ui, {12, 51, 300, 52}, ui.text(UiText::Theme), Style::Secondary, true,
                       config.selectedThemeId == ui::themes::kDefaultThemeId && themes.resolve(config.selectedThemeId).definition.colors == ui::themes::ThemeColors{} ? "Ink" : themes.resolve(config.selectedThemeId).definition.name)) {
                const auto& next = themes.next(config.selectedThemeId);
                config.selectedThemeId = next.id; ui.setTheme(next); changed = true;
            }
            if (button(ui, {328, 51, 300, 52}, ui.text(UiText::Language), Style::Secondary, true,
                       languages_ ? locales::localeName(*languages_, config.locale) : std::string_view{config.locale})) {
                config.locale = !languages_ ? (config.locale=="en"?"de":config.locale=="de"?"ro":"en") : std::string{nextLocale(*languages_, config.locale)};
                ui.setLocale(config.locale); changed = true;
            }
            std::string standby{ui.text(UiText::Off)};
            const size_t index = config.standbyTimerIndex;
            if (index < standbyDurations.size() && standbyDurations[index]) standby = std::to_string(standbyDurations[index] / 60000UL) + "m";
            if (button(ui, {12, 111, 300, 53}, ui.text(UiText::Standby), Style::Secondary, true, standby)) {
                config.standbyTimerIndex.cycle(); changed = true;
            }
            const UiText saver = config.screensaver == standby::Kind::maze ? UiText::Maze
                               : config.screensaver == standby::Kind::voronoi ? UiText::Voronoi
                               : config.screensaver == standby::Kind::reaction ? UiText::Reaction
                               : config.screensaver == standby::Kind::screenOff ? UiText::ScreenOff : UiText::Life;
            if (button(ui, {328, 111, 300, 53}, ui.text(UiText::Screensaver), Style::Secondary, true, ui.text(saver))) {
                config.screensaver = settings::cycleEnum(config.screensaver); changed = true;
            }
            return changed;
        }
        const ui::Rect content = detail::content(ui);
        constexpr int16_t gap = 6;
        constexpr int16_t backWidth = 56;
        constexpr int16_t topHeight = 42;
        if (ui.button({content.x, content.y, backWidth, topHeight}, "<<"))
            screen = ui.modernPresentation() ? Screen::DisplayHub : Screen::Settings;
        if (ui.slider({static_cast<int16_t>(content.x + backWidth + gap), content.y,
                       static_cast<int16_t>(content.w - backWidth - gap), topHeight},
                      ui.text(UiText::Brightness), config.brightnessPercent, "%")) {
            if (setBrightness != nullptr)
                setBrightness(config.brightnessPercent);
            changed = true;
        }

        const int16_t halfWidth = static_cast<int16_t>((content.w - gap) / 2);
        const int16_t sectionsY = static_cast<int16_t>(content.y + topHeight + 4);
        ui.separator({content.x, sectionsY, halfWidth, 10}, ui.text(UiText::AppearanceSection));
        ui.separator({static_cast<int16_t>(content.x + halfWidth + gap), sectionsY, halfWidth, 10},
                     ui.text(UiText::StandbySection));

        const int16_t firstRowY = static_cast<int16_t>(sectionsY + 14);
        constexpr int16_t rowHeight = 40;
        const int16_t secondRowY = static_cast<int16_t>(firstRowY + rowHeight + 4);
        const ui::themes::Theme& selectedTheme = themes.resolve(config.selectedThemeId);
        if (ui.setting({content.x, firstRowY, halfWidth, rowHeight}, ui.text(UiText::Theme),
                       selectedTheme.definition.name, ui::SettingLayout::Inline)) {
            const ui::themes::Theme& nextTheme = themes.next(config.selectedThemeId);
            config.selectedThemeId = nextTheme.id;
            ui.setTheme(nextTheme);
            changed = true;
        }

        if (ui.setting({content.x, secondRowY, halfWidth, rowHeight}, ui.text(UiText::Language),
                       languages_ ? locales::localeName(*languages_, config.locale) : std::string_view{config.locale},
                       ui::SettingLayout::Inline)) {
            config.locale = !languages_ ? std::string{Localization::kDefaultLocale}
                                        : std::string{nextLocale(*languages_, config.locale)};
            ui.setLocale(config.locale);
            changed = true;
        }

        std::string standby{ui.text(UiText::Off)};
        const size_t standbyIndex = config.standbyTimerIndex;
        if (standbyIndex < standbyDurations.size() && standbyDurations[standbyIndex] != 0) {
            standby = std::to_string(standbyDurations[standbyIndex] / 60000UL) + "m";
        }
        if (ui.setting({static_cast<int16_t>(content.x + halfWidth + gap), firstRowY, halfWidth, rowHeight},
                       ui.text(UiText::Standby), standby, ui::SettingLayout::Inline)) {
            config.standbyTimerIndex.cycle();
            changed = true;
        }

        const UiText screensaver = config.screensaver == standby::Kind::maze      ? UiText::Maze
                                 : config.screensaver == standby::Kind::voronoi   ? UiText::Voronoi
                                 : config.screensaver == standby::Kind::reaction  ? UiText::Reaction
                                 : config.screensaver == standby::Kind::screenOff ? UiText::ScreenOff
                                                                                  : UiText::Life;
        if (ui.setting({static_cast<int16_t>(content.x + halfWidth + gap), secondRowY, halfWidth, rowHeight},
                       ui.text(UiText::Screensaver), ui.text(screensaver), ui::SettingLayout::Inline)) {
            config.screensaver = settings::cycleEnum(config.screensaver);
            changed = true;
        }
        return changed;
    }

} // namespace screens
