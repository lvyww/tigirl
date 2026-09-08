# Bundled fonts

`PrivateFonts` loads `.ttf`, `.otf` and `.ttc` files from the immutable DLL
version's `字体` directory when the configured family begins with `#`.
A weak cache reuses a shared resource lease across services in one process.
Reloading unchanged settings acquires the replacement lease before releasing the
old lease, so focus changes do not unload/reload the font. Candidate windows retain
a lease until their HFONT is deleted. The final lease removes the resources.

Registration uses `FR_PRIVATE | FR_NOT_ENUM` through
[AddFontResourceExW](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-addfontresourceexw).
It does not install a system font or change the font registry. GDI font mapping
chooses private resources ahead of a public family with the same name. Regular
family names continue to use installed system fonts. Missing or rejected bundled
fonts produce debug output and allow the system fallback.

The package contains the exact local reference `LXGWWenKaiGBScreen.ttf`, without
subsetting or modification (26,234,636 bytes). `data/fonts/provenance.json` records
its source, embedded copyright, version and SHA-256. `OFL.txt` accompanies it.
The build copies these files into `build/ARM64/Release/字体`.

`tests/PrivateFontProbe.vcxproj` verifies on native ARM64 that selecting the Chinese
family name retrieves exactly the original font bytes via
[GetFontData](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getfontdata),
that candidate glyphs exist, that two callers reuse one lease, and that the final
release removes the registration before a successful reload. Evidence is in
`build/private-font-arm64.json`. The test is independent of installed TSF and
needs no elevation. It does not measure font memory sharing across processes.

Installation generations now hash all package artifacts, including font and
license files; a font-only update therefore produces a new immutable directory.
`install_arm64.ps1 -CheckOnly` validates the ARM64 binary and five artifact hashes
without changing registration or requesting elevation. `build/package-preflight.json`
records the pending package. Actual installation and new candidate-window
application tests remain outstanding.
