#include "ui/screens/watch/Layout.h"

namespace screens {
    bool typographySettings(ui::Context& ui, settings::TypographySettings& config, FontCatalog& fonts, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::Typography), Screen::Settings, screen);
        auto grid = ui.pagedGrid(area, 8, 1, 56);
        const auto families = fonts.families();
        const auto selected = std::ranges::find(families, config.fontId, &FontCatalog::Family::id);
        const size_t index = selected == families.end() ? 0 : selected - families.begin();
        bool changed = false;
        if (!families.empty() && watch::setting(ui, grid.item(0), UiText::Typeface, families[index].label)) {
            config.fontId = families[(index + 1) % families.size()].id;
            changed = true;
        }
        if (watch::setting(ui, grid.item(1), UiText::FontSize, RFont4::sizeLabel(config.fontSizeIndex))) {
            config.fontSizeIndex.cycle();
            changed = true;
        }
        changed |= watch::toggle(ui, grid.item(2), UiText::Focus, config.focusHighlight);
        changed |= watch::stepper(ui, grid.item(3), UiText::Tracking, config.tracking, " px");
        changed |= watch::stepper(ui, grid.item(4), UiText::Anchor, config.anchor, "%");
        changed |= watch::stepper(ui, grid.item(5), UiText::GuideWidth, config.guideWidth, " px");
        changed |= watch::stepper(ui, grid.item(6), UiText::GuideGap, config.guideGap, " px");
        if (ui.card(grid.item(7), ui.text(UiText::Reset), {}, watch::textSize(ui))) {
            changed |= config != settings::TypographySettings{};
            config = {};
        }
        return changed;
    }
} // namespace screens
