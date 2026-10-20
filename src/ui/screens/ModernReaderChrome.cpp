#include "ui/screens/ModernReaderChrome.h"

namespace screens::modern {
    ReaderControl readerControlAt(uint16_t x, uint16_t y, const settings::ReadingSettings& config,
                                  bool reading, bool canRead, bool focusActive) {
        if (focusActive && ui::contains(readerControlRect(ReaderControl::Focus),x,y)) return ReaderControl::Focus;
        if (canRead && settings::visible(config.chapterVisibility,reading)
            && ui::contains(readerControlRect(ReaderControl::Chapter),x,y)) return ReaderControl::Chapter;
        if (settings::visible(config.progressVisibility,reading)
            && ui::contains(readerControlRect(ReaderControl::Footer),x,y)) return ReaderControl::Footer;
        if (ui::contains(readerControlRect(ReaderControl::Controls),x,y)) return ReaderControl::Controls;
        return ReaderControl::None;
    }

    void readerChrome(ui::Context& ui, const readerLayout::Chrome& view,
                      const settings::ReadingSettings& config, const Board::Power::BatteryState& battery) {
        using namespace ui::themes;
        ui.label(readerControlRect(ReaderControl::Chapter), settings::visible(config.chapterVisibility,view.reading)
                 ? view.chapter : std::string_view{},11,Muted,ui::TextAlign::Start,1,view.locale);
        ui.label(readerControlRect(ReaderControl::Focus),ui.focusBadge(),12,Accent,ui::TextAlign::Center);
        ui.battery({514,0,116,36},battery.status.percent,battery.charging,
                   settings::visible(config.batteryLabelVisibility,view.reading)?view.batteryLabel:std::string_view{},
                   settings::visible(config.batteryIconVisibility,view.reading));
        const auto handle=readerControlRect(ReaderControl::Controls);
        if(ui.redraw(handle,1)) ui.paint(handle,[&](Arduino_GFX& gfx,ui::Rect target){
            gfx.fillRoundRect(target.x+(target.w-36)/2,target.y+18,36,3,1,ui.blend(Muted,170));
        });
        ui.label({12,136,230,34},view.reading&&view.pausePending?std::string{ui.text(UiText::Pause)}+"…":std::string{},11,Muted);
        const bool progress=settings::visible(config.progressVisibility,view.reading);
        ui.label(readerControlRect(ReaderControl::Footer),progress?view.footer:std::string_view{},11,Muted,ui::TextAlign::Right);
        ui.progress({0,170,640,2},progress?view.percent:0);
    }
}
