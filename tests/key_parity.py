"""Generate real-table key traces and compare the original Core to native C++."""
import json
from pathlib import Path
import random
import subprocess
import shutil
import tempfile
from windows_process import run_windows

ROOT = Path(__file__).resolve().parents[1]
WINROOT = subprocess.check_output(["wslpath", "-w", str(ROOT)], text=True).strip()
BUILD = ROOT / "build"
keys = []

def case(name, events):
    for i, event in enumerate(events):
        keys.append(dict(event, reset=(i == 0), case=name))

def tap(vk, **modifiers):
    return [dict(vk=vk, action=action, **modifiers) for action in ("down", "up")]

def typing(text):
    result = []
    oem = {";":186, "'":222, "/":191, "[":219, "`":192, " ":32, ".":190, ",":188, "-":189, "=":187}
    for ch in text:
        vk = oem[ch] if ch in oem else ord(ch.upper())
        result += tap(vk, shift=ch.isupper())
    return result

main = []
adjustment_codes = []
with open(ROOT / "data/tiger-v2.tcd.expected.jsonl", encoding="utf-8-sig") as f:
    for line in f:
        entry = json.loads(line)
        if entry["section"] == 1 and entry["values"] and entry["key"].isascii() and entry["key"].isalpha():
            main.append(entry["key"])
            if len(entry["key"]) in (2, 3) and len(entry["values"]) >= 8:
                adjustment_codes.append(entry["key"])
rng = random.Random(20260908)
for code in rng.sample(main, 250):
    for ending in [" ", "1", "2", "0", ";", "'", ".", "=2", "a "]:
        case(code + repr(ending), typing(code + ending))
    for vk in [8, 9, 13, 27]:
        case(code + f" key {vk}", typing(code) + tap(vk))
for text in ["ni", "hao", "zhong", "guo", "shuru", "shijie", "zzz"]:
    for ending in [" ", "2", "'", "=3", "- "]:
        case("pinyin " + text + ending, typing("`"+text+ending))
case("pinyin escape grave", typing("``"))
case("pinyin backspace", typing("`ni") + tap(8) + tap(8))
for text in [";", "/", "[", "z", ";a", "/a", "[a", "zz", ";;", "//", "[["]:
    case("quick " + text, typing(text + " "))
for text in ["3.14", "''", "Abc123 ", "Abc,", "ABC "]:
    case("literal " + text, typing(text))
case("shift toggle raw", typing("ab") + tap(160) + typing("ab") + tap(160) + typing("ab "))
case("shift chord", tap(160)[:1] + typing("A") + tap(160)[1:] + typing("bc "))
case("ctrl space", tap(162, ctrl=True)[:1] + tap(32, ctrl=True) + tap(162)[1:] + typing("ab"))
case("shortcut cancels", typing("ab") + tap(162, ctrl=True)[:1] + tap(67, ctrl=True) + tap(162)[1:] + typing("a "))
for code in rng.sample(adjustment_codes, 12):
    for modifiers in [dict(ctrl=True), dict(ctrl=True, shift=True), dict(alt=True)]:
        for digit in [1, 2, 5, 9]:
            down = dict(vk=48+digit, action="down", **modifiers)
            events = [down, dict(down, repeat=3), dict(down, repeat=2), dict(down, action="up")]
            case(f"adjust {code} {modifiers} {digit}", typing(code) + events + typing(" "))
        case(f"adjust paged {code} {modifiers}", typing(code+"=") + tap(50, **modifiers) + typing(" "))

trace = BUILD / "keys.jsonl"
trace.write_text("".join(json.dumps(k) + "\n" for k in keys), encoding="utf-8")
with tempfile.TemporaryDirectory(prefix="key-oracle-", dir=BUILD) as isolated, open(BUILD / "keys-oracle.jsonl", "w", encoding="utf-8") as output:
    shutil.copytree(ROOT / "data/staging", isolated, dirs_exist_ok=True)
    isolated_windows = subprocess.check_output(["wslpath", "-w", isolated], text=True).strip()
    run_windows(["/mnt/c/Program Files/dotnet/dotnet.exe", WINROOT+r"\tools\ReferenceOracle\bin\Release\net10.0-windows\ReferenceOracle.dll",
                    "trace", isolated_windows, WINROOT+r"\build\keys.jsonl"], stdout=output, check=True)
rows = []
for k in keys:
    rows.append(" ".join(str(int(x)) for x in [k["reset"], k["vk"], k.get("scan",0), k["action"]=="down",
        k.get("shift",False),k.get("ctrl",False),k.get("alt",False),k.get("win",False),k.get("caps",False),
        k.get("num",True), k.get("repeat",1), k.get("extended",False)]))
(BUILD / "keys-native.tsv").write_text("\n".join(rows)+"\n")
with open(BUILD / "keys-native.jsonl", "w", encoding="utf-8") as output:
    run_windows([BUILD / "tests/ARM64/engine_probe.exe", WINROOT+r"\data\tiger-v2.tcd", WINROOT+r"\build\keys-native.tsv"], text=True, stdout=output, check=True)
failures = []
with open(BUILD / "keys-oracle.jsonl", encoding="utf-8-sig") as oracle, open(BUILD / "keys-native.jsonl", encoding="utf-8") as native:
    expected_rows = [json.loads(x) for x in oracle]
    actual_rows = [json.loads(x) for x in native]
assert len(expected_rows) == len(actual_rows) == len(keys)
for i, (expected, actual) in enumerate(zip(expected_rows, actual_rows)):
    result, state = expected["result"], expected["snapshot"]
    ui = state["Ui"]
    normalized = dict(handled=result["Handled"], cancel=result["CancelComposition"], chinese=ui["IsChinese"],
        mode=ui["CompositionState"], page=state["CandidatePageIndex"], raw=state["RawInput"],
        commit=result["TextToOutput"] or "", candidates=ui["Candidates"], annotations=ui["CandidateAnnotations"])
    if actual != normalized:
        failures.append(dict(index=i, case=keys[i]["case"], key=keys[i],
            differences={field:dict(expected=normalized[field],actual=actual.get(field)) for field in normalized if normalized[field] != actual.get(field)}))
report = dict(events=len(keys), mismatches=len(failures), failures=failures)
(BUILD / "key-parity.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(dict(events=len(keys),mismatches=len(failures),first=failures[:8]),ensure_ascii=False,indent=2))
raise SystemExit(bool(failures))
