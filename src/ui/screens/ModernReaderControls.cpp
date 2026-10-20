#include "ui/screens/ReaderScreen.h"
#include "ui/screens/ModernReaderChrome.h"

namespace screens {
    bool ReaderScreen::handleModernControls(ui::Context& ui,uint32_t nowMs,Preferences& preferences,
                                           settings::SettingsStore& store,Screen& screen) {
        using modern::ReaderControl;
        if(pageBrowsing_ && ui.modernReadingPalette()) return handlePage(ui,nowMs,preferences,store,screen);
        if(!ui.modernReadingPalette() || ui.width()!=640 || ui.height()!=172
           || session.metadata.writingMode==WritingMode::verticalRl){cancelModernControls();return false;}
        const auto* touch=ui.touch();if(!touch)return false;
        if(ui::hasTouch(*touch,ui::TouchCancel)){
            if(modernHolding_)finishPause(preferences,nowMs);
            cancelModernControls();resetTouch();return true;
        }
        if(ui::hasTouch(*touch,ui::TouchStart)){
            resetTouch();lastTapValid_=false;
            modernControl_=static_cast<uint8_t>(modern::readerControlAt(touch->x,touch->y,settings_,session.playing,
                ReadingLoop::wordCount(session)>0,!ui.focusBadge().empty()));
            modernCanvasTouch_=modernControl_==0 && ui::contains({0,36,640,100},touch->x,touch->y);
            modernTouchX_=touch->x;modernTouchY_=touch->y;modernTouchMoved_=false;modernHolding_=false;
            modernPressedAtMs_=nowMs;modernPressedPlaying_=session.playing;
            return true;
        }
        const int dx=int(touch->x)-modernTouchX_,dy=int(touch->y)-modernTouchY_;
        modernTouchMoved_|=std::abs(dx)>18 || std::abs(dy)>18;
        const bool released=ui::hasTouch(*touch,ui::TouchRelease);
        if(modernCanvasTouch_){
            if(modernTouchMoved_ && modernHolding_){finishPause(preferences,nowMs);modernHolding_=false;}
            if(!released){
                if(!modernTouchMoved_ && settings_.holdToRead && !modernPressedPlaying_ && !modernHolding_
                   && nowMs-modernPressedAtMs_>=350){modernHolding_=true;start(nowMs,false);}
                return true;
            }
            const bool holding=modernHolding_;cancelModernControls();
            if(holding){finishPause(preferences,nowMs);return true;}
            if(dy < -30 && std::abs(dy)>std::abs(dx)*1.3){
                finishPause(preferences,nowMs);screen=Screen::QuickControls;ui.invalidate();return true;
            }
            if(!modernTouchMoved_ && (!settings_.holdToRead || modernPressedPlaying_)
               && session.playing==modernPressedPlaying_) toggle(preferences,nowMs);
            return true;
        }
        if(!released)return true;
        const auto control=static_cast<ReaderControl>(modernControl_);modernControl_=0;
        if(modernTouchMoved_ || control==ReaderControl::None
           || control!=modern::readerControlAt(touch->x,touch->y,settings_,session.playing,
                                               ReadingLoop::wordCount(session)>0,!ui.focusBadge().empty())) return true;
        switch(control){
        case ReaderControl::Controls:finishPause(preferences,nowMs);screen=Screen::QuickControls;ui.invalidate();break;
        case ReaderControl::Chapter:finishPause(preferences,nowMs);openLocator(screen);break;
        case ReaderControl::Focus:finishPause(preferences,nowMs);screen=Screen::FocusSession;break;
        case ReaderControl::Footer:
            if(!session.playing){settings_.footerMetric=settings::cycleEnum(settings_.footerMetric);store.acceptChanges();}break;
        default:break;
        }
        return true;
    }
}
