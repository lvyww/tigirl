"""Original-Core live mutation semantics, including empty keys and aliases."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
def win(path):
    return subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip()

ops = []
queries = [";nt", ";ntx", ";ntxy", ";ntxyz", "a", "aznative", "native", "nativealias", "nativeescape"]
def op(kind, code, text):
    ops.append(dict(kind=kind, code=code, text=text, queries=queries))

op("add", ";nt", "父")
op("add", ";ntxyz", "子")
op("delete", ";ntxyz", "子")
op("add", ";ntx", "中")
op("delete", ";ntx", "中")
op("add", " AZNative ", "测试")
op("delete", "aznative", "测试")
for text in ["一", "二", "三", "四"]:
    op("add", "native", text)
for kind, text in [("advance", "三"), ("top", "四"), ("top", "四"), ("delete", "二"),
                   ("delete", "二"), ("advance", "四"), ("advance", "不存在"), ("top", "新增")]:
    op(kind, "native", text)
op("add", "nativealias", "显示=>内容")
op("add", "nativealias", "其他")
op("top", "nativealias", "内容")
op("advance", "nativealias", "其他")
op("add", "nativealias", "新显示=>内容")
op("delete", "nativealias", "内容")
op("add", "nativeescape", r"标签=>第一行\n第二行\s\t\\字𠀀")
op("top", "nativeescape", "第一行\r\n第二行 \t\\字𠀀")

source = BUILD / "lexicon-operations.jsonl"
source.write_text("".join(json.dumps(x, ensure_ascii=True)+"\n" for x in ops))
def hex16(text):
    raw = text.encode("utf-16-be")
    return raw.hex()
trace = BUILD / "lexicon-operations.tsv"
trace.write_text("".join(" ".join([str(["add", "delete", "top", "advance"].index(x["kind"])),
    hex16(x["code"]), hex16(x["text"]), *map(hex16, queries)])+"\n" for x in ops))
with tempfile.TemporaryDirectory(prefix="lexicon-oracle-", dir=BUILD) as isolated:
    shutil.copytree(ROOT / "data/staging", isolated, dirs_exist_ok=True)
    expected = subprocess.check_output(["/mnt/c/Program Files/dotnet/dotnet.exe",
        win(ROOT / "tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll"),
        "changes", win(isolated), win(source)], text=True, encoding="utf-8-sig")
actual = subprocess.check_output([str(BUILD / "lexicon_probe"), str(ROOT / "data/tiger-v2.tcd"), str(trace)], text=True)
(BUILD / "lexicon-oracle.jsonl").write_text(expected, encoding="utf-8")
(BUILD / "lexicon-native.jsonl").write_text(actual, encoding="utf-8")
expected = list(map(json.loads, expected.splitlines()))
actual = list(map(json.loads, actual.splitlines()))
assert len(expected) == len(actual) == len(ops)
failures = [dict(index=i, operation=ops[i], expected=a, actual=b) for i, (a, b) in enumerate(zip(expected, actual)) if a != b]
report = dict(operations=len(ops), mismatches=len(failures), failures=failures)
(BUILD / "lexicon-parity.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False, indent=2))
raise SystemExit(bool(failures))
