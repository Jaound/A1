"""원본 픽셀을 보존하고 지정한 배경 영역만 알파로 분리한다. Pillow, NumPy 사용."""
from pathlib import Path
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
root=Path(__file__).parent
src=Image.open(root/"Source"/"SD.png").convert("RGB")
rgb=np.asarray(src).astype(float)
# 연결된 배경을 외곽과 닫힌 빈 공간의 표본으로 지정한다. 의상 내부에는 시드가 없다.
neutral=(rgb.min(2)>236)&(rgb.max(2)-rgb.min(2)<14)
mask=Image.fromarray((neutral*255).astype("uint8")).copy()
seeds=[(0,0),(.48,.32),(.52,.39),(.32,.46),(.47,.76),(.63,.67),(.84,.36),(.46,.09)]
seeds += [(x/320,y/320) for x,y in [(250,64),(78,73),(102,77),(128,168),(96,178),(113,163),(211,114)]]
for x,y in seeds:
    point=(int(x*src.width),int(y*src.height))
    if mask.getpixel(point)==255: ImageDraw.floodfill(mask,point,128)
bg=np.asarray(mask)==128
# 바닥 그림자는 신발 바로 아래와 신발 사이에만 있다.
shadow=Image.new("L",src.size)
draw=ImageDraw.Draw(shadow)
for poly in [[(.04,.931),(.098,.929),(.096,.953),(.045,.951)],
             [(.187,.933),(.71,.919),(.737,.944),(.734,.962),(.17,.962)],
             [(.825,.924),(.88,.924),(.88,.963),(.821,.959)]]:
    draw.polygon([(int(x*src.width),int(y*src.height)) for x,y in poly],fill=255)
dark=rgb[:,:,0]-rgb[:,:,1]
bg |= (np.asarray(shadow)>0)&(dark>4)&(dark<45)&(rgb[:,:,0]>110)&(rgb[:,:,0]<213)
yy,xx=np.indices(bg.shape)
bg |= ((yy/src.height>.919)&(yy/src.height<.967)&(xx/src.width>.187)&(xx/src.width<.69))
near=np.asarray(Image.fromarray((bg*255).astype("uint8")).filter(ImageFilter.MaxFilter(7)))>0
alpha=np.where(bg,0,np.where(near,np.clip((255-rgb.min(2))/58,0,1),1))
unmatte=np.clip(255+(rgb-255)/np.maximum(.01,alpha[:,:,None]),0,255)
sd=Image.fromarray(np.dstack((unmatte,alpha*255)).astype("uint8"))
sd.save(root/"Prepared"/"SD.png")
im=Image.open(root/"Source"/"Standing.png")
w,h=im.size
left,top,width=json.loads((root/"Art.json").read_text("utf-8"))["faceCrop"]
im.crop((int(left*w),int(top*h),int((left+width)*w),int(top*h+width*w))).resize((384,384),Image.Resampling.LANCZOS).save(root/"Prepared"/"Portrait.png")
