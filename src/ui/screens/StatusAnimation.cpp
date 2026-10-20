#include "ui/screens/Screens.h"
namespace screens {
namespace { std::string title_,line1_,line2_;int progress_=-1;uint32_t tick_=0;bool replaying_=false; }
void rememberStatus(std::string_view title,std::string_view line1,std::string_view line2,int progress) {
    if(replaying_) return;
    title_=title;line1_=line1;line2_=line2;progress_=progress;
}
void animateStatus(ui::Context& ui,uint32_t nowMs) {
    if(ui.width()!=640 || ui.height()!=172 || nowMs-tick_<80) return;
    tick_=nowMs;replaying_=true;status(ui,title_,line1_,line2_,progress_);replaying_=false;
}
}
