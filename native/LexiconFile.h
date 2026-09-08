#pragma once
#include "LexiconImport.h"
#include <filesystem>
namespace tiger {
ParsedLexiconRows parseLexiconFile(const std::filesystem::path& path);
}
