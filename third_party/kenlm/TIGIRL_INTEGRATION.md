# Tigirl query integration

Copied from TigerClaw `e74a3a6` (`third_party/kenlm`). UPSTREAM.json records the
underlying libime KenLM source revision; TIGERCLAW_PATCHES.md describes its UTF-8
Windows filename patch. No KenLM scoring or model-format source changes were
made for Tigirl.

Tigirl builds the query sources as C++14 (the original headers use
std::binary_function), with KENLM_MAX_ORDER=6, a static MSVC runtime and no
UNICODE/_UNICODE macros on the vendored translation units. Windows filename
opening still explicitly uses the upstream UTF-8-to-_wopen patch.

native/Kenlm.props compiles these sources into each TSF DLL and Windows probe;
CMakeLists.txt supplies the equivalent static library for portable tests.
There is no jointkenlm.dll or C ABI dependency in Tigirl. native/SentenceFivegram
uses KenLM's C++ full-history interface, read-only LAZY mappings, and the same
fivegram's observed bigrams (excluding unknown tokens) for isolation penalties.

Training/executable sources are not part of the build. LICENSE, COPYING and
COPYING.* plus util/double-conversion/LICENSE accompany the distributed runtime.
