#define NOMINMAX
#include <windows.h>
#include <icu.h>
#include <iostream>
int main() {
    UVersionInfo version{}; u_getUnicodeVersion(version);
    std::cout<<"// Generated from Windows ICU Unicode "<<int(version[0])<<'.'<<int(version[1])<<'.'<<int(version[2])<<".\n";
    std::cout<<"// Regenerate with tools/export_grapheme.cpp; do not edit ranges.\n";
    const char* names[]={"CONTROL","CR","LF","L","V","LV","LVT","T","EXTEND","ZWJ","SPACING_MARK","PREPEND","REGIONAL_INDICATOR"};
    const int values[]={U_GCB_CONTROL,U_GCB_CR,U_GCB_LF,U_GCB_L,U_GCB_V,U_GCB_LV,U_GCB_LVT,U_GCB_T,U_GCB_EXTEND,U_GCB_ZWJ,U_GCB_SPACING_MARK,U_GCB_PREPEND,U_GCB_REGIONAL_INDICATOR};
    for(int i=0;i<13;++i) std::cout<<"constexpr int GCB_"<<names[i]<<'='<<values[i]<<";\n";
    std::cout<<"struct GraphemeRange { std::uint32_t first,last; unsigned char value; };\ninline constexpr GraphemeRange graphemeRanges[]={\n";
    int start=0,previous=-1;
    for(int c=0;c<=0x110000;++c) {
        const int value=c==0x110000?-1:u_getIntPropertyValue(c,UCHAR_GRAPHEME_CLUSTER_BREAK)|(u_hasBinaryProperty(c,UCHAR_EXTENDED_PICTOGRAPHIC)?32:0);
        if(value==previous) continue;
        if(previous>0) std::cout<<'{'<<start<<','<<c-1<<','<<previous<<"},\n";
        previous=value;start=c;
    }
    std::cout<<"};\n";
}
