"""Check all original palette constants and independent source-over pixel math."""
import json,re,subprocess,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/'build'
source=Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/next/TigerClaw.Overlay/OverlayThemeResolver.cs')
text=source.read_text(encoding='utf-8-sig')
rows=re.findall(r'\{ "([^"\n]+)", new OverlayThemePalette\("([A-F0-9]+)", "([A-F0-9]+)", "([A-F0-9]+)", "([A-F0-9]+)", ([0-9.]+), new CornerRadius\(([^)]+)\)\)',text)
assert len(rows)==9
expected={}
for name,*values in rows:
 name=name.encode().decode('unicode_escape'); corners=[float(x.strip()) for x in values[5].split(',')]
 if len(corners)==1:corners*=4
 expected[name]=dict(colors=[int(x,16) for x in values[:4]],border=float(values[4]),corners=corners)
expected['unknown']=expected['默认']
def over(target,source,coverage=255):
 a=((source>>24)*coverage+127)//255
 channels=[(((source>>s)&255)*a+((target>>s)&255)*(255-a)+127)//255 for s in (0,8,16)]
 return sum(v<<s for v,s in zip(channels,(0,8,16)))|((a+((target>>24)*(255-a)+127)//255)<<24)
for exe in [BUILD/'theme_probe',BUILD/'tests/ARM64/theme_probe.exe']:
 if not exe.exists():continue
 run=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
 actual=[json.loads(x) for x in run.stdout.splitlines()];assert len(actual)==10
 for row in actual:
  want=expected[row['name']];colors=[int(x,16) for x in row['colors']]
  assert colors[:4]==want['colors'] and row['border']==want['border'] and row['corners']==want['corners']
  fg,bg,border,selection=colors[:4];base=over(0,bg)
  assert colors[4:]==[0,base,over(base,selection),over(base,fg),over(base,fg,128),over(base,border)],row
 suffix='arm64' if exe.suffix=='.exe' else 'linux'
 result=dict(palettes=9,unknown_fallback=True,pixel_cases=60,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),timing=run.stderr.strip(),status='passed')
 (BUILD/f'theme-parity-{suffix}.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
