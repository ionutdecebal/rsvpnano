#pragma once
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#define FILE_READ "r"
#define FILE_WRITE "w"

namespace fixture {
struct Faults {
    bool noRoot=false,folderFailure=false,writeFailure=false,shortWrite=false,shortRead=false,corruptRead=false,removeFailure=false;
    size_t writes=0,reads=0;
};
struct Node { std::string data;bool directory=false; };
}
class File {
public:
    File()=default;
    File(std::shared_ptr<fixture::Node> node,fixture::Faults* faults):node_(std::move(node)),faults_(faults){}
    explicit operator bool()const{return bool(node_);}
    bool isDirectory()const{return node_&&node_->directory;}
    size_t size()const{return node_?node_->data.size():0;}
    size_t read(uint8_t* out,size_t count){
        ++faults_->reads;count=std::min(count,size()-position_);
        if(faults_->shortRead&&count)--count;
        if(count)std::memcpy(out,node_->data.data()+position_,count);
        if(count&&faults_->corruptRead)out[0]^=1;
        position_+=count;return count;
    }
    size_t write(const uint8_t* data,size_t count){
        ++faults_->writes;if(!node_||faults_->writeFailure)return 0;
        if(count&&faults_->shortWrite)--count;
        node_->data.append(reinterpret_cast<const char*>(data),count);return count;
    }
    void flush(){}
    void close(){node_.reset();}
private:
    std::shared_ptr<fixture::Node> node_;fixture::Faults* faults_=nullptr;size_t position_=0;
};
namespace fs {
class FS {
public:
    fixture::Faults faults;
    std::map<std::string,std::shared_ptr<fixture::Node>> nodes;
    std::vector<std::string> removed,openedForWrite;
    FS(){nodes["/"]=std::make_shared<fixture::Node>(fixture::Node{{},true});}
    bool exists(const char* path)const{return nodes.contains(path);}
    File open(const char* path,const char* mode=FILE_READ){
        if(faults.noRoot&&std::string_view{path}=="/")return {};
        if(std::string_view{mode}==FILE_WRITE){
            openedForWrite.emplace_back(path);
            if(faults.writeFailure)return {};
            nodes[path]=std::make_shared<fixture::Node>();
        }
        const auto it=nodes.find(path);return it==nodes.end()?File{}:File{it->second,&faults};
    }
    bool mkdir(const char* path){
        if(faults.folderFailure||nodes.contains(path))return false;
        nodes[path]=std::make_shared<fixture::Node>(fixture::Node{{},true});return true;
    }
    bool remove(const char* path){
        removed.emplace_back(path);if(faults.removeFailure)return false;return nodes.erase(path)!=0;
    }
};
}
