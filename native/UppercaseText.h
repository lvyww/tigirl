#pragma once
#include <string>
#include <string_view>
namespace tiger {
struct ManualTimerSpec {
    bool matched=false;
    // -1 consumes a syntactically matching command without scheduling (zero
    // or unparseable minutes); 0 is a valid immediately due timer.
    int milliseconds=-1;
};
ManualTimerSpec parseManualTimer(std::u16string_view code);
bool numericUppercasePrefix(std::u16string_view code);
std::u16string uppercaseCurrency(std::u16string_view code);
}
