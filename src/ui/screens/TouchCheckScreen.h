#pragma once
#include "ui/screens/Screens.h"
namespace screens {
class TouchCheckScreen {
public:
    Action draw(ui::Context& ui, Screen& screen);
private:
    uint32_t seen_=0;
    uint16_t x_=0,y_=0,minY_=172,maxY_=0;
    bool down_=false;
};
}
