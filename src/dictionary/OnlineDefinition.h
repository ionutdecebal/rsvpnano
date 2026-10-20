#pragma once
#include <string>
#include <map>
#include <vector>
#include "text/Utf8Text.h"
namespace dictionary {
struct WikiDefinition{std::string definition;std::vector<std::string> examples;};
struct WikiPart{std::string partOfSpeech;std::vector<WikiDefinition> definitions;};
// Provider HTML is displayed as plain text. Ignore markup and decode the common
// entities used in definitions; never interpret a provider response as UI code.
inline std::string dictionaryText(std::string_view html){
 std::string out;bool tag=false;for(size_t i=0;i<html.size()&&out.size()<1800;i++){
  char c=html[i];if(c=='<'){tag=true;continue;}if(c=='>'){tag=false;continue;}if(tag)continue;
  if(c=='&'){size_t end=html.find(';',i+1);if(end!=std::string_view::npos&&end-i<16){auto entity=html.substr(i,end-i+1);std::string value;
   if(entity=="&amp;")value="&";else if(entity=="&lt;")value="<";else if(entity=="&gt;")value=">";else if(entity=="&quot;")value="\"";else if(entity=="&#39;"||entity=="&apos;")value="'";else if(entity=="&nbsp;")value=" ";
   if(!value.empty()){out+=value;i=end;continue;}
  }}out+=c;
 }return out;
}
}
