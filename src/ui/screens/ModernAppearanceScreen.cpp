#include "ui/screens/ReaderScreen.h"
#include "ui/screens/ReaderLayout.h"
#include "ui/ClippedGfx.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"

namespace screens {
namespace {
int informationPreset(const settings::ReadingSettings& c,bool reading){
    const bool battery=settings::visible(c.batteryIconVisibility,reading)&&settings::visible(c.batteryLabelVisibility,reading);
    const bool chapter=settings::visible(c.chapterVisibility,reading),progress=settings::visible(c.progressVisibility,reading);
    if(!battery&&!chapter&&!progress && !settings::visible(c.batteryIconVisibility,reading)&&!settings::visible(c.batteryLabelVisibility,reading))return 0;
    if(battery&&progress)return chapter?2:1;
    return 3; // Preserve older per-element choices until a preset is explicitly selected.
}
void applyInformation(settings::ReadingSettings& c,bool reading,int level){
    const auto set=[&](settings::Visibility& field,bool shown){if(settings::visible(field,reading)!=shown)settings::toggleVisibility(field,reading);};
    set(c.batteryIconVisibility,level>0);set(c.batteryLabelVisibility,level>0);set(c.progressVisibility,level>0);set(c.chapterVisibility,level==2);
}
}
bool ReaderScreen::modernAppearance(ui::Context& ui,Screen& screen){
    using namespace ui::themes;using ui::modern::button;using ui::modern::Style;using C=modern::Copy;
    const auto t=[&](C key){return modern::copy(ui,key);};
    auto& config=settings_;auto& type=config.typography;
    const bool chunks=config.mode==settings::ReadingMode::chunks,onScreen=appearancePage_==2,reading=appearanceDialPage_==0;
    bool changed=false;
    if(button(ui,{8,0,100,43},"‹ "+std::string{ui.text(UiText::Settings)},Style::Plain,true,{},13))screen=Screen::Settings;
    ui.label({120,0,310,43},t(C::Appearance),15);
    if(button(ui,{480,0,152,43},std::string{t(C::Try)}+" ↗",Style::Accent,true,{},13))screen=Screen::Reader;
    ui::modern::rule(ui,{0,43,640,1});
    constexpr C tabs[]{C::Text,C::Guides,C::OnScreen};
    for(uint8_t i=0;i<3;++i){
        if(button(ui,{6,int16_t(48+i*41),94,39},t(tabs[i]),appearancePage_==i?Style::Secondary:Style::Plain,true,{},13)){
            appearancePage_=i;ui.invalidate();return false;
        }
    }
    ui::modern::rule(ui,{103,44,1,128});ui::modern::rule(ui,{366,44,1,128});
        // This is the device's actual RFont4 face and metrics, not browser text.
        constexpr std::string_view word = "reading", before = "the", after = "on";
        const ui::Rect preview = onScreen ? ui::Rect{110,75,246,57} : ui::Rect{110,51,246,100};
        text_.prepare(word);
        const auto& raster = face_.raster.get();
        const int16_t inkHeight = raster.wordInkBottom - raster.wordInkTop + 1;
        const int16_t baseline = preview.y + (preview.h - inkHeight) / 2 - raster.wordInkTop;
        const int16_t anchor = preview.x + (chunks ? preview.w / 2 : preview.w * type.anchor / 100);
        const int focus = focusOffset(word);
        const int16_t wordWidth = text_.textAdvance(word, type.tracking);
        const int16_t focusCenter = chunks ? wordWidth / 2 : wordAnchorOffset(word);
        const int16_t wordX = anchor - focusCenter;
        const int16_t beforeWidth = text_.textAdvance(before, type.tracking);
        const int16_t afterWidth = text_.textAdvance(after, type.tracking);
        const int16_t beforeX = wordX - 24 - beforeWidth, afterX = wordX + wordWidth + 24;
        auto state = ui::Context::signature(type.fontId);
        for (int value : {int(type.fontSizeIndex), int(type.tracking), int(type.anchor), int(type.guideSize),
                          int(type.guideTextGap), int(type.focusHighlight), int(config.mode), int(config.phantomWords), int(appearancePage_),
                          int(config.chapterVisibility), int(config.progressVisibility), int(config.batteryIconVisibility),
                          int(config.batteryLabelVisibility), int(config.arrowsVisibility)})
            state = ui::Context::combine(state, value);
        if (ui.redraw(preview, state)) {
            ui.paint(preview, [&](Arduino_GFX& output, ui::Rect translated) {
                ui::ClippedGfx clipped(output, translated);
                Arduino_GFX& previous = text_.setOutput(clipped);
                const int16_t dx = translated.x - preview.x, dy = translated.y - preview.y;
                drawGuides(ui, clipped, anchor + dx, baseline + dy, translated);
                drawWord(word, wordX + dx, baseline + dy, preview.y + preview.h / 2 + dy, focus, false, ui);
                text_.setTextColor(ui.blend(Foreground, config.phantomWords ? 64 : 28), ui.color(Background));
                if (!chunks && beforeX >= preview.x)
                    text_.drawString(before, beforeX + dx, baseline + dy, type.tracking);
                if (!chunks && afterX + afterWidth <= preview.x + preview.w)
                    text_.drawString(after, afterX + dx, baseline + dy, type.tracking);
                text_.setOutput(previous);
            });
        }
        if(onScreen){
            const bool chapter=settings::visible(config.chapterVisibility,reading),battery=settings::visible(config.batteryLabelVisibility,reading),progress=settings::visible(config.progressVisibility,reading);
            ui.label({112,49,158,22},chapter?ui.text(UiText::Chapter):std::string_view{},11,Muted);
            ui.label({278,49,78,22},battery?"84%":"",11,Muted,ui::TextAlign::Right);
            ui.label({280,135,76,23},progress?"42%":"",11,Muted,ui::TextAlign::Right);
            if(ui.redraw({217,148,32,3},1))ui.paint({217,148,32,3},[&](Arduino_GFX& gfx,ui::Rect r){gfx.fillRoundRect(r.x,r.y,r.w,r.h,1,ui.color(Muted));});
            constexpr C presetNames[]{C::Quiet,C::Essentials,C::Detailed};
            for(uint8_t i=0;i<2;++i){
                const int preset=informationPreset(config,i==0);
                if(button(ui,{378,int16_t(50+i*51),250,46},t(i==0?C::WhileReading:C::WhilePaused),appearanceDialPage_==i?Style::Accent:Style::Secondary,true,
                          preset<3?t(presetNames[preset]):std::string_view{"Custom"},13,ui::TextAlign::Start)){
                    appearanceDialPage_=i;applyInformation(config,i==0,(preset+1)%3);changed=true;
                    ui.invalidate();
                }
            }
            ui.label({378,153,250,18},t(reading?C::PreviewReading:C::PreviewPaused),11,Muted);
        }else if(appearancePage_==0){
            changed|=ui.slider({378,50,250,33},t(C::Tracking),type.tracking);
            if(chunks)ui.label({378,87,250,33},t(C::Centred),13,Muted,ui::TextAlign::Center);
            else changed|=ui.slider({378,87,250,33},t(C::Anchor),type.anchor,"%");
            const auto families=fonts.families();const auto found=std::ranges::find(families,type.fontId,&FontCatalog::Family::id);
            const size_t family=found==families.end()?0:found-families.begin();
            if(button(ui,{378,124,121,44},families.empty()?"Font":families[family].label,Style::Secondary,!families.empty(),{},13)){
                type.fontId=families[(family+1)%families.size()].id;changed=true;
            }
            if(button(ui,{507,124,121,44},RFont4::sizeLabel(type.fontSizeIndex),Style::Secondary,true,{},13)){type.fontSizeIndex.cycle();changed=true;}
        }else{
            if(chunks)ui.label({378,50,250,70},t(C::GuidesUnavailableChunks),13,Muted,ui::TextAlign::Start,3);
            else{
                changed|=ui.slider({378,50,250,33},t(C::GuideSize),type.guideSize," px");
                changed|=ui.slider({378,87,250,33},t(C::TextGap),type.guideTextGap," px");
            }
            if(button(ui,{378,124,120,44},t(C::Phantoms),Style::Secondary,!chunks,t(chunks?C::Off:config.phantomWords?C::On:C::Off),11)){
                config.phantomWords=!config.phantomWords;changed=true;
            }
            if(button(ui,{506,124,122,44},t(C::Focal),Style::Secondary,!chunks,t(chunks?C::Off:type.focusHighlight?C::On:C::Off),11)){
                type.focusHighlight=!type.focusHighlight;changed=true;
            }
        }
        return changed;
    }
}
