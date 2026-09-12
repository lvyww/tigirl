"""Run real Windows CandidateUI publication regressions with a controlled clock.

MSVC Developer PowerShell: python tests/candidate_ui_presentation_test.py --negative-control
No installed IME, private fonts, dictionary/model downloads or user data required.
The TSF owner is mocked; this is not a physical-input/QQ acceptance test.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

from work_directory import temporary_work_directory

ROOT = Path(__file__).resolve().parents[1]
COMMON = [
    "native/Engine.cpp", "native/Dictionary.cpp", "native/Lexicon.cpp",
    "native/DynamicText.cpp", "native/UppercaseText.cpp", "native/Text.cpp",
    "native/SentenceSession.cpp", "native/SentenceAutoCommit.cpp",
    "native/LexiconSerialize.cpp", "native/CandidatePresentation.cpp",
    "native/CandidateTheme.cpp", "native/tsf/CandidateRenderer.cpp",
    "native/tsf/PrivateFonts.cpp",
]
LIBS = ["ole32.lib", "oleaut32.lib", "uuid.lib", "user32.lib", "gdi32.lib",
        "shcore.lib", "d2d1.lib", "dwrite.lib", "windowscodecs.lib"]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="cl")
    parser.add_argument("--negative-control", action="store_true",
                        help="Also verify that restoring the old visibility-only animation condition fails")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("This test requires real Windows layered windows and the MSVC environment")
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error(f"Compiler not found: {args.cxx}; run in an MSVC developer shell")
    with temporary_work_directory(prefix="tigirl-ui-presentation-") as work:
        flags = ["/nologo", "/std:c++17", "/EHsc", "/utf-8", "/O2", "/MT",
                 "/DUNICODE", "/D_UNICODE", f"/I{ROOT / 'native'}",
                 f"/I{ROOT / 'native/tsf'}", f"/I{ROOT / 'tests'}"]

        def compile_source(source: Path, name: str) -> Path:
            output = work / (name + ".obj")
            subprocess.run([compiler, *flags, "/c", str(source), f"/Fo:{output}"],
                           cwd=work, check=True, timeout=180)
            return output

        common = [compile_source(ROOT / path, f"common-{i}") for i, path in enumerate(COMMON)]

        def build_and_run(source: Path, name: str) -> subprocess.CompletedProcess:
            obj = compile_source(source, name)
            exe = work / (name + ".exe")
            subprocess.run([compiler, "/nologo", str(obj), *(str(p) for p in common),
                            f"/Fe:{exe}", "/link", *LIBS], cwd=work, check=True, timeout=180)
            return subprocess.run([str(exe), str(work / (name + ".tcd"))], cwd=work,
                                  capture_output=True, text=True, timeout=90)

        result = build_and_run(ROOT / "tests/candidate_ui_presentation_probe.cpp", "presentation")
        print(result.stdout, end="", flush=True)
        if result.returncode:
            raise RuntimeError(f"CandidateUI regressions failed ({result.returncode}): {result.stderr}")
        direction = build_and_run(ROOT / "tests/candidate_direction_ui_probe.cpp", "direction")
        print(direction.stdout, end="", flush=True)
        if direction.returncode:
            raise RuntimeError(f"Candidate direction regressions failed ({direction.returncode}): {direction.stderr}")
        if args.negative_control:
            # Change only the animation decision in a temporary copy; never edit
            # the working tree and never accept compilation errors as a control.
            production = (ROOT / "native/tsf/CandidateUI.cpp").read_text(encoding="utf-8")
            gate = "!firstCandidateFrame && IsWindowVisible(window_)"
            if production.count(gate) != 1:
                raise RuntimeError("Animation gate changed; update the negative-control mutation")
            (work / "legacy-candidate-ui.cpp").write_text(
                production.replace(gate, "IsWindowVisible(window_)"), encoding="utf-8")
            probe = (ROOT / "tests/candidate_ui_presentation_probe.cpp").read_text(encoding="utf-8")
            include = '#include "../native/tsf/CandidateUI.cpp"'
            if probe.count(include) != 1:
                raise RuntimeError("Probe source include changed; update negative control")
            source = work / "legacy-probe.cpp"
            source.write_text(probe.replace(include, '#include "legacy-candidate-ui.cpp"'), encoding="utf-8")
            control = build_and_run(source, "legacy")
            expected = "First candidates did not publish final geometry atomically"
            if control.returncode == 0 or expected not in control.stderr:
                raise RuntimeError(f"Negative control did not detect original geometry bug: {control}")
            print('{"negative_control":"passed","legacy_visibility_only_gate":"rejected"}', flush=True)


if __name__ == "__main__":
    main()
