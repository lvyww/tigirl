#include "../native/ManagementUri.h"
#include <iostream>
int wmain() {
    unsigned checks=0;
    for(auto name:{L"虎码字词",L"虎整句",L"测试 空格",L"繁體-𠀀"}) {
        auto parsed=tiger::parseManagementUri(tiger::managementUri(L"use",name));
        if(parsed.first!=L"use" || parsed.second!=name)return 1;++checks;
    }
    for(auto bad:{L"https://use/1234",L"nativetiger://use",L"nativetiger://use/123",L"nativetiger://use/xxxx",L"nativetiger://use/0000",L"nativetiger://use/../"}) {
        bool rejected=false;try{tiger::parseManagementUri(bad);}catch(...){rejected=true;}if(!rejected)return 2;++checks;
    }
    std::cout<<"management URI checks passed: "<<checks<<'\n';
}
