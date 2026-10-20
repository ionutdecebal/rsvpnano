#include "ui/screens/ScreenCommon.h"
#include "ui/screens/ModernScreens.h"

#include <algorithm>
#include <ranges>

namespace screens {
    namespace {
        struct BookLanguage { std::string_view tag, name; };
        constexpr BookLanguage commonLanguages[]{
            {"", "Automatic"}, {"en", "English"}, {"fr", "French"}, {"de", "German"},
            {"es", "Spanish"}, {"it", "Italian"}, {"pt", "Portuguese"}, {"nl", "Dutch"},
            {"pl", "Polish"}, {"ru", "Russian"}, {"uk", "Ukrainian"}, {"tr", "Turkish"},
            {"ar", "Arabic"}, {"he", "Hebrew"}, {"hi", "Hindi"}, {"zh", "Chinese"},
            {"ja", "Japanese"}, {"ko", "Korean"}
        };
        std::string_view languageName(const locales::Catalog& catalog, std::string_view locale) {
            const auto common = std::ranges::find(commonLanguages, locale, &BookLanguage::tag);
            if (common != std::end(commonLanguages)) return common->name;
            if (locale.empty() || locale == "und") return "Unspecified";
            return locales::localeName(catalog, locale);
        }
        bool modernBookFonts(ui::Context& ui, const BookMetadata& metadata, settings::ReadingOverrides& overrides,
                             const locales::Catalog& catalog, FontCatalog& fonts, Screen& screen) {
            using ui::modern::button;
            using ui::modern::Style;
            static bool choosingLanguage = false;
            static size_t languagePage = 0, targetIndex = 0;
            if (choosingLanguage) {
                std::vector<BookLanguage> choices(std::begin(commonLanguages), std::end(commonLanguages));
                const auto add = [&](std::string_view tag) {
                    if (!tag.empty() && tag != "und" && std::ranges::find(choices, tag, &BookLanguage::tag) == choices.end())
                        choices.push_back({tag, languageName(catalog, tag)});
                };
                metadata.forEachLanguage([&](std::string_view tag, uint32_t) { add(tag); });
                for (const auto& pack : catalog) add(pack.locale);
                if (overrides.locale) add(*overrides.locale);
                const size_t pages = (choices.size()+5)/6;
                languagePage = std::min(languagePage,pages-1);
                if (button(ui,{8,0,108,44},"‹ Book settings",Style::Plain,true,{},12)) { choosingLanguage=false; ui.invalidate(); }
                ui.label({126,0,290,44},"Book language",16);
                if (button(ui,{438,0,52,44},"‹",Style::Plain,languagePage>0)) { --languagePage; ui.invalidate(); }
                ui.label({494,0,76,44},std::to_string(languagePage+1)+" / "+std::to_string(pages),12,ui::themes::Muted,ui::TextAlign::Center);
                if (button(ui,{576,0,52,44},"›",Style::Plain,languagePage+1<pages)) { ++languagePage; ui.invalidate(); }
                const std::string_view active = overrides.locale ? std::string_view{*overrides.locale} : std::string_view{};
                bool changed = false;
                for (size_t i=0;i<6 && languagePage*6+i<choices.size();++i) {
                    const auto& choice = choices[languagePage*6+i];
                    if (button(ui,{int16_t(12+(i%3)*210),int16_t(51+(i/3)*60),int16_t(i%3==2?196:198),53},
                               choice.name,active==choice.tag?Style::Accent:Style::Secondary,true,
                               choice.tag.empty()?"Use book metadata":choice.tag,14)) {
                        if (choice.tag.empty()) overrides.locale.reset(); else overrides.locale=choice.tag;
                        choosingLanguage=false; changed=true; ui.invalidate();
                    }
                }
                return changed;
            }
            if (modern::header(ui,"Book font & language",screen,Screen::Settings)==Action::Resume) screen=Screen::Reader;
            const auto language = overrides.locale ? languageName(catalog,*overrides.locale)
                                                    : languageName(catalog,metadata.locale.empty()?"und":std::string_view{metadata.locale});
            const std::string languageValue = overrides.locale ? std::string{language} : "Auto · "+std::string{language};
            if (button(ui,{12,51,300,53},"Book language",Style::Secondary,true,languageValue)) {
                choosingLanguage=true;languagePage=0;ui.invalidate();
            }
            bool changed=false;
            if (button(ui,{12,111,300,53},"Use book defaults",Style::Secondary,
                       overrides.locale.has_value() || !overrides.languageFonts.empty(),"Reset language and font overrides")) {
                overrides.locale.reset();overrides.languageFonts.clear();changed=true;
            }
            struct Target { std::string_view locale; uint32_t scripts; };
            std::vector<Target> targets;
            metadata.forEachLanguage([&](std::string_view locale,uint32_t scripts) {
                if (scripts & ~UnicodeText::ScriptMath) targets.push_back({locale,scripts & ~UnicodeText::ScriptMath});
            });
            if (targets.empty()) targets.push_back({metadata.locale.empty()?"und":std::string_view{metadata.locale},metadata.scriptMask & ~UnicodeText::ScriptMath});
            if (metadata.scriptMask & UnicodeText::ScriptMath) targets.push_back({settings::kMathFontTarget,UnicodeText::ScriptMath});
            targetIndex=std::min(targetIndex,targets.size()-1);
            const auto target=targets[targetIndex];
            const auto targetName = target.locale==settings::kMathFontTarget ? std::string_view{"Math"} : languageName(catalog,target.locale);
            if (button(ui,{328,51,300,53},targets.size()==1?"Font applies to":"Font section ›",Style::Secondary,true,
                       std::string{targetName}+(targets.size()>1?" · "+std::to_string(targetIndex+1)+" / "+std::to_string(targets.size()):" text"))) {
                targetIndex=(targetIndex+1)%targets.size();ui.invalidate();
            }
            const auto selected=std::ranges::find(overrides.languageFonts,target.locale,&settings::LanguageFont::locale);
            const std::string_view id=selected==overrides.languageFonts.end()?std::string_view{}:std::string_view{selected->fontId};
            const auto families=fonts.families();
            const auto active=std::ranges::find(families,id,&FontCatalog::Family::id);
            const auto label=active==families.end()?std::string_view{"Use appearance font"}:std::string_view{active->label};
            if (button(ui,{328,111,300,53},"Typeface ›",Style::Secondary,true,label)) {
                size_t next=active==families.end()?0:static_cast<size_t>(active-families.begin())+1;
                const auto fontLocale=target.locale==settings::kMathFontTarget?std::string_view{}:target.locale;
                while(next<families.size() && !families[next].usableFor(fontLocale,target.scripts)) ++next;
                if(next==families.size()) {
                    if(selected!=overrides.languageFonts.end()) { overrides.languageFonts.erase(selected);changed=true; }
                } else {
                    if(selected==overrides.languageFonts.end()) overrides.languageFonts.push_back({std::string{target.locale},families[next].id});
                    else selected->fontId=families[next].id;
                    changed=true;
                }
            }
            return changed;
        }
    }

    bool bookFonts(ui::Context& ui, const BookMetadata& metadata, settings::ReadingOverrides& overrides,
                   const locales::Catalog& localeCatalog, FontCatalog& fonts, Screen& screen) {
        if (ui.modernPresentation()) return modernBookFonts(ui,metadata,overrides,localeCatalog,fonts,screen);
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
            screen = ui.modernPresentation() ? Screen::Settings : Screen::Read;
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
