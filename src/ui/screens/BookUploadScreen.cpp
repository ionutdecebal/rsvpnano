#include "ui/screens/OnboardingScreen.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"
#include "ui/screens/CompanionQr.generated.h"

namespace screens {
void OnboardingScreen::books(ui::Context& ui){
    using C=modern::Copy;using ui::modern::button;using ui::modern::Style;using namespace ui::themes;
    const auto t=[&](C key){return modern::copy(ui,key);};
    const ui::Rect qr{16,53,111,111};
    if(ui.redraw(qr,1,true))ui.paint(qr,[&](Arduino_GFX& gfx,ui::Rect r){
        // Fixed contrast and a real quiet zone, independent of the selected theme.
        gfx.fillRect(r.x,r.y,r.w,r.h,0xffff);
        for(uint8_t y=0;y<companionqr::modules;++y)for(uint8_t x=0;x<companionqr::modules;){
            if(!(companionqr::rows[y]&(1u<<x))){++x;continue;}
            const auto start=x;while(x<companionqr::modules&&(companionqr::rows[y]&(1u<<x)))++x;
            gfx.fillRect(r.x+(start+companionqr::quiet)*companionqr::scale,
                         r.y+(y+companionqr::quiet)*companionqr::scale,(x-start)*companionqr::scale,companionqr::scale,0);
        }
    });
    if(button(ui,{147,53,66,34},"Wi-Fi",uploadUsb_?Style::Plain:Style::Accent,true,{},13))uploadUsb_=false;
    if(button(ui,{219,53,66,34},"USB",uploadUsb_?Style::Accent:Style::Plain,true,{},13))uploadUsb_=true;
    ui.label({295,53,329,34},"ionutdecebal.github.io/rsvpnano",11,Muted,ui::TextAlign::Right);
    const C keys[]{uploadUsb_?C::UploadUsbOne:C::UploadWifiOne,uploadUsb_?C::UploadUsbTwo:C::UploadWifiTwo,uploadUsb_?C::UploadUsbThree:C::UploadWifiThree};
    for(int i=0;i<3;++i){
        const int16_t y=91+i*22;
        ui.label({147,y,14,20},std::to_string(i+1),11,Accent);
        ui.label({168,y,456,int16_t(i==2?34:20)},t(keys[i]),13,Foreground,ui::TextAlign::Start,i==2?2:1);
    }
}
}
