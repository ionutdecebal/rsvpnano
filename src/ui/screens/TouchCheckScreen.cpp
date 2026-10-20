#include "ui/screens/TouchCheckScreen.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"
#include "ui/ClippedGfx.h"
namespace screens {
Action TouchCheckScreen::draw(ui::Context& ui,Screen& screen){
    using namespace ui::themes;using C=modern::Copy;
    const auto* touch=ui.touch();
    if(touch){
        down_=!ui::hasTouch(*touch,ui::TouchRelease);
        if(down_){
            x_=touch->x;y_=touch->y;minY_=std::min(minY_,y_);maxY_=std::max(maxY_,y_);
            if(y_>=44&&y_<172&&x_<640)seen_|=1u<<(std::min(5,((int(x_)+1)*6-1)/640)+std::min(2,((int(y_)-43)*3-1)/128)*6);
        }
    }
    if(ui::modern::button(ui,{8,0,94,43},"‹ "+std::string{modern::copy(ui,C::Display)},ui::modern::Style::Plain,true,{},13))screen=Screen::InterfaceSettings;
    ui.label({112,0,180,43},modern::copy(ui,C::TouchTest),15);
    ui.label({300,0,230,21},modern::copy(ui,C::TouchSlide),11,Muted);
    const auto coords=minY_<=maxY_?"X "+std::to_string(x_)+"  Y "+std::to_string(y_)+"  ["+std::to_string(minY_)+"–"+std::to_string(maxY_)+"]":"—";
    ui.label({300,21,230,22},coords,11,Accent);
    if(ui::modern::button(ui,{544,4,88,35},ui.text(UiText::Reset),ui::modern::Style::Secondary,true,{},13)){
        seen_=0;minY_=172;maxY_=0;down_=false;
    }
    for(int row=0;row<3;++row)for(int col=0;col<6;++col){
        const int index=row*6+col;const bool passed=seen_&(1u<<index);
        const ui::Rect rect{int16_t(col*640/6),int16_t(44+row*128/3),int16_t((col+1)*640/6-col*640/6),int16_t((row+1)*128/3-row*128/3)};
        const bool pointer=down_&&ui::contains(rect,x_,y_);
        auto signature=ui::Context::combine(passed,pointer?1+(uint32_t(x_)<<8)+y_:0);
        if(ui.redraw(rect,signature))ui.paint(rect,[&](Arduino_GFX& out,ui::Rect target){
            ui::ClippedGfx gfx(out,target);
            gfx.fillRect(target.x,target.y,target.w,target.h,ui.color(passed?SurfaceActive:SurfaceMuted));
            gfx.drawRect(target.x,target.y,target.w,target.h,ui.color(passed?Accent:Outline));
            if(pointer){const int x=x_+target.x-rect.x,y=y_+target.y-rect.y;
                gfx.drawFastHLine(x-7,y,15,ui.color(Foreground));gfx.drawFastVLine(x,y-7,15,ui.color(Foreground));}
        });
    }
    return Action::None;
}
}
