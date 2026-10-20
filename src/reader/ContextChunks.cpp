#include "reader/ContextChunks.h"
#include <algorithm>
#include <array>
#include "text/Utf8Text.h"
namespace ContextChunks { namespace {
constexpr std::string_view articles = " a an the this that these those my your his her its our their some any each every another no ";
constexpr std::string_view prepositions = " in on at by for from to into onto through under over beneath beyond beside behind between among across around against of with without towards toward until before after ";
constexpr std::string_view conjunctions = " and but or nor so yet because although while unless if when where ";
constexpr std::string_view pronouns = " i you he she it we they someone something everyone everything nobody nothing there ";
constexpr std::string_view auxiliaries = " am is are was were be been being have has had do does did can could will would shall should may might must ";
constexpr std::string_view adjectives = " blue red green dark bright small little quiet open old new tall warm cold long short narrow last first empty brass wooden folded broad young useful real soft pale distant late same other confident previous single ";
constexpr std::string_view verbs = " found find put take took opened open closed close left leave stood stand sat sit looked look watched watch smiled smile turned turn went go began begin read wrote write brought bring set get got gave give grew grow knew know heard hear saw see made make kept keep felt feel lay lie caught catch slept sleep came come ran run lost lose forgot remember tell told said say see let wore wear hung hang meant mean ";
bool has(std::string_view list, std::string_view word) {
    if (word.empty()) return false;
    size_t at = 0;
    while ((at = list.find(word, at)) != std::string_view::npos) {
        if (at > 0 && list[at-1] == ' ' && at+word.size() < list.size() && list[at+word.size()] == ' ') return true;
        ++at;
    }
    return false;
}
std::string clean(std::string_view word) {
    std::string result;
    for (unsigned char c : word) {
        if (c >= 'A' && c <= 'Z') result += char(c + ('a'-'A'));
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'') result += char(c);
    }
    return result;
}
bool verb(std::string_view w) { return has(verbs,w) || w.ends_with("ed") || w.ends_with("ing"); }
bool adjective(std::string_view w) {
    if (has(adjectives,w)) return true;
    for (auto suffix : {"ous","ful","less","ive","able","ible","ish"}) if (w.ends_with(suffix)) return true;
    return false;
}
bool content(std::string_view w) {
    return !w.empty() && !has(articles,w) && !has(prepositions,w) && !has(conjunctions,w)
        && !has(pronouns,w) && !has(auxiliaries,w);
}
int bond(std::string_view left, std::string_view right, std::string_view previous = {}) {
    if (boundary(left)) return -12;
    const auto a=clean(left), b=clean(right);
    if (has(articles,a)) return 9;
    if (b == "can" && has(" watering tin spray paint soup ",a)) return 9;
    if (has(auxiliaries,a)) return 7;
    if (a == "not" || a == "never") return 6;
    if (has(pronouns,a) && (verb(b) || has(auxiliaries,b))) return 7;
    if (has(prepositions,a)) return 5;
    if (has(conjunctions,a)) return 4;
    if (has(articles,clean(previous)) && content(a) && content(b) && !verb(b)) return 9;
    if (adjective(a) && content(b)) return 7;
    if (has(prepositions,b) || has(conjunctions,b) || has(articles,b) || has(pronouns,b)) return -6;
    return 0;
}
int score(std::span<const std::string> words,size_t start,size_t end) {
    const size_t n=end-start;
    int value=n==1?-4:n==2?2:3;
    for(size_t i=start;i+1<end;++i) value+=bond(words[i],words[i+1],i?words[i-1]:std::string_view{});
    if(end<words.size() && !boundary(words[end-1]))
        value-=std::max(0,bond(words[end-1],words[end],end>=2?words[end-2]:std::string_view{}));
    const auto last=clean(words[end-1]);
    if(!boundary(words[end-1]) && (has(articles,last)||has(prepositions,last)||has(auxiliaries,last)
        ||has(conjunctions,last)||last=="not"||last=="never")) value-=n==1?4:10;
    if(boundary(words[end-1])) value+=4;
    return value;
}
} // namespace
bool boundary(std::string_view word) {
    const auto bare=clean(word);
    if(word.ends_with(".") && has(" mr mrs ms dr prof st vs ",bare)) return false;
    while(!word.empty()) {
        const size_t at=Utf8Text::lastCodepointStart(word);
        auto tail=word.substr(at);uint32_t cp=0;Utf8Text::next(tail,cp);
        word=word.substr(0,at);
        if(cp=='\''||cp=='"'||cp==')'||cp==']'||cp=='}'||cp==0x2019||cp==0x201d||cp==0xbb) continue;
        return cp=='.'||cp==','||cp=='!'||cp=='?'||cp==';'||cp==':'||cp==0x2026||cp==0x2013||cp==0x2014;
    }
    return false;
}
bool englishToken(std::string_view word) {
    if(word.size()>kMaximumWordBytes) return false;
    uint32_t cp=0;
    while(Utf8Text::next(word,cp)) {
        if(cp<128 || (cp>=0x2010 && cp<=0x2026) || cp==0xab || cp==0xbb) continue;
        return false;
    }
    return true;
}
uint8_t count(std::span<const std::string> words,const Fits& fits) {
    words=words.first(std::min(words.size(),kLookahead));
    if(words.empty()) return 0;
    std::array<int,kLookahead+1> costs;costs.fill(-100000);costs[words.size()]=0;
    std::array<uint8_t,kLookahead> next{};
    for(size_t start=words.size();start-->0;) {
        std::string text;
        for(size_t n=1;n<=3 && start+n<=words.size();++n) {
            const size_t end=start+n;
            if(n>1)text+=' ';
            text+=words[end-1];
            if(n>1 && (boundary(words[end-2]) || !fits(text))) break;
            const int value=score(words,start,end)+costs[end];
            if(value>costs[start]) { costs[start]=value;next[start]=static_cast<uint8_t>(n); }
        }
    }
    return next[0]?next[0]:1;
}
} // namespace ContextChunks
