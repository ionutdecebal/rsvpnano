#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace dictionary {
// Reader must perform an exact bounded read(offset,buffer,count). Shared by device and host tests.
template<typename Reader>
bool findDefinition(Reader read,uint32_t fileSize,std::string_view word,std::string& json) {
    json.clear();
    std::array<uint8_t,24> header{};
    auto u32=[](const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
    if(fileSize<24||!read(0,header.data(),24)||std::memcmp(header.data(),"RSVPDIC1",8))return false;
    const uint32_t count=u32(header.data()+8),index=u32(header.data()+12),keys=u32(header.data()+16),data=u32(header.data()+20);
    if(index!=24||count>1000000||uint64_t(index)+uint64_t(count)*16!=keys||keys>data||data>fileSize)return false;
    uint32_t lo=0,hi=count;
    while(lo<hi){
        const uint32_t mid=lo+(hi-lo)/2;std::array<uint8_t,16> raw{};
        if(!read(index+mid*16,raw.data(),16))return false;
        const uint32_t keyOffset=u32(raw.data()),keyLength=u32(raw.data()+4),offset=u32(raw.data()+8),length=u32(raw.data()+12);
        if(keyLength>256||!keyLength||length>6144||!length||uint64_t(keys)+keyOffset+keyLength>data||uint64_t(data)+offset+length>fileSize)return false;
        std::string key(keyLength,'\0');if(!read(keys+keyOffset,reinterpret_cast<uint8_t*>(key.data()),keyLength))return false;
        const int comparison=word.compare(key);
        if(comparison<0)hi=mid;else if(comparison>0)lo=mid+1;else{
            json.resize(length);return read(data+offset,reinterpret_cast<uint8_t*>(json.data()),length);
        }
    }
    return false;
}
}
