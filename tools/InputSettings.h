#pragma once
#include <windows.h>
#include <filesystem>
bool showInputSettings(HWND owner,const std::filesystem::path& path,int testMode=0);
