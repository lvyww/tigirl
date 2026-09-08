#define NOMINMAX
#include <windows.h>
#include "ReminderLedger.h"
#include "Text.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    try {
        if(argc<3)return 2;
        const auto path=std::filesystem::u8path(argv[1]);const std::string operation=argv[2];
        if(operation=="reserve" && argc==4) {
            const auto count=std::stoi(argv[3]);
            for(int i=0;i<count;++i){const auto ticket=tiger::reserveReminder(path);std::cout<<tiger::utf8(ticket.epoch)<<' '<<ticket.sequence<<'\n';}
        } else if(operation=="accept" && argc==6) {
            std::cout<<tiger::acceptReminder(path,{tiger::utf16(argv[3]),std::stoull(argv[4])},std::stoull(argv[5]))<<'\n';
        } else if(operation=="read" && argc==3) {
            const auto record=tiger::readReminder(path);
            std::cout<<tiger::utf8(record.epoch)<<' '<<record.allocated<<' '<<record.accepted<<' '<<record.deadline<<'\n';
        } else if(operation=="deny-reserve" && argc==3) {
            const auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot hold ledger fixture");
            bool rejected=false;
            try{tiger::reserveReminder(path);}catch(const std::exception&){rejected=true;}
            CloseHandle(file);
            if(!rejected)throw std::runtime_error("Denied reservation succeeded");
            std::cout<<"blocked\n";
        } else return 2;
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
