#include "dictionary/Dictionary.h"
#include "dictionary/OnlineDefinition.h"
#include "dictionary/RootCa.h"
#include "network/WifiConnection.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <glaze/glaze.hpp>
#include <time.h>
namespace dictionary {
namespace {
constexpr glz::opts jsonOptions{.error_on_unknown_keys=false};
std::string encode(std::string_view value){constexpr char hex[]="0123456789ABCDEF";std::string s;for(const uint8_t c:value){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')s+=char(c);else{s+='%';s+=hex[c>>4];s+=hex[c&15];}}return s;}
class Sink:public Stream {
public:
    Sink(std::string& text):text_(text){}
    size_t write(uint8_t c)override{return write(&c,1);}
    size_t write(const uint8_t* p,size_t n)override{
        if(total_+n>65536){failed=true;return 0;}
        total_+=n;
        text_.append(reinterpret_cast<const char*>(p),n);return n;
    }
    int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}
    bool failed=false;
private:std::string& text_;size_t total_=0;
};
}
void Dictionary::lookupOnline(settings::SettingsStore& settings) {
    if(query_.empty()){message="Search for a word first";return;}
    const auto& ssid=settings.settings().network.ssid;
    if(ssid.empty()){message="Set up Wi-Fi under Connections first";return;}
    const bool wasConnected=WiFi.status()==WL_CONNECTED;
    if(!wasConnected&&!net::connectSaved(settings.settings().network,settings.secrets())){message="Wi-Fi unavailable · offline dictionary still works";net::disconnect();return;}
    if(time(nullptr)<1700000000){
        configTime(0,0,"pool.ntp.org","time.cloudflare.com");const uint32_t start=millis();
        while(time(nullptr)<1700000000&&millis()-start<10000)delay(100);
    }
    if(time(nullptr)<1700000000){message="Clock not synced · cannot verify server certificate";if(!wasConnected)net::disconnect();return;}
    {
        WiFiClientSecure client;client.setCACert(kRootCa);client.setHandshakeTimeout(12);
        HTTPClient http;http.setConnectTimeout(10000);http.setTimeout(15000);http.setReuse(false);
        const auto url="https://en.wiktionary.org/api/rest_v1/page/definition/"+encode(query_);
        std::string reply;
        if(!http.begin(client,url.c_str()))message="Could not open dictionary connection";
        else{
            http.addHeader("User-Agent","RSVPNano/0.2 (https://github.com/ionutdecebal/rsvpnano)");
            const int code=http.GET();Sink sink(reply);const int count=code>0?http.writeToStream(&sink):-1;http.end();
            if(code==404)message="No online entry found";
            else if(code!=200||count<0||sink.failed)message="Dictionary unavailable · try again later";
            else{
                std::map<std::string,std::vector<WikiPart>> entries;
                if(glz::read<jsonOptions>(entries,reply))message="Online entry could not be read";
                else{
                    Definition result;result.word=query_;result.source="Wiktionary";result.license="CC BY-SA 4.0";result.url="https://en.wiktionary.org/wiki/"+encode(query_);
                    for(const auto& part:entries["en"])for(const auto& d:part.definitions){
                        if(result.senses.size()>=12)break;
                        result.senses.push_back(Sense{part.partOfSpeech,dictionaryText(d.definition),d.examples.empty()?std::string{}:dictionaryText(d.examples.front()),{}});
                    }
                    definition_=std::move(result);sense_=0;definitionOffsets_={0};message=definition_.senses.empty()?"No English definition found":"Wiktionary · CC BY-SA 4.0";
                }
            }
        }
    }
    if(!wasConnected)net::disconnect();
}
}
