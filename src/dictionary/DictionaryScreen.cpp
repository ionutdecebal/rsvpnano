#include "dictionary/Dictionary.h"
#include "ui/screens/ModernScreens.h"
#include "ui/SansFont.h"
#include "text/Utf8Text.h"
namespace dictionary {
namespace {
struct TextPage {std::array<std::string,4> lines;size_t end=0;};
TextPage page(std::string_view body,size_t offset,int width){
    TextPage result;result.end=std::min(offset,body.size());auto remaining=body.substr(result.end);
    for(auto& line:result.lines){
        if(remaining.empty())break;
        size_t end=0,space=0;auto rest=remaining;uint32_t cp=0;
        while(Utf8Text::next(rest,cp)){
            const auto next=remaining.size()-rest.size();
            if(cp=='\n')break;
            if(ui::sans::width(remaining.substr(0,next),ui::sans::font(15))>width)break;
            end=next;if(cp==' ')space=end;
        }
        if(end<remaining.size()&&space)end=space;
        if(!end)end=Utf8Text::prefixBytes(remaining,1);
        line=remaining.substr(0,end);remaining.remove_prefix(end);result.end+=end;
        while(!remaining.empty()&&(remaining.front()==' '||remaining.front()=='\n')){remaining.remove_prefix(1);++result.end;}
    }
    return result;
}
void pageLabels(ui::Context& ui,const TextPage& p,int x,int y,int width){
    for(size_t i=0;i<4;++i)ui.label({int16_t(x),int16_t(y+int(i)*21),int16_t(width),21},p.lines[i],15);
}
}
screens::Action Dictionary::draw(ui::Context& ui,ReadingSession& session,screens::Screen& screen,uint32_t) {
    using screens::Action;using screens::Screen;using ui::modern::button;using ui::modern::Style;
    if(screen==Screen::DictionarySearch){
        const auto result=ui.keyboard({0,0,640,172},draft_,100,keyboard_,"Dictionary search");
        if(result==ui::KeyboardAction::Cancel)screen=Screen::Dictionary;
        if(result==ui::KeyboardAction::Submit){query_=draft_;lookup();screen=Screen::Dictionary;}
        return Action::None;
    }
    if(button(ui,{8,0,102,44},"‹ Controls",Style::Plain)) {screen=Screen::QuickControls;return Action::None;}
    ui.label({120,0,180,44},"Dictionary",17);
    if(screen==Screen::Dictionary){
        if(button(ui,{314,0,44,44},"?",Style::Plain)){screen=Screen::DictionaryHelp;return Action::None;}
        if(button(ui,{370,0,122,44},"Search",Style::Secondary)){draft_=query_;screen=Screen::DictionarySearch;}
        if(button(ui,{504,0,128,44},"Look online",Style::Accent,!query_.empty()))return Action::OnlineDictionary;
        ui::modern::rule(ui,{0,43,640,1});
        ui.label({16,51,190,35},query_.empty()?"Choose a word":query_,20);
        ui.label({16,91,190,24},definition_.senses.empty()?"Offline first":"Meaning "+std::to_string(sense_+1)+" · page "+std::to_string(definitionOffsets_.size()),12,ui::themes::Muted);
        if(button(ui,{16,120,88,44},"‹ Word",Style::Secondary,lookupWord_>0,{},12)){--lookupWord_;query_=std::string(ReadingLoop::wordAt(session,lookupWord_));lookup();}
        if(button(ui,{114,120,92,44},"Word ›",Style::Secondary,lookupWord_+1<ReadingLoop::wordCount(session),{},12)){++lookupWord_;query_=std::string(ReadingLoop::wordAt(session,lookupWord_));lookup();}
        if(definition_.senses.empty())ui.label({226,54,398,104},message,16,ui::themes::Muted,ui::TextAlign::Start,4);
        else{
            sense_=std::min(sense_,definition_.senses.size()-1);const auto& sense=definition_.senses[sense_];
            const std::string body=sense.pos+" · "+sense.definition+(sense.example.empty()?"":" Example: "+sense.example);
            const auto text=page(body,definitionOffsets_.back(),338);pageLabels(ui,text,226,50,340);
            ui.label({226,140,342,24},message,10,ui::themes::Muted);
            if(button(ui,{578,50,50,50},"↑",Style::Secondary,sense_>0||definitionOffsets_.size()>1)){
                if(definitionOffsets_.size()>1)definitionOffsets_.pop_back();else{--sense_;definitionOffsets_={0};}
            }
            if(button(ui,{578,111,50,50},"↓",Style::Secondary,text.end<body.size()||sense_+1<definition_.senses.size())){
                if(text.end<body.size())definitionOffsets_.push_back(text.end);else{++sense_;definitionOffsets_={0};}
            }
        }
        return Action::None;
    }
    return Action::None;
}
}
