"""Build/run synthetic Engine regressions; no installed IME or user data needed.

Linux/WSL: python3 tests/candidate_selection_test.py [--sanitize]
MSVC Developer PowerShell: python tests/candidate_selection_test.py --cxx cl
Windows builds the Engine probe only, not a TSF host integration test.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    "tests/candidate_selection_probe.cpp",
    "native/Engine.cpp", "native/Dictionary.cpp", "native/Lexicon.cpp",
    "native/DynamicText.cpp", "native/UppercaseText.cpp", "native/Text.cpp",
    "native/SentenceSession.cpp", "native/SentenceAutoCommit.cpp",
    "native/LexiconSerialize.cpp",
]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--sanitize", action="store_true", help="Enable ASan/UBSan (GCC/Clang)")
    args = parser.parse_args()
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error(f"Compiler not found: {args.cxx}")
    msvc = Path(compiler).name.lower() in {"cl", "cl.exe"}
    if msvc and args.sanitize:
        parser.error("--sanitize requires GCC or Clang with ASan/UBSan")
    with tempfile.TemporaryDirectory(prefix="tigirl-selection-") as tmp:
        work = Path(tmp)
        exe = work / ("probe.exe" if os.name == "nt" else "probe")
        if msvc:
            flags = ["/nologo", "/std:c++17", "/EHsc", "/utf-8", "/O2", f"/I{ROOT / 'native'}"]
            output = [f"/Fe:{exe}"]
        else:
            flags = ["-std=c++17", "-O2", "-I", str(ROOT / "native")]
            if args.sanitize:
                flags += ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            output = ["-o", str(exe)]
        subprocess.run([compiler, *flags, *(str(ROOT / p) for p in SOURCES), *output],
                       cwd=work, check=True)
        subprocess.run([str(exe), str(work / "fixture.tcd")], cwd=work, check=True)


if __name__ == "__main__":
    main()
