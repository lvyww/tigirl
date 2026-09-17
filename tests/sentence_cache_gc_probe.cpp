#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "SentenceCache.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::wstring revision(int value) {
    std::wstring result(63,L'0');
    result+=static_cast<wchar_t>(L'0'+value);
    return result;
}
std::size_t revisions(const std::filesystem::path& root) {
    std::size_t count=0;
    for(const auto& item:std::filesystem::directory_iterator(root))
        if(tiger::sentence_cache_detail::revisionName(item.path().filename().u16string()))++count;
    return count;
}
std::size_t tombstones(const std::filesystem::path& root) {
    std::size_t count=0;
    for(const auto& item:std::filesystem::directory_iterator(root))
        if(item.path().filename().wstring().rfind(L".gc-",0)==0)++count;
    return count;
}
}

int main() {
    try {
        using namespace tiger::sentence_cache_detail;
        const auto root=std::filesystem::current_path()/L"sentence-cache-gc-fixture";
        std::error_code ignored;std::filesystem::remove_all(root,ignored);std::filesystem::create_directories(root);
        const auto base=std::filesystem::file_time_type::clock::now()-std::chrono::hours(1);
        std::vector<std::filesystem::path> directories;
        for(int i=0;i<10;++i) {
            auto directory=root/revision(i);std::filesystem::create_directories(directory);
            {std::ofstream lock(directory/L".import.lock",std::ios::binary);lock<<'x';}
            std::error_code error;std::filesystem::last_write_time(directory,base+std::chrono::seconds(i),error);
            check(!error,"cannot timestamp cache fixture");directories.push_back(std::move(directory));
        }
        const auto current=directories.back().filename().u16string();
        const auto heldPath=directories.front()/L".import.lock";
        HANDLE held=CreateFileW(heldPath.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(held!=INVALID_HANDLE_VALUE,"cannot open held import lock");
        OVERLAPPED offset{};check(LockFileEx(held,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&offset)!=FALSE,"cannot hold import lock");

        prune(root,current);
        check(std::filesystem::is_directory(directories.back()),"active revision was removed");
        check(std::filesystem::is_directory(directories.front()),"locked revision was removed");
        check(revisions(root)==9,"first GC did not retain exactly the active/recent/locked revisions");

        UnlockFileEx(held,0,1,0,&offset);CloseHandle(held);
        prune(root,current);
        check(revisions(root)==maximumRevisions,"unlocked stale revision was not collected");
        check(tombstones(root)==0,"retired cache tombstone was left behind");
        std::filesystem::remove_all(root);
        std::cout<<"PASS: sentence cache GC preserves active/locked revisions and bounds stale revisions.\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"sentence cache GC regression failed: "<<error.what()<<"\n";
        return 1;
    }
}
#else
int main(){return 0;}
#endif
