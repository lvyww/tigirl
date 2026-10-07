#!/usr/bin/env python3
"""Generate Tigirl's original default SSF. No external artwork or fonts."""
import pathlib, struct, zipfile, zlib


def chunk(kind, data):
    return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))


def build(destination):
    width, height, radius = 96, 40, 8
    rows=bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            cover=0
            for dx,dy in ((.25,.25),(.75,.25),(.25,.75),(.75,.75)):
                px,py=x+dx,y+dy
                cx=min(max(px,radius),width-radius)
                cy=min(max(py,radius),height-radius)
                cover+=(px-cx)**2+(py-cy)**2<=radius**2
            a=round(cover*255/4)
            edge=x==0 or y==0 or x==width-1 or y==height-1
            rows.extend((222,209,190,a) if edge else (255,250,240,a))
    image=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(bytes(rows),9))+chunk(b'IEND',b'')
    text='[General]\nskin_name=虎娘默认\nskin_author=虎娘\n[Display]\nfont_size=17\npinyin_color=0x4d5465\nzhongwen_first_color=0x2d6ca3\nzhongwen_color=0x2f333a\ncomphint_color=0x727b82\n'
    for section in ('Scheme_H1','Scheme_V1'):
        text+=f'[{section}]\npic=panel.png\nlayout_horizontal=0,12,12\nlayout_vertical=0,12,12\npinyin_marge=6,2,10,10\nzhongwen_marge=3,6,10,10\n'
    for section in ('Scheme_H2','Scheme_V2'):
        text+=f'[{section}]\n'
        for prefix in ('pinyin','zhongwen'):
            text+=f'{prefix}_pic=panel.png\n{prefix}_layout_horizontal=0,12,12\n{prefix}_layout_vertical=0,12,12\n{prefix}_marge=6,6,10,10\n'
    destination.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(destination,'w') as output:
        for name,data in [('skin.ini',text.encode('utf-8')),('panel.png',image)]:
            info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16;output.writestr(info,data)
    return destination


if __name__=='__main__':
    print(build(pathlib.Path(__file__).resolve().parents[1]/'resources'/'Skins'/'默认.ssf'))
