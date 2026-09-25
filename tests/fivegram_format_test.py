"""Small, independently constructed Q8 wire fixtures; no full model required."""
import argparse, math, struct, subprocess, tempfile
from pathlib import Path

def model():
    data=bytearray(256)
    def put(fmt,offset,*values):struct.pack_into('<'+fmt,data,offset,*values)
    data[:8]=b'TCSKNM03'
    put('II',8,2,256);put('IIII',24,5,5,256,16)
    put('Q',40,256);put('HHH',56,0,1,2)
    for i in range(5):put('iIiI',160+16*i,-300000000,100000000,-20000000,10000000)
    for text,p in [('<unk>',0),('<s>',1),('</s>',50),('中',255),('国',100)]:
        b=text.encode();data.extend(struct.pack('<H',len(b))+b+bytes([p,0]))
    put('Q',48,len(data)-256)
    offsets={}
    for section in range(4):
        directory=len(data);data.extend(bytes(256*40))
        put('QQQ',64+24*section,directory,2 if section==0 else 0,3 if section==0 else 0)
        if section:continue
        for ctx,bow,successors in [(1,1,[(2,50),(3,200)]),(3,0,[(3,255)])]:
            block=len(data);data.extend(struct.pack('<HBH',ctx,bow,len(successors)))
            for token,p in successors:data.extend(struct.pack('<HB',token,p))
            index=len(data);data.extend(struct.pack('<4HQ',ctx,0,0,0,block))
            put('QQQIIQ',directory+ctx*40,block,index-block,index,1,1,len(successors))
            offsets[ctx]=(block,index,directory+ctx*40)
    put('Q',16,len(data));return data,offsets

def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);a=p.parse_args()
    probe=str(a.probe.resolve())
    with tempfile.TemporaryDirectory(prefix='tigirl-q8-format-') as d:
        root=Path(d);data,offsets=model();valid=root/'valid.bin';valid.write_bytes(data)
        # Final-step natural-log scores: observed, nonzero backoff, exact zero
        # backoff, max Q8 code, OOV, BOS/EOS and missing higher-order contexts.
        rows=[(-10,['中']),(-22,['国']),(-20,['中','国']),(-4.5,['中','中']),(-25,['\x03']),(-32,['unknown']),(-20,['中','中','中','中','国'])]
        cases=root/'scores.tsv';cases.write_text(''.join(format(score*math.log(10),'.17g')+'\t'+'\t'.join(tokens)+'\n' for score,tokens in rows),encoding='utf-8')
        subprocess.run([probe,'oracle',str(valid),str(cases)],check=True)
        variants={}
        for version in (0,1,3):
            bad=bytearray(data);struct.pack_into('<I',bad,8,version);variants[f'version-{version}']=bad
        block,index,meta=offsets[1]
        for name,fmt,offset,value in [('successor-count','H',block+3,65535),('index-offset','Q',index+8,len(data)+1),('vocab-length','H',256,65535),('section-offset','Q',64,len(data)+1)]:
            bad=bytearray(data);struct.pack_into('<'+fmt,bad,offset,value);variants[name]=bad
        bad=bytearray(data[:-1]);struct.pack_into('<Q',bad,16,len(bad));variants['truncated-directory']=bad
        for name,bad in variants.items():
            f=root/(name+'.bin');f.write_bytes(bad);subprocess.run([probe,'reject',str(f)],check=True)
        print(f'PASS: Q8 wire scores and {len(variants)} malformed/unsupported formats')
if __name__=='__main__':main()
