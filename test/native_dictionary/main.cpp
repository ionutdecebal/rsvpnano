#include <fstream>
#include <cassert>
#include <vector>
#include <iostream>
#include "dictionary/DictionaryPack.h"
int main(int argc,char** argv){
    assert(argc==2);std::ifstream file(argv[1],std::ios::binary);assert(file);
    file.seekg(0,std::ios::end);const uint32_t size=file.tellg();
    size_t reads=0,total=0;
    auto read=[&](uint32_t at,uint8_t* p,size_t n){++reads;total+=n;file.clear();file.seekg(at);return bool(file.read(reinterpret_cast<char*>(p),n));};
    std::string json;
    for(const char* word:{"book","thought","read","dictionary","running","zebra"}){
        reads=total=0;assert(dictionary::findDefinition(read,size,word,json));assert(json.find("definition")!=std::string::npos);
        assert(reads<45);assert(total<9000);std::cout<<word<<": "<<reads<<" reads / "<<total<<" bytes\n";
    }
    assert(!dictionary::findDefinition(read,size,"zzzzzznonexistent",json));assert(!dictionary::findDefinition(read,12,"book",json));
    auto broken=[&](uint32_t at,uint8_t* p,size_t n){if(!read(at,p,n))return false;if(at==0)p[0]='X';return true;};
    assert(!dictionary::findDefinition(broken,size,"book",json));
    auto badBounds=[&](uint32_t at,uint8_t* p,size_t n){if(!read(at,p,n))return false;if(at==0){p[20]=p[21]=p[22]=p[23]=255;}return true;};
    assert(!dictionary::findDefinition(badBounds,size,"book",json));
    std::cout<<"Dictionary exact reads, missing keys, corrupt headers and bounds: passed\n";
}
