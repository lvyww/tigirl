#define NOMINMAX
#include "ReminderLaunch.h"
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    if(argc!=5)return 2;
    try {
        const auto launched=tiger::launchReminder(argv[1],argv[2],std::stoi(argv[3]),argv[4]);
        std::cout<<launched.processId<<'\n';return 0;
    } catch(const std::exception&) {return 1;}
}
