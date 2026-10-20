#include "ui/Modern.h"
#include "ui/PocketLines.h"
#include "ui/PocketLines.generated.h"
#include "ui/Logo.generated.h"

namespace {
void drawMask(ui::Context& ui,Arduino_GFX& gfx,int x,int y,int width,int height,
              std::span<const uint16_t> runs,ui::themes::ColorRole role,uint8_t opacity=255,bool flipped=false){
    uint16_t colors[16];for(int i=0;i<16;++i)colors[i]=ui.blend(role,i*17*opacity/255);
    int pixel=0;
    for(const auto run:runs){
        int count=run>>4;const int alpha=run&15;
        while(count){
            const int row=pixel/width,col=pixel%width,length=std::min(count,width-col);
            if(alpha)gfx.drawFastHLine(x+col,y+(flipped?height-1-row:row),length,colors[alpha]);
            pixel+=length;count-=length;
        }
    }
}
}

namespace ui::pocketlines {
void mark(Context& ui,Arduino_GFX& gfx,int16_t x,int16_t y,int size,bool flipped,themes::ColorRole role,uint8_t opacity){
    drawMask(ui,gfx,x,y,size,size,asset::mask(size),role,opacity,flipped);
}
void guides(Context& ui,Arduino_GFX& gfx,Rect bounds,int16_t anchor,int16_t textTop,int16_t textBottom,
            int size,int gap,themes::ColorRole role,uint8_t opacity){
    const int room=std::max(0,std::min(int(textTop-bounds.y),int(bounds.y+bounds.h-1-textBottom)));
    size=std::min(size,room);if(size<6)return;
    gap=std::min(gap,room-size);
    const int x=anchor-size/2;
    mark(ui,gfx,x,textTop-gap-size,size,false,role,opacity);
    mark(ui,gfx,x,textBottom+gap+1,size,true,role,opacity);
}
}
namespace ui::modern {
void logo(Context& ui,int16_t x,int16_t y,uint32_t nowMs){
    namespace art=ui::logo;
    const Rect rect{x,y,246,105};const uint32_t frame=(nowMs/80)%24;
    if(!ui.redraw(rect,frame,true))return;
    ui.paint(rect,[&](Arduino_GFX& gfx,Rect target){
        gfx.fillRect(target.x,target.y,target.w,target.h,ui.color(themes::Background));
        const int top=target.y+(target.h-art::height)/2;
        drawMask(ui,gfx,target.x,top,art::width,art::height,art::foreground,themes::Foreground);
        drawMask(ui,gfx,target.x,top,art::width,art::height,art::accent,themes::AccentBar,uint8_t(255-int(frame<12?frame:24-frame)*7));

    });
}
}
