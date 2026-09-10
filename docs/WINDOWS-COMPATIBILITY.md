# Windows compatibility

The x64 installer targets Windows 10 1809 (build 17763), including Enterprise
LTSC 2019, and newer x64 Windows. It includes x64 and x86 TSF DLLs; 32-bit
Windows and ARM64 Windows are not supported by this installer. ARM64X remains
a separate development build. Windows 7 is not supported.

## ICU removal

Production DLLs and helpers no longer link to or load ICU. `native/Unicode.h`
uses checked-in Unicode 15.1.0 tables for simple upper/lower casing, letter
categories, decimal digits and Unicode White_Space. This pins character behavior
across OS releases. Existing .NET ordinal exceptions (U+0131, U+017F) and
invariant lowercase U+0130 handling remain at their call sites. UTF-16 surrogate
pairs and isolated surrogates keep their previous behavior.

`tools/generate_unicode_properties.ps1` regenerates the tables on a development
machine with Windows ICU Unicode 15.1.0; it refuses other Unicode versions.
Neither generation nor ICU installation is required to build or run the product.
`UnicodePropertiesProbe` dynamically loads ICU only as a test oracle and compares
all 1,114,112 code points for all five operations. The pre-existing grapheme
exporter also remains a development-only ICU tool; it is not distributed.

File ordering during import uses Windows NLS `CompareStringEx` in place of ICU
culture collation. Primary scheme files still precede other files, and equal
sort keys retain enumeration order. Accented letters, punctuation and other
special filenames may change relative order. If files contain duplicate entries,
this can change merge precedence when rebuilding a dictionary. NLS culture data
can also differ across OS versions. Existing compiled dictionaries are unaffected
until rebuilt. `tests/lexicon_order_nls_test.ps1` compares against .NET Framework
NLS across six cultures. The historical `lexicon_order_parity.py` ICU oracle is
not the compatibility target for this release.

## Remaining requirements

The renderer retains DirectWrite Factory3 and Per-Monitor V2 DPI handling;
Windows 10 1809 provides these APIs. System `icu.dll`, introduced in 1903,
is no longer required. The installer and PowerShell deployment guard both use
build 17763. Version checks alone do not prove compatibility: validate on a real
1809/LTSC 2019 installation before declaring that platform field-tested.

Reference: https://learn.microsoft.com/windows/win32/intl/international-components-for-unicode--icu-
