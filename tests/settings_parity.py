"""Compare actual original config-file reload getters with native parsing."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/'build'
def win(path): return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
flags=['默认中文','shift切换中英文','Ctrl+空格切换中英文','中文状态下使用英文标点','/输出顿号','回车清屏','TAB清屏','空码自动清屏','最大码长无重自动上屏','`键拼音反查','分号次选','引号三选','显示注释','显示拆分']
flags += ['竖排候选','显示候选序号','候选窗显示编码','隐藏候选']
cases=['','# comment\r\n','未知配置 否\n默认中文', ' 默认中文\t否\r默认中文,是\n默认中文 off', (ROOT/'data/staging/config.txt').read_text(encoding='utf-8-sig')]
for key in flags:
    for value in ['是','否','TRUE','fAlSe','on','OFF','1','0','','yes','是 #comment','无效']:
        cases.append(key+'\t'+value)
for key in ['最大码长','每页候选个数']:
    for value in ['','0','-1','1','5','10','16','17','200','+3','0004','2147483647','-2147483648','2147483648','-2147483649','3.5','0x04',' 3 ','4 #comment']:
        cases.append(key+','+value)
for value in ['- =','[ ]','Shift Tab/Tab','PageUp/PageDown','pageup/pagedown','','invalid']:
    cases.append('翻页键 '+value)
cases += ['\u3000默认中文\u00a0 否\u3000','默认中文, 否\n默认中文 是','默认中文=否','默认中文:否','默认中文\t\t否', '默认中文,,否']
cases += ['字体 '+x for x in ['', 'Microsoft YaHei UI', '#霞鹜文楷 GB 屏幕阅读版', '字体名#保留井号']]
cases += ['字体大小 '+x for x in ['', '2', '3', '17.5', '200', '201', '-20', '1e2', 'abc']]
cases += ['主题 '+x for x in ['', '默认', '通透', '一般通透', '迷雾', '星夜', '纸', '粉', '赛博朋克', '清晨', 'unknown']]
trace=BUILD/'settings-cases.jsonl'; trace.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in cases),encoding='utf-8')
hexfile=BUILD/'settings-cases.hex'; hexfile.write_text(''.join(x.encode().hex()+'\n' for x in cases))
with tempfile.TemporaryDirectory(prefix='settings-oracle-',dir=BUILD) as temp:
    shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
    output=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'settings',win(temp),win(trace)],text=True,encoding='utf-8-sig')
(BUILD/'settings-oracle.jsonl').write_text(output,encoding='utf-8')
expected=[json.loads(x) for x in output.splitlines()]
for exe in [BUILD/'settings_probe',BUILD/'tests/ARM64/settings_probe.exe']:
    if not exe.exists(): continue
    actual=[json.loads(x) for x in subprocess.check_output([str(exe),win(hexfile) if exe.suffix=='.exe' else str(hexfile)],text=True).splitlines()]
    assert len(expected)==len(actual)==len(cases)
    failures=[dict(case=i,text=cases[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
    result=dict(cases=len(cases),mismatches=len(failures),failures=failures)
    suffix='arm64' if exe.suffix=='.exe' else 'linux'
    (BUILD/f'settings-parity-{suffix}.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(dict(platform=suffix,cases=len(cases),mismatches=len(failures),first=failures[:2]),ensure_ascii=False))
    assert not failures
