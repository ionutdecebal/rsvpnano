#include "ui/screens/ScreenCommon.h"

#include "localization/LocaleCatalog.h"

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

    void InterfaceScreen::cycleLanguage(ui::Context& ui, settings::InterfaceSettings& config) {
        config.locale = !languages_ ? (config.locale=="en"?"de":config.locale=="de"?"ro":"en")
                                   : std::string{nextLocale(*languages_, config.locale)};
        ui.setLocale(config.locale);
    }

    bool InterfaceScreen::begin(ui::Context& ui, settings::InterfaceSettings& config, const locales::Catalog& languages,
                                void (*setBrightness)(uint8_t)) {
        languages_ = &languages;
        if (setBrightness != nullptr)
            setBrightness(config.brightnessPercent);

        themes.loadFromSd();
        const ui::themes::Theme& selected = themes.resolve(config.selectedThemeId);
        const bool corrected = config.selectedThemeId != selected.id;
        config.selectedThemeId = selected.id;
        ui.setTheme(selected);
        ui.setLocale(config.locale);
        return corrected;
    }

} // namespace screens
