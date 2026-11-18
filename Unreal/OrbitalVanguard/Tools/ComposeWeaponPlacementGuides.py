"""Blender 정사영을 사용자가 표시할 수 있는 한국어 4면 도면으로 조합한다."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont, ImageOps

root=Path.home()/'Documents/OrbitalVanguardAssets/WeaponPlacementGuide_v1'
rows=json.loads((root/'views/report.json').read_text(encoding='utf8'))
names={'corvette':'초계함','frigate':'호위함','vector':'백터급 구축함','bastion':'바스티온급 구축함',
       'cruiser':'순양함','air_defense':'방공함','radar':'레이더함','battleship':'전함',
       'carrier':'항공모함','missile_ship':'전략 미사일함','dreadnought':'구축전함','battlecarrier':'항공순양함',
       'repair':'보급수리함','mining':'채굴선','transport':'수송선','assault':'강습선'}
labels={'top':'위에서','bottom':'아래에서','left':'왼쪽 측면','right':'오른쪽 측면'}
folder=root/'도면';folder.mkdir(parents=True,exist_ok=False)
font='C:/Windows/Fonts/malgun.ttf';title=ImageFont.truetype(font,32);normal=ImageFont.truetype(font,24);small=ImageFont.truetype(font,20)
for ident,name in names.items():
    by_view={row['view']:row for row in rows if row['id']==ident}
    if set(by_view)!=set(labels):raise RuntimeError(ident+': 4면 이미지 누락')
    sheet=Image.new('RGB',(1840,1250),'#101d2b');draw=ImageDraw.Draw(sheet)
    draw.text((30,20),name+'  |  무장 위치 표시용',font=title,fill='#f1f8ff')
    draw.text((30,70),'각 위치에 번호를 적고, 위·아래와 측면의 같은 위치에는 같은 번호를 사용해 주세요.',font=normal,fill='#c9d9e8')
    for index,key in enumerate(('top','bottom','left','right')):
        x=20+(index%2)*910;y=120+(index//2)*540
        draw.rounded_rectangle((x,y,x+900,y+520),radius=7,fill='#1d2d3b',outline='#456177',width=2)
        draw.text((x+16,y+8),labels[key],font=normal,fill='#eaf5ff')
        draw.text((x+650,y+10),'앞쪽  →',font=normal,fill='#6be2f4')
        row=by_view[key]
        with Image.open(row['file']) as im:
            view=im.convert('RGB')
            if row['flip_x']:view=ImageOps.mirror(view)
            sheet.paste(view,(x,y+50))
    draw.text((30,1210),'예: 1번 함포 T2, 위쪽, 앞을 향함  /  2번 요격포, 왼쪽 측면',font=small,fill='#c9d9e8')
    sheet.save(folder/(name+'.png'),optimize=True)
print('배치 도면',len(names),'개:',folder)
