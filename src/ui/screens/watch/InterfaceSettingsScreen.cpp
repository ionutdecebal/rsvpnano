#include "ui/screens/watch/Layout.h"

namespace screens {
    bool InterfaceScreen::draw(ui::Context& ui, settings::InterfaceSettings& config,
                               std::span<const uint32_t> standbyDurations, void (*setBrightness)(uint8_t),
                               Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::Display), Screen::Settings, screen);
        auto grid = ui.pagedGrid(area, 5, 1, 56);
        bool changed = false;
        if (watch::stepper(ui, grid.item(0), UiText::Brightness, config.brightnessPercent, "%")) {
            if (setBrightness)
                setBrightness(config.brightnessPercent);
            changed = true;
        }
        if (watch::setting(ui, grid.item(1), UiText::Theme, themes.resolve(config.selectedThemeId).definition.name)) {
            const auto& next = themes.next(config.selectedThemeId);
            config.selectedThemeId = next.id;
            ui.setTheme(next);
            changed = true;
        }
        if (watch::setting(ui, grid.item(2), UiText::Language,
                           languages_ ? locales::localeName(*languages_, config.locale) : config.locale)) {
            std::string next{Localization::kDefaultLocale};
            bool found = config.locale == Localization::kDefaultLocale;
            if (languages_)
                for (const auto& pack: *languages_) {
                    if (found) {
                        next = pack.locale;
                        break;
                    }
                    found = pack.locale == config.locale;
                }
            config.locale = std::move(next);
            ui.setLocale(config.locale);
            changed = true;
        }
        const size_t index = config.standbyTimerIndex;
        const std::string duration = index < standbyDurations.size() && standbyDurations[index]
                                       ? std::to_string(standbyDurations[index] / 60000) + "m"
                                       : std::string{ui.text(UiText::Off)};
        if (watch::setting(ui, grid.item(3), UiText::Standby, duration)) {
            config.standbyTimerIndex.cycle();
            changed = true;
        }
        const UiText kind = config.screensaver == standby::Kind::maze      ? UiText::Maze
                          : config.screensaver == standby::Kind::voronoi   ? UiText::Voronoi
                          : config.screensaver == standby::Kind::reaction  ? UiText::Reaction
                          : config.screensaver == standby::Kind::screenOff ? UiText::ScreenOff
                                                                           : UiText::Life;
        if (watch::setting(ui, grid.item(4), UiText::Screensaver, ui.text(kind))) {
            config.screensaver = settings::cycleEnum(config.screensaver);
            changed = true;
        }
        return changed;
    }
} // namespace screens
