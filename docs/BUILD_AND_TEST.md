# Compile, test, and stage a release

## Compile without release data

From PowerShell, with Visual Studio C++ tools and a Windows SDK installed:

```powershell
./build_native.ps1 -Platform x64,Win32
# Explicit compatibility toolset (CI uses VS 2022 on windows-2025):
./build_native.ps1 -Platform x64,Win32 -PlatformToolset v143
# Cross-compilation only; requires the ARM64 C++ component:
./build_native.ps1 -Platform ARM64
```

This entry point builds the DLL, architecture loader probe, and all four production
tools. It does not look for the sentence model, touch user data, register COM, or
install the IME. Build failures stop immediately. Existing output names and paths
are preserved: DLLs under `build/<platform>/Release`, tools/probes under
`build/tests/<platform>`. `-ToolPlatform` can select companion-tool architectures
independently (used by the existing x64 release-staging wrapper).

The default project toolset remains v145; selecting v143 is explicit, not a silent
fallback. ARM64X retains its existing experimental build path.

Production tools and tests now share `msbuild/NativeApp.props` and source groups,
rather than tools importing test executables. Source item identities are retained
so existing downstream test projects can still remove/replace their entry points.

## Synthetic regression tests

```sh
python tests/run_core_tests.py
python tests/run_core_tests.py --sanitize
```

From an MSVC Developer PowerShell matching the architecture to test:

```powershell
python tests/run_core_tests.py --cxx cl
```

The runner compiles shared sources once, then runs candidate selection, sentence
session, reveal/animation, and code masking regressions. Fixtures and object files
live in a temporary directory. A failed compile/assertion or timeout fails the run.
The existing focused test entry points remain available.

`.github/workflows/build-and-test.yml` runs Windows x64/Win32 builds and these
regressions for pull requests, plus Linux ASan/UBSan. Jobs have time limits and a
read-only token; checkout does not persist credentials. No deployment, signing,
user-input logging, automatic merge, or repository-setting changes are included.

## Release staging remains strict

`build_x64.ps1`, `build_arm64.ps1`, and `build_arm64x.ps1` still require a valid
sentence model for release staging. Supply `-SentenceModelPath` explicitly on a
new machine. Missing/corrupt release data remains an error; compile-only CI does
not weaken model validation or generate placeholder production resources.

Before shipping, separately validate the actual release toolset/architectures,
model and dictionary hashes, installation/rollback, TSF UI-less and native-window
selection, real application input, focus transitions and multi-DPI behavior.
A successful CI run is not a physical-input or complete Windows-host acceptance.
