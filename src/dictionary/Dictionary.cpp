#include "dictionary/Dictionary.h"
#include "dictionary/DictionaryPack.h"
#include "dictionary/DictionaryInstaller.h"
#include "board/BoardStorage.h"
#include "text/Utf8Text.h"
#include "text/UnicodeText.h"
#include <glaze/glaze.hpp>
#include <algorithm>
namespace dictionary {
namespace {
constexpr glz::opts jsonOptions{.error_on_unknown_keys=false};
std::string normalized(std::string_view word){
    std::string result;bool begun=false;std::string_view rest=word;size_t end=0;
    while(!rest.empty()){
        const auto before=rest;uint32_t cp=0;Utf8Text::next(rest,cp);
        const bool letter=UnicodeText::isWordCharacter(cp);
        if(!begun&&!letter)continue;
        begun=true;
        if(cp>='A'&&cp<='Z')result+=char(cp+32);else result+=before.substr(0,before.size()-rest.size());
        if(letter)end=result.size();
    }
    size_t limit=std::min<size_t>(end,100);
    while(limit<result.size() && limit && (static_cast<unsigned char>(result[limit])&0xC0)==0x80)--limit;
    result.resize(limit);return result;
}
}
void Dictionary::open(ReadingSession& session) {
    lookupWord_=session.state.wordIndex;
    query_=normalized(ReadingLoop::wordAt(session,lookupWord_));
    lookup();
}
void Dictionary::lookup(){
    query_=normalized(query_);
    definition_={};sense_=0;definitionOffsets_={0};
    if (!recoverInstall()) { message="SD dictionary recovery needed; check the card"; return; }
    if(query_.empty()){message="Open a book or search for a word";return;}
    auto pack=Board::Storage::filesystem().open("/dictionaries/english.oewn",FILE_READ);
    if(!pack){message="Install english.oewn in /dictionaries on the SD card";return;}
    std::string result;
    const bool found=findDefinition([&](uint32_t offset,uint8_t* dest,size_t count){return pack.seek(offset)&&pack.read(dest,count)==count;},pack.size(),normalized(query_),result);
    if(!found){message="No offline entry · try Search or Look online";return;}
    if(glz::read<jsonOptions>(definition_,result)){message="Dictionary entry could not be read";definition_={};return;}
    message="Open English Wordnet · CC BY 4.0";
}
}
