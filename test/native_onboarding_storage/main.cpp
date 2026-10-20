#include <cassert>
#include <cstdio>
#include <FS.h>
#include "storage/fs/OnboardingStorage.h"

using OnboardingStorage::State;
struct Run {
    fs::FS disk;
    std::vector<OnboardingStorage::Progress> progress;
    bool cancel=false;
    size_t cancelAfterWrites=0;
    static bool stop(void* ctx){auto& self=*static_cast<Run*>(ctx);return self.cancel||(self.cancelAfterWrites&&self.disk.faults.writes>=self.cancelAfterWrites);}
    static void report(void* ctx,OnboardingStorage::Progress p){static_cast<Run*>(ctx)->progress.push_back(p);}
    OnboardingStorage::Progress check(bool mounted=true){return OnboardingStorage::check(disk,mounted,0x12345678,report,stop,this);}
    void preserve(const char* path,const char* text){disk.nodes[path]=std::make_shared<fixture::Node>(fixture::Node{text,false});}
    size_t probes()const{size_t n=0;for(auto& [name,node]:disk.nodes)if(name.find(".nano-check-")!=std::string::npos)++n;return n;}
};
int main(){
    {Run r;assert(r.check(false).state==State::Unavailable);assert(r.disk.openedForWrite.empty());assert(r.disk.nodes.size()==1);}
    {Run r;r.disk.faults.noRoot=true;assert(r.check().state==State::Unavailable);assert(r.disk.openedForWrite.empty());}
    std::puts("PASS: unavailable or unreadable card performs no writes.");
    {Run r;auto p=r.check();assert(p.state==State::Ready&&p.stage==2);assert(r.progress.size()==3);for(int i=0;i<3;++i)assert(r.progress[i].stage==i&&r.progress[i].state==State::Checking);assert(r.disk.openedForWrite.size()==7);assert(r.disk.removed.size()==7);assert(!r.probes());assert(r.disk.faults.writes==56&&r.disk.faults.reads==56);}
    std::puts("PASS: an empty card prepares all seven folders and verifies/cleans seven test files.");
    {Run r;r.preserve("/my-book.epub","MY BOOK");r.preserve("/settings-backup.toml","MY SETTINGS");
     r.disk.mkdir("/library");r.preserve("/library/.nano-check-12345678-0.tmp","PRE-EXISTING");
     assert(r.check().state==State::Ready);assert(r.disk.nodes.at("/my-book.epub")->data=="MY BOOK");assert(r.disk.nodes.at("/settings-backup.toml")->data=="MY SETTINGS");assert(r.disk.nodes.at("/library/.nano-check-12345678-0.tmp")->data=="PRE-EXISTING");assert(r.probes()==1);}
    std::puts("PASS: user files and an existing probe-name collision are never overwritten or removed.");
    {Run r;r.disk.faults.folderFailure=true;assert(r.check().state==State::FoldersFailed);assert(r.disk.openedForWrite.empty());}
    {Run r;r.preserve("/library","EXISTING FILE");assert(r.check().state==State::FoldersFailed);assert(r.disk.nodes.at("/library")->data=="EXISTING FILE");assert(r.disk.openedForWrite.empty());}
    std::puts("PASS: folder failure/file collision stops before save probes without replacing the file.");
    {Run r;r.disk.faults.writeFailure=true;assert(r.check().state==State::SaveFailed);assert(!r.probes());}
    {Run r;r.disk.faults.shortWrite=true;assert(r.check().state==State::SaveFailed);assert(!r.probes());}
    std::puts("PASS: denied and partial writes fail the save stage and clean owned files.");
    {Run r;r.disk.faults.shortRead=true;assert(r.check().state==State::SaveFailed);assert(!r.probes());}
    {Run r;r.disk.faults.corruptRead=true;assert(r.check().state==State::SaveFailed);assert(!r.probes());}
    std::puts("PASS: short and corrupted reads are detected, not reported as ready.");
    {Run r;r.disk.faults.removeFailure=true;assert(r.check().state==State::SaveFailed);assert(r.probes()==1);}
    std::puts("PASS: failure to remove the owned test file is a failure, never a ready result.");
    {Run r;r.cancel=true;assert(r.check().state==State::Cancelled);assert(r.disk.openedForWrite.empty());}
    {Run r;r.cancelAfterWrites=2;assert(r.check().state==State::Cancelled);assert(!r.probes());assert(r.disk.openedForWrite.size()==1);}
    std::puts("PASS: cooperative cancellation/deadline stops before I/O or between chunks and cleans its file.");
    {Run r;r.disk.mkdir("/library");for(unsigned i=0;i<16;++i){char p[64];std::snprintf(p,sizeof(p),"/library/.nano-check-12345678-%u.tmp",i);r.preserve(p,"EXISTING");}assert(r.check().state==State::SaveFailed);assert(r.probes()==16);assert(r.disk.openedForWrite.empty());assert(r.disk.removed.empty());}
    std::puts("PASS: exhausting probe-name attempts fails safely without deleting prior data.");
    std::puts("9 production onboarding storage regression groups passed.");
}
