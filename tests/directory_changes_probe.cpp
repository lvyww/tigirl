#include "tsf/DirectoryChanges.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using tiger::tsf::DirectoryChanges;
static void check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
int wmain(int argc,wchar_t** argv) {
    try {
        check(argc==3,"directory_changes_probe <marked-root> <watch|write-stage>");
        const std::filesystem::path root(argv[1]);
        check(root.is_absolute() && std::filesystem::exists(root/L".directory-changes-test"),"Missing isolated test marker");
        const std::wstring mode(argv[2]);
        if(mode==L"root-rename") {
            const auto moved=std::filesystem::path(root.wstring()+L".moved");
            check(!std::filesystem::exists(moved),"Root rename destination exists");
            DirectoryChanges watcher;check(SUCCEEDED(watcher.open(root)),"Cannot watch rename fixture");
            std::error_code error;std::filesystem::rename(root,moved,error);
            const auto watchedError=error.value();
            if(!error)std::filesystem::rename(moved,root);
            watcher.close();
            std::filesystem::rename(root,moved);std::filesystem::rename(moved,root);
            std::cout<<"{\"watched_rename_error\":"<<watchedError<<",\"closed_rename_succeeded\":true}\n";
            return 0;
        }
        if(mode!=L"watch") {
            const int stage=std::stoi(mode);
            const auto config=root/L"config.txt",temp=root/L"config.tmp";
            if(stage==0) { std::ofstream out(config);out<<"default"; }
            else if(stage==1) {
                {std::ofstream out(temp);out<<"changed";}
                check(MoveFileExW(temp.c_str(),config.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"Atomic replacement failed");
            } else if(stage==2) {
                std::filesystem::create_directories(root/L"user");
                std::ofstream out(root/L"user"/L"tiger-words.tcu",std::ios::app);out<<"journal";
            } else if(stage==3) {
                const auto schema=root/L"schemas"/L"测试方案";
                std::filesystem::create_directories(schema/L"generations"/L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
                {std::ofstream out(schema/L"generations"/L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"/L"tiger-v2.tcd");out<<"fixture";}
                std::ofstream out(schema/L"current.txt");out<<"generation\taaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            } else if(stage==4) {
                std::ofstream out(root/L"自定义选重键.txt");out<<"1 81";
            } else if(stage==5)check(std::filesystem::remove(config),"Delete failed");
            else if(stage==6) {
                for(int i=0;i<128;++i) {std::ofstream out(root/(L"burst-"+std::to_wstring(i)));out<<i;}
            } else if(stage==7) {
                std::ofstream out(root/L"user"/L"tiger-words.tcu",std::ios::app);out<<"append";
            } else if(stage==8) {
                // Same path and byte count: last-write invalidation must work.
                std::ofstream out(root/L"自定义选重键.txt");out<<"1 82";
            } else if(stage==9) {
                std::filesystem::rename(root/L"schemas"/L"测试方案",root/L"schemas"/L"改名方案");
            } else throw std::runtime_error("Unknown mutation stage");
            std::cout<<"written\n";return 0;
        }
        DirectoryChanges watcher;check(SUCCEEDED(watcher.open(root)),"Cannot open watcher");
        std::cout<<"ready\n"<<std::flush;
        for(std::string command;std::getline(std::cin,command);) {
            if(command=="close")break;
            bool changed=false;
            if(command=="changed") {
                const auto deadline=GetTickCount64()+5000;
                do {check(SUCCEEDED(watcher.poll(changed)),"Change poll failed");if(!changed)Sleep(10);}while(!changed && GetTickCount64()<deadline);
                check(changed,"Missed cross-process change");
                std::cout<<"changed\n"<<std::flush;
            } else if(command=="quiet") {
                // Notifications may coalesce or arrive more than once. Require
                // a quiet interval, not an invented one-write/one-event count.
                const auto deadline=GetTickCount64()+5000;auto quietSince=GetTickCount64();
                do {
                    check(SUCCEEDED(watcher.poll(changed)),"Quiet poll failed");
                    if(changed)quietSince=GetTickCount64();
                    Sleep(10);
                }while(GetTickCount64()-quietSince<100 && GetTickCount64()<deadline);
                check(GetTickCount64()-quietSince>=100,"Watch never became idle");
                std::cout<<"quiet\n"<<std::flush;
            } else if(command=="idle") {
                check(SUCCEEDED(watcher.poll(changed)) && !changed,"Unexpected directory invalidation");
                std::cout<<"idle\n"<<std::flush;
            } else throw std::runtime_error("Unknown watcher command");
        }
        watcher.close();bool changed=true;
        check(watcher.poll(changed)==E_HANDLE && !changed,"Closed watcher did not fail cleanly");
        check(watcher.open(L"relative")==E_INVALIDARG,"Relative root accepted");
        check(FAILED(watcher.open(root/L"missing")),"Missing root accepted");
        DWORD before=0,after=0;check(GetProcessHandleCount(GetCurrentProcess(),&before)!=FALSE,"Handle baseline failed");
        for(int i=0;i<100;++i) {check(SUCCEEDED(watcher.open(root)),"Reopen failed");watcher.close();}
        check(GetProcessHandleCount(GetCurrentProcess(),&after)!=FALSE && after==before,"Watcher handle leak");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
