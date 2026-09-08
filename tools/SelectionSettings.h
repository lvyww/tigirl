#pragma once
#include <windows.h>
#include <filesystem>
bool showSelectionSettings(HWND owner,const std::filesystem::path& path,int testMode=0);
