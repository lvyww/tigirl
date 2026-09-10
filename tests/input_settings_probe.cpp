#define NOMINMAX
#include "../tools/InputSettings.h"
#include "ConfigStore.h"
#include <fstream>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
 if(argc<2)return 2;
 const auto root=std::filesystem::path(argv[1]);
 if(!std::filesystem::exists(root/L".schema-manager-test"))return 2;
 try {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  const auto config=root/L"config.txt";
  if(!showInputSettings(nullptr,config,1))throw std::runtime_error("Settings were not saved");
  const auto saved=tiger::readConfiguration(config);
  if(showInputSettings(nullptr,config,2) || tiger::readConfiguration(config)!=saved)throw std::runtime_error("Cancel changed configuration");
  CoUninitialize();return 0;
 }catch(const std::exception& e){std::ofstream(root/L"error.txt")<<e.what();std::cerr<<e.what();return 1;}
}
