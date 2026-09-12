#include "ui/screens/watch/Layout.h"

#include <algorithm>
#include <ranges>

namespace screens {
    bool bookFonts(ui::Context& ui, const BookMetadata& metadata, settings::ReadingOverrides& overrides,
                   const locales::Catalog& localeCatalog, FontCatalog& fonts, Screen& screen) {
        size_t languageCount = 0;
        metadata.forEachLanguage([&languageCount](std::string_view, uint32_t scripts) {
            languageCount += (scripts & ~UnicodeText::ScriptMath) != 0;
        });
        const bool useUndetermined = languageCount == 0 && (metadata.scriptMask & ~UnicodeText::ScriptMath) != 0;
        languageCount += useUndetermined;
        if ((metadata.scriptMask & UnicodeText::ScriptMath) != 0)
            ++languageCount;

        auto area = watch::header(ui, ui.text(UiText::Typeface), Screen::Read, screen);
        const auto page = ui.pagedGrid(area, languageCount + 1, 1, 56);
        size_t row = 0;
        if (ui.card(page.item(row++), ui.text(UiText::Reset), {}, watch::textSize(ui))) {
            const bool changed = !overrides.languageFonts.empty();
            overrides.languageFonts.clear();
            return changed;
        }

        const auto families = fonts.families();
        bool changed = false;
        const auto chooseFont = [&](std::string_view label, std::string_view locale, uint32_t requiredScripts,
                                    std::string_view selectedId) -> std::optional<std::string> {
            const auto rect = page.item(row++);
            if (rect.w <= 0 || rect.h <= 0)
                return std::nullopt;
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
            if (!ui.card(rect, label, value, watch::textSize(ui)))
                return std::nullopt;
            for (size_t next = active == families.size() ? 0 : active + 1; next < families.size(); ++next)
                if (families[next].usableFor(locale, requiredScripts))
                    return families[next].id;
            return active == families.size() ? std::nullopt : std::optional<std::string>{std::string{}};
        };

        const auto drawTarget = [&](std::string_view locale, uint32_t requiredScripts) {
            const auto selected = std::ranges::find(overrides.languageFonts, locale, &settings::LanguageFont::locale);
            const std::string_view selectedId =
                selected == overrides.languageFonts.end() ? std::string_view{} : std::string_view{selected->fontId};
            const bool math = locale == settings::kMathFontTarget;
            const auto nextId = chooseFont(math ? std::string_view{"Math"} : locales::localeName(localeCatalog, locale),
                                           math ? std::string_view{} : locale, requiredScripts, selectedId);
            if (!nextId)
                return;
            if (nextId->empty()) {
                if (selected != overrides.languageFonts.end())
                    overrides.languageFonts.erase(selected);
            } else if (selected == overrides.languageFonts.end()) {
                overrides.languageFonts.push_back({.locale = std::string{locale}, .fontId = std::move(*nextId)});
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
