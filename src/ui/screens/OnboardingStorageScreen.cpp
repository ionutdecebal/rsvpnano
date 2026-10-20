#include "ui/screens/OnboardingScreen.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"

namespace screens {
using State=OnboardingStorage::State;
using C=modern::Copy;
using ui::modern::button;
using ui::modern::Style;
using namespace ui::themes;

void OnboardingScreen::beginStorageCheck(uint32_t nowMs){
    pause();touching_=holding_=false;++storageRequest_;
    storage_={State::Checking,0};storageStartedMs_=nowMs;
    storageAttempted_=storageVisible_=true;storageStartPending_=false;
}
void OnboardingScreen::cancelStorageCheck(){
    ++storageRequest_;
    if(storageChecking())storage_.state=State::Cancelled;
    storageStartPending_=false;
}
void OnboardingScreen::acceptStorageProgress(uint32_t request,OnboardingStorage::Progress progress,uint32_t nowMs){
    update(nowMs);
    if(!active_||!storageVisible_||request!=storageRequest_||!storageChecking())return;
    storage_=progress;storage_.stage=std::min<uint8_t>(2,storage_.stage);
}
Action OnboardingScreen::storageCheck(ui::Context& ui,uint32_t nowMs){
    const auto t=[&](C key){return modern::copy(ui,key);};
    if(storageStartPending_&&!storageBusy_){beginStorageCheck(nowMs);return Action::CheckOnboardingStorage;}
    const bool busy=storageChecking(),ready=storage_.state==State::Ready;
    const bool failed=!busy&&!ready;
    if(button(ui,{8,0,44,43},"‹",Style::Plain,true,{},23)){next(-1);return Action::None;}
    ui.label({62,0,266,43},t(C::SdCheck),13);
    if(failed&&button(ui,{342,0,150,43},t(C::SdDemo),Style::Plain,true,{},13)){
        cancelStorageCheck();storageVisible_=false;step_=1;return Action::None;
    }
    const C nextLabel=ready?C::SdContinue:busy?C::SdWorking:storageBusy_?C::SdFinishing:C::SdRetry;
    if(button(ui,{500,4,132,36},t(nextLabel),Style::Primary,!busy&&!storageBusy_,{},13)){
        if(ready){storageVisible_=false;step_=1;return Action::None;}
        beginStorageCheck(nowMs);return Action::CheckOnboardingStorage;
    }
    ui::modern::rule(ui,{0,43,640,1});
    C title=C::SdTimeout,body=C::SdTimeoutBody;
    switch(storage_.state){
    case State::Checking:title=C::SdChecking;body=C::SdCheckingBody;break;
    case State::Ready:title=C::SdReady;body=C::SdReadyBody;break;
    case State::Unavailable:title=C::SdUnavailable;body=C::SdUnavailableBody;break;
    case State::FoldersFailed:title=C::SdFolders;body=C::SdFoldersBody;break;
    case State::SaveFailed:title=C::SdWrite;body=C::SdWriteBody;break;
    default:if(storageBusy_)body=C::SdBusyBody;break;
    }
    ui.label({16,51,310,28},t(title),21,Foreground,ui::TextAlign::Start,1);
    ui.label({16,84,310,51},t(body),13,Muted,ui::TextAlign::Start,3);
    if(failed)ui.label({16,140,310,30},t(C::SdDemoNote),11,Muted,ui::TextAlign::Start,2);
    ui::modern::rule(ui,{338,52,1,112});
    constexpr C labels[]{C::SdOpen,C::SdPrepare,C::SdSave};
    for(uint8_t i=0;i<3;++i){
        const bool passed=ready||i<storage_.stage,working=busy&&i==storage_.stage,error=failed&&i==storage_.stage;
        const C result=passed?C::SdPassed:working?C::SdWorking:error?(storage_.state==State::TimedOut?C::SdIncomplete:C::SdFailed):C::SdNotRun;
        const int16_t y=52+i*38;
        const ui::Rect marker{352,int16_t(y+9),20,20};
        const uint32_t signature=passed?1:error?2:working?3+(nowMs/200)%4:0;
        if(ui.redraw(marker,signature))ui.paint(marker,[&](Arduino_GFX& gfx,ui::Rect r){
            gfx.fillRect(r.x,r.y,r.w,r.h,ui.color(Background));
            const auto colour=ui.color(passed||error||working?Accent:Muted);
            gfx.drawCircle(r.x+9,r.y+9,8,colour);
            if(passed){gfx.drawLine(r.x+5,r.y+9,r.x+8,r.y+12,colour);gfx.drawLine(r.x+8,r.y+12,r.x+13,r.y+5,colour);}
            else if(error){gfx.drawFastVLine(r.x+9,r.y+4,7,colour);gfx.drawPixel(r.x+9,r.y+14,colour);}
            else if(working)gfx.fillCircle(r.x+6+int(signature-3)*2,r.y+9,2,colour);
        });
        ui.label({380,y,151,36},t(labels[i]),13,error?Accent:passed||working?Foreground:Muted,ui::TextAlign::Start,2);
        ui.label({536,y,88,36},t(result),11,Muted,ui::TextAlign::Right,2);
    }
    return Action::None;
}
}
