"""Real TSF sentence focus recovery, staged DLL and isolated synthetic schema."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from work_directory import temporary_work_directory
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform',choices=['x64','ARM64','Win32'],default='x64')
    parser.add_argument('--model',type=Path,default=ROOT/'data/Models/sentence-fivegram-mobile.bin')
    args=parser.parse_args()
    if os.name!='nt':parser.error('Requires Windows')
    binaries=ROOT/'build/tests'/args.platform
    importer=binaries/'Tigirl.Import.exe';host=binaries/'candidate_sentence_focus_host.exe'
    dll=ROOT/'build'/args.platform/'Release/Tigirl.dll'
    for p in (importer,host,dll,args.model):
        if not p.is_file():parser.error(f'Missing input: {p}')
    with temporary_work_directory(prefix='tigirl-sentence-focus-') as root:
        source=root/'source';source.mkdir();user=root/'user';user.mkdir();package=root/'package';package.mkdir()
        (source/'词条.txt').write_text('aa 中 100\naa 人 90\nbb 国 100\nbb 华 90\n',encoding='utf-8')
        (source/'补充语料.txt').write_text('中国 1000000\n中华 1000\n',encoding='utf-8')
        shutil.copytree(source,user/'码表/测试整句')
        subprocess.run([str(importer),'--schema',str(source),str(root/'pinyin'),str(user),'测试整句','zh-CN'],check=True,capture_output=True,timeout=120)
        subprocess.run([str(importer),str(source),str(root/'pinyin'),str(package/'tiger-v2.tcd'),'zh-CN'],check=True,capture_output=True,timeout=120)
        (user/'.candidate-layout-test').touch()
        # Focus tests keep ranking fixed; Engine focus tests separately verify
        # that real manual-learning evidence survives focus changes.
        (user/'config.txt').write_text('当前码表\t测试整句\n整句语言模型\t'+str(args.model.resolve())+'\n高频字仅使用最优码组句\t0\n整句Tab自学习\t否\n',encoding='utf-8-sig')
        staged=package/'Tigirl.dll';shutil.copy2(dll,staged)
        result=subprocess.run([str(host),str(staged),str(user)],capture_output=True,text=True,timeout=90)
        print(result.stdout,end='',flush=True)
        if result.returncode:raise RuntimeError(result.stderr)
        if json.loads(result.stdout).get('status')!='passed':raise RuntimeError('Invalid report')
if __name__=='__main__':main()
