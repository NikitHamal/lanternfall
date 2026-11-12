import sys; sys.path.insert(0,'/tmp/agentB_run/pylib')
from PIL import Image, ImageDraw
ids=open(sys.argv[1]).read().split(); out=sys.argv[2]
W=256;cols=6;rows=(len(ids)+cols-1)//cols
sh=Image.new('RGB',(cols*W,rows*W),(30,30,34));d=ImageDraw.Draw(sh)
for n,i in enumerate(ids):
    im=Image.open(f'shots/shortlist/{i}.png').convert('RGBA');im.thumbnail((W,W-14))
    bg=Image.new('RGBA',im.size,(60,60,66,255));bg.alpha_composite(im)
    x,y=(n%cols)*W,(n//cols)*W;sh.paste(bg.convert('RGB'),(x,y+14));d.text((x+2,y+1),i[:38],fill=(255,255,255))
sh.save(out);print(sh.size)
