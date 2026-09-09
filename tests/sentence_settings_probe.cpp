#include "../native/SentenceSettings.h"
#include <fstream>
#include <iostream>
std::u16string token(const std::string& h){std::u16string s;if(h!="-")for(std::size_t i=0;i<h.size();i+=4)s+=static_cast<char16_t>(std::stoul(h.substr(i,4),nullptr,16));return s;}
void hex(std::u16string_view s){const char* h="0123456789abcdef";std::cout<<'"';for(auto c:s)std::cout<<h[c>>12]<<h[(c>>8)&15]<<h[(c>>4)&15]<<h[c&15];std::cout<<'"';}
int wmain(int argc,wchar_t** argv) {
 try {
  if(argc!=2)return 2;std::ifstream input(argv[1]);if(!input)throw std::runtime_error("Missing fixtures");
  for(std::string line;std::getline(input,line);){auto c=tiger::parseSentenceSettings(token(line));
   std::cout<<"{\"enable\":"<<(c.autoEnableBySchema?"true":"false")<<",\"automatic\":"<<(c.autoCommit?"true":"false")<<",\"duplicates\":"<<(c.allowDuplicateSingleCharacters?"true":"false")<<",\"retained\":"<<c.minimumRetainedRaw<<",\"common\":"<<c.commonCharacterLimit<<",\"white\":";hex(c.fullCodeWhitelist);
   std::cout<<",\"characters\":[";bool first=true;for(const auto& value:c.whitelist()){if(!first)std::cout<<',';first=false;hex(value);}std::cout<<"]}\n";
  }return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
