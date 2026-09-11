from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
deploy = (ROOT / "packaging" / "setup" / "deploy.ps1").read_text(encoding="utf-8-sig")
iss = (ROOT / "packaging" / "setup" / "Tigirl.iss").read_text(encoding="utf-8-sig")
zip_install = (ROOT / "packaging" / "install.ps1").read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


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

# Uninstall commits at registration removal; damaged inventory only limits file cleanup.
require("Uninstall inventory is incomplete; registration removal will continue" in deploy, "damaged manifest still blocks uninstall")
require("Registration removal is the uninstall commit point" in deploy, "uninstall commit boundary is missing")
require("Program files retained for retry/reboot" in deploy, "locked/corrupt file cleanup is still fatal")
require("Input method registration belongs to another installation." in deploy, "foreign registration protection was removed")

# The ZIP installer follows the same machine/user transaction boundary.
require("The new machine registration remains active" in zip_install, "ZIP installer still treats user initialization as machine failure")
require("$recovery=if($installed.previous)" not in zip_install, "ZIP user initialization still launches rollback/uninstall")

print("PASS: repairable setup policy, commit boundary, persistent logs, and non-fatal file cleanup.")
