#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
namespace tiger {
inline constexpr std::u16string_view inputSettingsReloadKey=u"_native_settings_reload";
struct SelectionKeys;
struct CandidateStyle;
CandidateStyle cycleCandidateMode(const std::filesystem::path& path,bool& horizontalCode,bool& verticalCode);
void updateSelectionKeys(const std::filesystem::path& path,const SelectionKeys& before,const SelectionKeys& after);
// Repair a malformed selection file only if it still matches the displayed
// snapshot. Preserve its original bytes in a uniquely named adjacent backup.
void repairSelectionKeys(const std::filesystem::path& path,std::string_view expectedBytes,const SelectionKeys& after);
// Windows per-user configuration mutation. Cooperating TSF hosts lock a stable
// sidecar, read the latest value and atomically replace the complete text file.
// Adjust the latest persisted size under the configuration lock; no input reset.
double adjustCandidateFontSize(const std::filesystem::path& path,int wheelDelta);
bool toggleHiddenCandidates(const std::filesystem::path& path);
// Caller first validates/loads the target schema. Selection and the two-entry
// recent list are derived from the latest locked configuration and published together.
void selectSchemaConfiguration(const std::filesystem::path& path,std::u16string_view canonicalName);
std::u16string switchRecentSchemaConfiguration(const std::filesystem::path& path,
    const std::vector<std::u16string>& schemas,const std::function<void(std::u16string_view)>& prepare);
std::u16string readConfiguration(const std::filesystem::path& path);
std::string readConfigurationBytes(const std::filesystem::path& path);
// Apply only edited fields to the latest locked file, preserving concurrent
// changes to other settings (including schema selection and recent history).
void updateConfigurationValues(const std::filesystem::path& path,
    const std::vector<std::pair<std::u16string,std::u16string>>& changes,
    const std::function<void(std::u16string_view)>& validate={});
// Explicit settings saves carry a fresh reload request in the same atomic file
// replacement as the edited fields. Ordinary file edits/schema switches do not.
void saveInputConfiguration(const std::filesystem::path& path,
    const std::vector<std::pair<std::u16string,std::u16string>>& changes,
    const std::function<void(std::u16string_view)>& validate={});
// Publish a previously validated immutable generation using the configuration
// store's stable lock and atomic replacement. Caller retains its mapping.
void selectSchemaGeneration(const std::filesystem::path& schemaDirectory,std::u16string_view generation);
}
