#include "ui/screens/ScreenCommon.h"

#include <algorithm>
#include <ranges>

namespace screens {
    bool bookFonts(ui::Context& ui, const BookMetadata& metadata, settings::ReadingOverrides& overrides,
                   const locales::Catalog& localeCatalog, FontCatalog& fonts, Screen& screen) {
        const ui::Rect content = detail::content(ui);
        size_t languageCount = 0;
        metadata.forEachLanguage([&languageCount](std::string_view, uint32_t scripts) {
            languageCount += (scripts & ~UnicodeText::ScriptMath) != 0;
        });
        const bool useUndetermined = languageCount == 0 && (metadata.scriptMask & ~UnicodeText::ScriptMath) != 0;
        languageCount += useUndetermined;
        if ((metadata.scriptMask & UnicodeText::ScriptMath) != 0)
            ++languageCount;

        constexpr int16_t gap = 4;
        const uint8_t columns = content.w >= 600 ? 4 : 3;
        const size_t itemCount = languageCount + 2;
        const size_t rows = (itemCount + columns - 1) / columns;
        const int16_t rowHeight = std::min<int16_t>(34, static_cast<int16_t>(
            (content.h - gap * static_cast<int16_t>(rows - 1)) / static_cast<int16_t>(rows)));
        ui::Grid grid{content, columns, rowHeight, gap};
        if (ui.button(grid.next(), "<<"))
            screen = Screen::Read;
        if (ui.button(grid.next(), ui.text(UiText::Reset))) {
            const bool changed = !overrides.languageFonts.empty();
            overrides.languageFonts.clear();
            return changed;
        }

        const auto families = fonts.families();
        bool changed = false;
        const auto chooseFont = [&](std::string_view label, std::string_view locale, uint32_t requiredScripts,
                                    std::string_view selectedId) -> std::optional<std::string> {
            size_t active = families.size();
            for (size_t familyIndex = 0; familyIndex < families.size(); ++familyIndex) {
                if (families[familyIndex].usableFor(locale, requiredScripts)
                    && families[familyIndex].id == selectedId) {
                    active = familyIndex;
                    break;
                }
            }

            const std::string_view value = selectedId.empty() || active == families.size()
                                             ? ui.text(UiText::Default)
                                             : std::string_view{families[active].label};
            if (!ui.setting(grid.next(), label, value, ui::SettingLayout::Inline))
                return std::nullopt;
            for (size_t next = active == families.size() ? 0 : active + 1; next < families.size(); ++next)
                if (families[next].usableFor(locale, requiredScripts))
                    return families[next].id;
            return active == families.size() ? std::nullopt : std::optional<std::string>{std::string{}};
        };

        const auto drawTarget = [&](std::string_view locale, uint32_t requiredScripts) {
            const auto selected = std::ranges::find(overrides.languageFonts, locale,
                                                    &settings::LanguageFont::locale);
            const std::string_view selectedId = selected == overrides.languageFonts.end()
                                                  ? std::string_view{}
                                                  : std::string_view{selected->fontId};
            const bool math = locale == settings::kMathFontTarget;
            const auto nextId = chooseFont(math ? std::string_view{"Math"} : locales::localeName(localeCatalog, locale),
                                           math ? std::string_view{} : locale, requiredScripts, selectedId);
            if (!nextId)
                return;
            if (nextId->empty()) {
                if (selected != overrides.languageFonts.end())
                    overrides.languageFonts.erase(selected);
            } else if (selected == overrides.languageFonts.end()) {
                overrides.languageFonts.push_back({.locale = std::string{locale},
                                                   .fontId = std::move(*nextId)});
            } else {
                selected->fontId = std::move(*nextId);
            }
            changed = true;
        };
        metadata.forEachLanguage([&drawTarget](std::string_view locale, uint32_t scripts) {
            scripts &= ~UnicodeText::ScriptMath;
            if (scripts != 0)
                drawTarget(locale, scripts);
        });
        if (useUndetermined)
            drawTarget("und", metadata.scriptMask & ~UnicodeText::ScriptMath);
        if ((metadata.scriptMask & UnicodeText::ScriptMath) != 0)
            drawTarget(settings::kMathFontTarget, UnicodeText::ScriptMath);
        return changed;
    }

} // namespace screens
