from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
deploy = (ROOT / "packaging" / "setup" / "deploy.ps1").read_text(encoding="utf-8-sig")
iss = (ROOT / "packaging" / "setup" / "Tigirl.iss").read_text(encoding="utf-8-sig")
retirement = (ROOT / "packaging" / "retirement.ps1").read_text(encoding="utf-8-sig")
zip_install = (ROOT / "packaging" / "install.ps1").read_text(encoding="utf-8-sig")
package_builder = (ROOT / "package_x64.ps1").read_text(encoding="utf-8-sig")
initialize = (ROOT / "packaging" / "initialize.ps1").read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


# Use the highest standard LZMA2 preset without changing the full-extraction lifecycle.
require("\nCompression=lzma2/ultra64\n" in iss, "installer no longer uses the ultra64 compression preset")
require("\nSolidCompression=yes\n" in iss, "installer solid compression is disabled")
require("\nLZMAUseSeparateProcess=yes\n" in iss, "high-compression builds can exhaust the 32-bit compiler address space")
require("ExtractTemporaryFiles('{tmp}\\payload\\*')" in iss, "installer no longer fully extracts its payload before preflight")

# Simpler full-payload lifecycle: distinct runtime generations; architecture-independent runtime files live once under shared/.
require("InstallGeneration:=NewInstallGeneration" in iss, "setup no longer generates a per-run directory")
require("{#Generation}" not in iss, "setup still reuses a package-hash directory")
require("Get-NewVersionDirectory $Generation" in deploy, "fresh-directory collision guard is bypassed")
require("previous=$null;transaction=$j.id" in deploy, "committed installs retain a rollback generation")
require("Retained committed installation after interruption" in deploy, "committed recovery may reactivate retired files")
require("Remove-Version $old.FullName -AtReboot" in retirement, "upgrade immediately deletes old runtime files")
require("uninsneveruninstall" in iss, "Inno can bypass hash-aware runtime-file cleanup")
require("{app}\\maintenance" in iss, "uninstall backend depends on a retired directory")
require('"$stage\\shared"' in package_builder, "x64 package builder does not create the shared runtime directory")
require("New-Item -ItemType HardLink" not in zip_install, "ZIP installer still duplicates x64 resources into x86 via hard links")
require("New-Item $target -ItemType HardLink" not in deploy, "GUI installer still duplicates x64 resources into x86 via hard links")
require("shared\\Tigirl.exe" in initialize, "user initialization still launches an architecture-local desktop tool")
require("Save-DataBaseline $plan $baselinePath" in initialize, "successful data initialization does not publish its baseline")
require("$paths=@($config,$baselinePath," in initialize, "baseline is outside the user-data rollback journal")

# The machine log must survive deletion of Program Files\Tigirl.
require("Join-Path $env:ProgramData 'Tigirl\\Logs'" in deploy, "deploy log is not in ProgramData")
require("{commonappdata}\\Tigirl\\Logs" in iss, "installer log button does not use the persistent log directory")
require("if not ShellExec" in iss and "错误码" in iss, "OpenLog still ignores ShellExec failures")

# Interrupted upgrades are repaired before a new upgrade or an uninstall.
require(
    "if($Action -in @('Preflight','Begin','Recover','UninstallCheck','Uninstall')){Restore-Transaction}" in deploy,
    "uninstall does not recover an interrupted machine transaction first",
)
require("function Resolve-OwnedRecord" in deploy, "managed registrations cannot be adopted when install.json is damaged")
require("Adopting managed registration without a usable matching install record" in deploy, "repair adoption is not logged")
require("Installation record is missing." not in deploy, "uninstall still hard-fails on a missing install record")

# Apply deliberately unregisters the owned old server, then registers the new side-by-side version.
apply = deploy.index("}elseif($Action -eq 'Apply'){")
commit = deploy.index("}elseif($Action -eq 'Commit'){")
apply_block = deploy[apply:commit]
old_unregister = apply_block.index("Invoke-Registration $j.previous.directory -Remove")
new_register = apply_block.index("Invoke-Registration $j.directory")
require(old_unregister < new_register, "new DLL is registered before the old owned registration is removed")
require("Remove-RegistrationKeys" in apply_block, "forward repair has no safe fallback when old regsvr32 fails")

# Once the new registration is committed, user-data initialization is no longer allowed to roll it back.
commit_call = iss.index("Deploy('Commit'")
user_init = iss.index("Code:=UserStep('Initialize')")
require(commit_call < user_init, "user initialization still happens before the machine commit")
require("Machine registration is the commit point" in iss, "transaction boundary is not documented next to the implementation")
require("UserInitFailed:=True" in iss, "user initialization failure has no non-fatal state")
require("码表初始化未完成，正在恢复原安装" not in iss, "old fatal user-initialization rollback path remains")

# The graphical installer runs the original-user step hidden and waits for it. It must
# therefore be incapable of opening the conflict selector behind the disabled wizard.
require("-NoDialogs -SkipConflicts" in iss, "graphical setup does not explicitly preserve conflicts noninteractively")
require(
    "if(!$SkipConflicts -and !$NoDialogs)" in initialize,
    "-NoDialogs does not suppress the conflict-selection dialog",
)
require(
    "Conflict dialogs suppressed; retaining existing files" in initialize,
    "noninteractive conflict retention is not diagnosable in the user initialization log",
)
require("保留现有文件，不阻塞安装" in iss, "installer copy does not describe the noninteractive conflict policy")

# Because machine commit now precedes user initialization, the user journal must carry
# its own completion state. A crash in the user step must restore a prepared journal,
# while an initialized journal can be discarded when the matching machine tx committed.
require("state='prepared'" in initialize, "user journal has no prepared state")
require("state='initialized'" in initialize, "user journal has no initialized state")
require(
    "$committed -and $pending.state -eq 'initialized'" in initialize,
    "machine commit alone still causes an interrupted user journal to be discarded",
)

# Uninstall commits at registration removal; damaged inventory only limits file cleanup.
require("Uninstall inventory is incomplete; registration removal will continue" in deploy, "damaged manifest still blocks uninstall")
require("Registration removal is the uninstall commit point" in deploy, "uninstall commit boundary is missing")
require("Program files retained for retry/reboot" in deploy, "locked/corrupt file cleanup is still fatal")
require("Input method registration belongs to another installation." in deploy, "foreign registration protection was removed")

# The ZIP installer follows the same machine/user transaction boundary.
require("The new machine registration remains active" in zip_install, "ZIP installer still treats user initialization as machine failure")
require("$recovery=if($installed.previous)" not in zip_install, "ZIP user initialization still launches rollback/uninstall")

print("PASS: repairable setup policy, noninteractive conflict handling, commit boundary, persistent logs, durable user journal, and non-fatal file cleanup.")
