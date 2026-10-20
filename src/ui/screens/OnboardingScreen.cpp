#include "ui/PocketLines.h"
#include "ui/screens/OnboardingScreen.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"
#include "ui/SansFont.h"
#include "ui/ClippedGfx.h"
#include "text/Utf8Text.h"

namespace screens {
using C=modern::Copy;
using ui::modern::button;
using ui::modern::Style;
using namespace ui::themes;
namespace {
constexpr C titles[]{C::Welcome,C::Practice,C::TrayTitle,C::FindSpeed,C::FindPacing,C::EssentialMenu,C::PageLesson,C::EssentialSettings,C::Focus,C::PhysicalButtons,C::AddBooks,C::Dictionary};
void explanation(ui::Context& ui,C title,C body){
    ui.label({16,54,224,36},modern::copy(ui,title),21,Foreground,ui::TextAlign::Start,2);
    ui.label({16,94,224,70},modern::copy(ui,body),13,Muted,ui::TextAlign::Start,4);
}
}
void OnboardingScreen::loadSample(ui::Context& ui,uint32_t nowMs){
    sample_.words={};words_.clear();
    auto text=modern::copy(ui,C::TuningSample);
    while(!text.empty()){
        const auto end=text.find(' ');words_.emplace_back(text.substr(0,end));
        if(end==std::string_view::npos)break;text.remove_prefix(end+1);
    }
    ReadingLoop::setWords(sample_,words_,nowMs);sample_.metadata.locale=std::string{ui.locale()};
    selectedWord_=pageStart_=0;done_=false;
}
void OnboardingScreen::begin(ui::Context& ui,const settings::DeviceSettings& settings,uint32_t nowMs){
    close();draft_=settings.reading;draft_.mode=settings::ReadingMode::rsvp;
    step_=topic_=settingsTopic_=pageTool_=focusKind_=pacingHint_=0;button_=3;
    started_=paused_=controlsFound_=false;brightness_=settings.interface.brightnessPercent;
    storage_={};storageVisible_=storageAttempted_=storageBusy_=storageStartPending_=false;uploadUsb_=false;
    settingsChanged_=false;active_=true;loadSample(ui,nowMs);
}
void OnboardingScreen::close(){cancelStorageCheck();pause();active_=false;sample_=ReadingSession{};std::vector<std::string>{}.swap(words_);touching_=holding_=false;}
void OnboardingScreen::pause(){ReadingLoop::pause(sample_);if(started_&&step_==1)paused_=true;}
void OnboardingScreen::next(int delta){
    pause();touching_=holding_=false;
    if(storageVisible_){if(delta<0){cancelStorageCheck();storageVisible_=false;step_=0;}return;}
    if(step_==1&&delta==-1){storageVisible_=true;storageStartPending_=storage_.state==OnboardingStorage::State::Idle||storage_.state==OnboardingStorage::State::Cancelled;return;}
    step_=std::clamp<int>(step_+delta,0,11);done_=false;ReadingLoop::seekTo(sample_,0);
}
void OnboardingScreen::play(uint32_t nowMs,bool paced){
    if(!active_||storageVisible_||words_.empty())return;
    if(sample_.playing&&paced_==paced){pause();return;}
    timing_=draft_;timing_.mode=settings::ReadingMode::rsvp;
    if(!paced){timing_.pacing.longWordDelayMs=0;timing_.pacing.complexWordDelayMs=0;timing_.pacing.punctuationDelayMs=0;}
    paced_=paced;done_=false;started_=true;
    ReadingLoop::seekTo(sample_,0);ReadingLoop::start(sample_,nowMs);
}
void OnboardingScreen::hardwarePlay(uint32_t nowMs){if(step_==1||step_==3||step_==4)play(nowMs,step_==4);}
void OnboardingScreen::update(uint32_t nowMs){
    if(storageChecking()&&nowMs-storageStartedMs_>=OnboardingStorage::kTimeoutMs){
        ++storageRequest_;storage_.state=OnboardingStorage::State::TimedOut;
    }
    if(!active_||!sample_.playing)return;
    const auto duration=ReadingLoop::wordDurationMs(sample_,timing_,sample_.state.wordIndex);
    if(nowMs-sample_.lastAdvanceMs<duration)return;
    if(sample_.state.wordIndex+1>=words_.size()){pause();done_=true;return;}
    // Show every practice word for its real pacing duration; no catch-up skips.
    ReadingLoop::seekTo(sample_,sample_.state.wordIndex+1);sample_.lastAdvanceMs=nowMs;
}
void OnboardingScreen::touchPractice(ui::Context& ui,const settings::ReadingSettings& settings,uint32_t nowMs){
    const auto* touch=ui.touch();if(!touch)return;
    if(ui::hasTouch(*touch,ui::TouchCancel)){if(holding_)pause();touching_=holding_=false;return;}
    if(ui::hasTouch(*touch,ui::TouchStart)){
        touching_=ui::contains({266,44,374,128},touch->x,touch->y);
        touchX_=touch->x;touchY_=touch->y;pressedAt_=nowMs;moved_=holding_=false;pressedPlaying_=sample_.playing;return;
    }
    if(!touching_)return;
    const int dx=int(touch->x)-touchX_,dy=int(touch->y)-touchY_;
    moved_|=std::abs(dx)>18||std::abs(dy)>18;
    if(moved_&&holding_){pause();holding_=false;}
    if(ui::hasTouch(*touch,ui::TouchRelease)){
        touching_=false;
        if(step_!=1)return;
        if(holding_){pause();holding_=false;}
        else if(!moved_&&(!settings.holdToRead||pressedPlaying_)&&sample_.playing==pressedPlaying_)play(nowMs,false);
    }else if(step_==1&&!moved_&&settings.holdToRead&&!pressedPlaying_&&!holding_&&nowMs-pressedAt_>=350){
        holding_=true;play(nowMs,false);
    }
}
void OnboardingScreen::drawWord(ui::Context& ui,ui::Rect area){
    // The practice word uses the small UI face, with one stable focal anchor.
    // Actual book reading retains the selected RFont face and its own cache.
    const auto word=std::string_view{sample_.currentWord};
    if(!ui.redraw(area,ui::Context::signature(word)))return;
    const auto& font=ui::sans::font(28);
    size_t count=0;{auto rest=word;uint32_t cp;while(Utf8Text::next(rest,cp))++count;}
    const size_t focus=count<=1?0:count<=5?1:count<=9?2:3;
    auto rest=word;uint32_t cp;for(size_t i=0;i<focus;++i)Utf8Text::next(rest,cp);
    const size_t before=word.size()-rest.size();Utf8Text::next(rest,cp);const size_t after=word.size()-rest.size();
    const auto left=word.substr(0,before),letter=word.substr(before,after-before),right=word.substr(after);
    const int focalWidth=ui::sans::width(letter,font),leftWidth=ui::sans::width(left,font);
    ui.paint(area,[&](Arduino_GFX& out,ui::Rect r){
        ui::ClippedGfx gfx(out,r);const int anchor=r.x+r.w*35/100,baseline=r.y+(r.h+2*font.ascent-font.height)/2;
        const auto ink=ui.color(Foreground),bg=ui.color(Background);
        ui::sans::draw(gfx,left,font,anchor-focalWidth/2-leftWidth,baseline,ink,bg,r.x,r.y,r.w,r.h);
        ui::sans::draw(gfx,letter,font,anchor-focalWidth/2,baseline,ui.color(Accent),bg,r.x,r.y,r.w,r.h);
        ui::sans::draw(gfx,right,font,anchor+focalWidth/2,baseline,ink,bg,r.x,r.y,r.w,r.h);
        ui::pocketlines::guides(ui,gfx,r,anchor,baseline-font.ascent,baseline-font.ascent+font.height-1,12,4,Accent);
    });
}
Action OnboardingScreen::draw(ui::Context& ui,settings::DeviceSettings& settings,uint32_t nowMs){
    if(!active_)begin(ui,settings,nowMs);
    update(nowMs);
    if(storageVisible_)return storageCheck(ui,nowMs);
    const auto t=[&](C key){return modern::copy(ui,key);};
    if(step_==1)touchPractice(ui,settings.reading,nowMs);
    // The discovered panel fills the display just like the everyday controls.
    if(step_==2&&controlsFound_){
        if(button(ui,{16,8,104,72},t(C::Menu),Style::Secondary)){next(3);return Action::None;}
        if(button(ui,{128,8,104,72},t(C::Page),Style::Secondary)){next(4);return Action::None;}
        if(button(ui,{16,90,216,74},t(C::Dictionary),Style::Secondary)){next(9);return Action::None;}
        ui::modern::rule(ui,{246,8,1,156});
        ui.label({260,8,190,20},"2 / 11 · "+std::string{t(C::TrayTitle)},11,Muted);
        if(button(ui,{468,4,100,32},t(C::SkipTour),Style::Plain,true,{},11))return Action::SkipIntroduction;
        if(button(ui,{260,42,58,58},"−",Style::Secondary,true,{},26))draft_.wpm=std::max<int>(100,int(draft_.wpm)-10);
        ui.label({326,36,176,56},std::to_string(draft_.wpm),39,Foreground,ui::TextAlign::Center);
        ui.label({326,94,176,18},"WPM",11,Muted,ui::TextAlign::Center);
        if(button(ui,{510,42,58,58},"+",Style::Secondary,true,{},26))draft_.wpm=std::min<int>(1000,int(draft_.wpm)+10);
        if(button(ui,{260,120,48,44},"‹",Style::Secondary)){next(-1);return Action::None;}
        if(button(ui,{316,120,252,44},t(C::Next),Style::Primary)){next();return Action::None;}
        ui.verticalSlider({580,8,52,156},brightness_,5,100);return Action::None;
    }
    if(step_&&button(ui,{8,0,44,43},"‹",Style::Plain,true,{},23)){next(-1);return Action::None;}
    ui.label({int16_t(step_?62:16),0,int16_t(step_?270:320),43},step_?std::to_string(step_)+" / 11 · "+std::string{t(titles[step_])}:std::string{t(C::Welcome)},13);
    if(button(ui,{342,0,150,43},t(step_==4?C::KeepCurrent:C::SkipTour),Style::Plain,true,{},11)){
        if(step_==4){next();return Action::None;}return Action::SkipIntroduction;
    }
    const bool canNext=step_==1?paused_&&!sample_.playing:step_==2?controlsFound_:true;
    if(step_&&button(ui,{500,4,132,36},t(step_==11?C::OpenBook:step_==4?C::UseSettings:C::Next),Style::Primary,canNext,{},13)){
        if(step_==11)return Action::FinishIntroduction;
        if(step_==4){settings.reading.wpm=draft_.wpm;settings.reading.pacing=draft_.pacing;settingsChanged_=true;}
        next();return Action::None;
    }
    ui::modern::rule(ui,{0,43,640,1});
    if(step_==0){
        ui.label({16,50,310,68},t(C::WelcomeTitle),26,Foreground,ui::TextAlign::Start,2);
        ui.label({16,122,310,40},t(C::WelcomeBody),13,Muted,ui::TextAlign::Start,3);
        constexpr std::string_view languages[]{"en","de","ro"},names[]{"English","Deutsch","Română"};
        for(uint8_t i=0;i<3;++i)if(button(ui,{int16_t(350+i*94),54,88,42},names[i],settings.interface.locale==languages[i]?Style::Accent:Style::Secondary,true,{},13)){
            settings.interface.locale=languages[i];ui.setLocale(languages[i]);settingsChanged_=true;loadSample(ui,nowMs);
        }
        if(button(ui,{350,116,282,44},t(C::ShowMe),Style::Primary)){
            if(storageBusy_){waitForStorage();storageStartPending_=true;return Action::None;}
            beginStorageCheck(nowMs);return Action::CheckOnboardingStorage;
        }
    }else if(step_==1||step_==2){
        const bool hold=settings.reading.holdToRead;
        const C title=step_==2?C::SwipeTitle:sample_.playing?(hold?C::PauseHold:C::PauseTap):paused_?C::PausedTitle:hold?C::TryHold:C::TryTap;
        const C body=step_==2?C::SwipeBody:sample_.playing?C::SampleRunning:paused_?C::PausedBody:hold?C::TryHoldBody:C::TryTapBody;
        explanation(ui,title,body);ui::modern::rule(ui,{264,44,1,128});drawWord(ui,step_==2?ui::Rect{274,46,350,80}:ui::Rect{274,56,350,100});
        if(step_==2&&button(ui,{330,130,242,36},t(C::KeyboardAlternative),Style::Secondary,true,{},13)){hardwareControls();ui.invalidate();}
    }else if(step_==3){
        ui.label({12,49,240,18},t(C::BaseSpeed),11,Muted);
        bool changed=false;const bool wasPlaying=sample_.playing;
        if(button(ui,{12,71,48,44},"−",Style::Secondary,draft_.wpm>100,{},23)){draft_.wpm=std::max<int>(100,int(draft_.wpm)-10);changed=true;}
        ui.label({65,71,137,44},std::to_string(draft_.wpm)+" WPM",26,Foreground,ui::TextAlign::Center);
        if(button(ui,{208,71,48,44},"+",Style::Secondary,draft_.wpm<1000,{},23)){draft_.wpm=std::min<int>(1000,int(draft_.wpm)+10);changed=true;}
        if(changed){pause();if(wasPlaying)play(nowMs,false);}
        ui.label({12,115,244,51},t(C::SpeedAdvice),13,Muted,ui::TextAlign::Start,3);
        ui::modern::rule(ui,{266,50,1,116});drawWord(ui,{282,50,346,52});
        ui.label({282,104,346,18},t(done_?C::SampleFinished:C::BaseOnly),11,Muted,ui::TextAlign::Center);
        if(button(ui,{282,126,346,40},t(sample_.playing?C::StopSample:done_?C::ReplaySample:C::PlaySample),Style::Primary))play(nowMs,false);
    }else if(step_==4){
        const auto* touch=ui.touch();
        if(touch&&ui::hasTouch(*touch,ui::TouchStart)&&ui::contains({12,50,270,116},touch->x,touch->y)){
            pause();pacingHint_=std::min<int>(3,1+(touch->y-50)/40);
        }
        ui.slider({12,50,270,36},t(C::LongWords),draft_.pacing.longWordDelayMs," ms");
        ui.slider({12,90,270,36},t(C::ComplexWords),draft_.pacing.complexWordDelayMs," ms");
        ui.slider({12,130,270,36},t(C::Punctuation),draft_.pacing.punctuationDelayMs," ms");
        drawWord(ui,{296,50,332,36});
        constexpr C hints[]{C::PacingAdvice,C::LongDelayAdvice,C::ComplexDelayAdvice,C::PunctuationDelayAdvice};
        ui.label({296,89,332,35},t(hints[pacingHint_]),11,Muted,ui::TextAlign::Start,3);
        if(button(ui,{296,128,162,38},t(C::WithPauses),sample_.playing&&paced_?Style::Accent:Style::Secondary,true,{},13))play(nowMs,true);
        if(button(ui,{466,128,162,38},t(C::WithoutPauses),sample_.playing&&!paced_?Style::Accent:Style::Secondary,true,{},13))play(nowMs,false);
    }else if(step_==5||step_==7)explorer(ui,step_==7);
    else if(step_==6)pageLesson(ui);
    else if(step_==8){
        explanation(ui,focusKind_?C::TimerOnly:C::ReadFocus,focusKind_?C::TimerOnlyExplain:C::ReadFocusExplain);
        if(button(ui,{264,52,176,54},t(C::ReadFocus),focusKind_==0?Style::Accent:Style::Secondary))focusKind_=0;
        if(button(ui,{448,52,176,54},t(C::TimerOnly),focusKind_==1?Style::Accent:Style::Secondary))focusKind_=1;
        ui.label({264,116,360,46},t(C::FocusPresetExplain),13,Muted,ui::TextAlign::Start,3);
    }else if(step_==9)buttons(ui);
    else if(step_==10)books(ui);
    else if(step_==11){
        explanation(ui,C::Dictionary,C::DictionaryLesson);
        ui.label({268,51,356,24},t(C::DictionaryOffline),16,Accent);
        ui.label({268,78,356,43},t(C::DictionaryOfflineBody),12,Muted,ui::TextAlign::Start,3);
        ui.label({268,121,356,20},t(C::DictionaryOnline),14,Accent);
        ui.label({268,142,356,29},t(C::DictionaryOnlineBody),11,Muted,ui::TextAlign::Start,2);
    }
    return Action::None;
}
void OnboardingScreen::explorer(ui::Context& ui,bool settings){
    constexpr C menuTitles[]{C::Library,C::Chapters,C::Focus,C::Settings,C::BackBook};
    constexpr C menuBody[]{C::MenuLibrary,C::MenuChapters,C::MenuFocus,C::MenuSettings,C::MenuBook};
    constexpr C settingTitles[]{C::Reading,C::Appearance,C::Display,C::Connections,C::Device,C::Help};
    constexpr C settingBody[]{C::ReadingExplain,C::AppearanceExplain,C::DisplayExplain,C::ConnectionsExplain,C::DeviceExplain,C::HelpExplain};
    const auto* titles=settings?settingTitles:menuTitles;const auto* bodies=settings?settingBody:menuBody;
    auto& selected=settings?settingsTopic_:topic_;explanation(ui,titles[selected],bodies[selected]);
    for(uint8_t i=0;i<(settings?6:5);++i){
        const ui::Rect rect{int16_t(256+i%3*126),int16_t(52+i/3*58),int16_t(!settings&&i==4?244:118),52};
        if(button(ui,rect,modern::copy(ui,titles[i]),selected==i?Style::Accent:Style::Secondary,true,{},13))selected=i;
    }
}
void OnboardingScreen::setPageTool(ui::Context& ui,uint8_t tool){pageTool_=tool;touching_=false;ui.invalidate();}
void OnboardingScreen::pageLesson(ui::Context& ui){
    const auto t=[&](C key){return modern::copy(ui,key);};
    constexpr C titles[]{C::Page,C::Find,C::Search,C::Chapters,C::Position,C::SelectedWord};
    constexpr C bodies[]{C::BrowseExplain,C::FindExplain,C::SearchExplain,C::MenuChapters,C::PositionExplain,C::DemoBookReturn};
    explanation(ui,titles[pageTool_],bodies[pageTool_]);
    if(pageTool_==0){
        if(button(ui,{256,50,150,40},t(C::Find),Style::Secondary,true,{},13)){setPageTool(ui,1);return;}
        if(button(ui,{414,50,214,40},t(C::BackBook),Style::Accent,true,{},13)){setPageTool(ui,5);return;}
        const auto* touch=ui.touch();
        if(touch&&ui::hasTouch(*touch,ui::TouchStart)){
            touching_=ui::contains({256,94,372,72},touch->x,touch->y);touchX_=touch->x;touchY_=touch->y;
        }
        if(touch&&ui::hasTouch(*touch,ui::TouchCancel))touching_=false;
        bool tap=false;
        if(touch&&touching_&&ui::hasTouch(*touch,ui::TouchRelease)){
            const int dy=int(touch->y)-touchY_,dx=int(touch->x)-touchX_;touching_=false;
            if(std::abs(dy)>20){pageStart_=std::clamp<int>(int(pageStart_)+(dy<0?6:-6),0,int(words_.size()>6?words_.size()-6:0));ui.invalidate();}
            else tap=std::abs(dx)<18;
        }
        const auto& font=ui::sans::font(15);
        const int space=ui::sans::width(" ",font);
        int x=260,y=96;
        for(size_t index=pageStart_;index<words_.size();++index){
            const int width=ui::sans::width(words_[index],font);
            if(x>260&&x+width>624){x=260;y+=24;}
            if(y+22>168)break;
            const ui::Rect target{int16_t(x-space/2),int16_t(y),int16_t(width+space),24};
            if(tap&&ui::contains(target,touch->x,touch->y))selectedWord_=index;
            ui.label({int16_t(x),int16_t(y),int16_t(width),24},words_[index],15,index==selectedWord_?Accent:Foreground);
            x+=width+space;
        }
    }else{
        if(pageTool_==5)ui.label({264,54,356,62},words_[selectedWord_],28,Foreground,ui::TextAlign::Center);
        else for(uint8_t i=0;i<3;++i)if(button(ui,{int16_t(256+i*126),52,118,62},t(titles[i+2]),pageTool_==i+2?Style::Accent:Style::Secondary,true,{},13))setPageTool(ui,i+2);
        if(button(ui,{256,124,372,40},"‹ "+std::string{t(C::Page)},Style::Plain,true,{},13))setPageTool(ui,0);
    }
}
void OnboardingScreen::buttons(ui::Context& ui){
    const auto t=[&](C key){return modern::copy(ui,key);};
    // Front silhouette: 640:172 display aspect, with the three top-right buttons.
    const ui::Rect area{12,44,370,128};
    if(ui.redraw(area,button_))ui.paint(area,[&](Arduino_GFX& gfx,ui::Rect r){
        const auto edge=ui.color(Outline),ink=ui.color(Muted),active=ui.color(Accent);
        gfx.drawRoundRect(r.x+5,r.y+16,300,106,9,edge);
        gfx.drawRoundRect(r.x+27,r.y+34,256,69,3,edge);
        gfx.drawRoundRect(r.x+305,r.y+61,22,15,5,ink);
        for(int i=0;i<3;++i){const int x=r.x+218+i*25;
            gfx.fillRoundRect(x,r.y+11,14,6,2,button_==i+1?active:ink);
            gfx.drawCircle(x+7,r.y+5,4,button_==i+1?active:ink);
        }
    });
    ui.label({40,92,244,38},"Nano",26,Muted,ui::TextAlign::Center);
    ui.label({334,126,44,20},"USB",11,Muted,ui::TextAlign::Center);
    constexpr C positions[]{C::LeftThree,C::MiddleThree,C::RightThree},actions[]{C::RestartDevice,C::KeyboardAlternative,C::PlayPause};
    for(uint8_t i=0;i<3;++i)if(button(ui,{392,int16_t(47+i*41),236,39},t(positions[i]),button_==i+1?Style::Accent:Style::Plain,true,t(actions[i]),13,ui::TextAlign::Start))button_=i+1;
}
Action OnboardingScreen::help(ui::Context& ui,const settings::ReadingSettings& reading,Screen& screen){
    const auto t=[&](C key){return modern::copy(ui,key);};
    const auto action=modern::header(ui,t(C::Help),screen,Screen::Settings);if(action!=Action::None)return action;
    ui.label({16,54,238,48},t(reading.holdToRead?C::SwipeHelpHold:C::TouchHelpTap),13,Foreground,ui::TextAlign::Start,3);
    ui.label({16,109,238,52},t(C::SwipeHelp),13,Muted,ui::TextAlign::Start,3);
    if(button(ui,{276,52,348,52},t(C::AddBooks),Style::Secondary))screen=Screen::AddBooks;
    if(button(ui,{276,112,170,52},t(C::LocateButtons),Style::Secondary,true,{},13))screen=Screen::ButtonMap;
    if(button(ui,{454,112,170,52},t(C::ReplayIntro),Style::Secondary,true,{},13))return Action::ReplayIntroduction;
    return Action::None;
}
}
