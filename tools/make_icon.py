"""Convert the supplied transparent logo to a multi-resolution Windows icon."""
from pathlib import Path
from PIL import Image
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('source',type=Path)
parser.add_argument('output',type=Path)
parser.add_argument('--wizard', action='store_true', help='Also create Inno wizard bitmaps beside the icon')
parser.add_argument('--wizard-large-source', type=Path, help='Separate artwork for the large wizard bitmap')
args=parser.parse_args()
image=Image.open(args.source).convert('RGBA')
# A small transparent margin prevents the silhouette touching the icon bounds.
image.thumbnail((240,240),Image.Resampling.LANCZOS)
canvas=Image.new('RGBA',(256,256))
canvas.alpha_composite(image,((256-image.width)//2,(256-image.height)//2))
canvas.save(args.output,format='ICO',sizes=[(s,s) for s in (16,20,24,32,40,48,64,96,128,256)])

if args.wizard:
    original=Image.open(args.source).convert('RGBA')
    for suffix,size,thumb in [('wizard',(164,314),144),('wizard-small',(55,55),49)]:
        bitmap=Image.new('RGB',size,(255,250,243))
        logo=(Image.open(args.wizard_large_source).convert('RGBA')
              if suffix == 'wizard' and args.wizard_large_source else original.copy())
        logo.thumbnail((thumb,thumb),Image.Resampling.LANCZOS)
        bitmap.paste(logo,((size[0]-logo.width)//2,(size[1]-logo.height)//2),logo)
        bitmap.save(args.output.with_name(args.output.stem+'-'+suffix+'.bmp'))
