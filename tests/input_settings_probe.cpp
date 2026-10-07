#define NOMINMAX
#include "../tools/InputSettings.h"
#include "ConfigStore.h"
#include "../native/tsf/SsfResources.h"
#include <fstream>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
 if(argc<2)return 2;
 const auto root=std::filesystem::path(argv[1]);
 if(!std::filesystem::exists(root/L".schema-manager-test"))return 2;
 try {
  if(!SetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root.c_str()))throw std::runtime_error("Cannot isolate user data");
  if(tiger::skin::skinDirectory()!=root/L"皮肤")throw std::runtime_error("Skin directory escaped user root");
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  const auto config=root/L"config.txt";
  if(argc>2 && std::wstring_view(argv[2])==L"--startup-test"){showInputSettings(nullptr,config,10);CoUninitialize();return 0;}
  if(argc>2 && std::wstring_view(argv[2])==L"--skin-test"){
   showInputSettings(nullptr,config,8);
   showInputSettings(nullptr,config,9); // Warm process-wide font / WIC caches before counting handles.
   const DWORD gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS), user=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
   for(int i=0;i<8;++i)showInputSettings(nullptr,config,9);
   if(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)>gdi+2 || GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)>user+2)
    throw std::runtime_error("Settings reopen leaked graphics or window resources");
   CoUninitialize();return 0;
  }
  if(argc>2 && std::wstring_view(argv[2])==L"--scroll-test"){showInputSettings(nullptr,config,6);CoUninitialize();return 0;}
  if(!showInputSettings(nullptr,config,1))throw std::runtime_error("Settings were not saved");
  const auto saved=tiger::readConfiguration(config);
  if(!showInputSettings(nullptr,config,3))throw std::runtime_error("Unmodified save failed");
  if(showInputSettings(nullptr,config,2) || tiger::readConfiguration(config)!=saved)throw std::runtime_error("Cancel changed configuration");
  const auto multiline=root/L"multiline-config.txt";
  std::filesystem::copy_file(config,multiline);
  if(!showInputSettings(nullptr,multiline,4))throw std::runtime_error("Long whitelist save failed");
  if(!showInputSettings(nullptr,multiline,3))throw std::runtime_error("Long whitelist did not reopen unchanged");
  const auto moved=root/L"moved-config.txt";
  std::filesystem::copy_file(config,moved);
  if(!showInputSettings(nullptr,moved,5) || !showInputSettings(nullptr,moved,3))throw std::runtime_error("Moved settings save / reopen failed");
  const auto precise=root/L"precise-font-config.txt";
  tiger::updateConfigurationValues(precise,{{u"字体大小",u"16.949999999999999"},{u"最大码长",u"4"}});
  if(!showInputSettings(nullptr,precise,7) || !showInputSettings(nullptr,precise,3))throw std::runtime_error("Font display precision regression");
  CoUninitialize();return 0;
 }catch(const std::exception& e){std::ofstream(root/L"error.txt")<<e.what();std::cerr<<e.what();return 1;}
}
