#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
namespace tiger {
struct ReminderTicket {std::u16string epoch;std::uint64_t sequence=0;};
struct ReminderRecord {
    std::u16string epoch;
    std::uint64_t allocated=0,accepted=0,deadline=0;
};
// Reserving a ticket does not cancel the currently accepted timer. The helper
// accepts it only after startup succeeds. Both operations use the configuration
// store's stable lock and atomic replacement, including across process exits.
ReminderTicket reserveReminder(const std::filesystem::path& path);
bool acceptReminder(const std::filesystem::path& path,const ReminderTicket& ticket,std::uint64_t deadline);
ReminderRecord readReminder(const std::filesystem::path& path);
}
