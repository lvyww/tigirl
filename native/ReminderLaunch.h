#pragma once
#include "ReminderLedger.h"
#include <windows.h>
namespace tiger {
struct LaunchedReminder {ReminderTicket ticket;DWORD processId=0;};
// Only an actual timer command calls this. The child acknowledges readiness
// before this process publishes the request, then waits for publication or
// parent exit. A test event substitutes for the popup only in explicit probes.
LaunchedReminder launchReminder(const std::filesystem::path& executable,
    const std::filesystem::path& ledger,int milliseconds,const std::wstring& testEvent={});
}
