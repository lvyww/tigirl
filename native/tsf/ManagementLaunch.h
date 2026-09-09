#pragma once
#include <windows.h>
#include <string_view>
namespace tiger::tsf {
HRESULT launchManagement(HINSTANCE module,std::wstring_view action,std::wstring_view value={});
}
