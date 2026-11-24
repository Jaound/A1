"""고정된 Git 원본 68장을 검증하고 UE 반입용 이미지를 Saved에 생성한다 (Pillow, NumPy)."""
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[1]
REVISION = "e9ac1995f0738c88087a37738a29b7cc65c50277"
SOURCE = "art/characters/source"
OUT = PROJECT / "Saved/HeroArt34/Prepared"
# 전신 원화에서 눈높이·흉상을 확인한 정사각형 초상 중심 X 및 상단 Y (원본 비율).
FACES = {
    "C01": (.465, .001), "C02": (.53, .009), "C03": (.46, .005),
    "C04": (.51, .005), "C05": (.51, .002), "G01": (.49, .009),
    "G02": (.48, .008), "G03": (.49, .004), "G04": (.49, .003),
    "M01": (.48, .013), "M03": (.49, .0), "M04": (.49, .0),
    "M05": (.49, .009), "M06": (.50, .009), "M07": (.52, .001),
    "M08": (.51, .012), "M09": (.50, .004), "M10": (.52, .012),
    "R01": (.40, .026), "R02": (.48, .002), "R03": (.50, .014),
    "R04": (.48, .002), "R05": (.49, .012), "R06": (.46, .013),
    "R07": (.46, .0), "R08": (.50, .002), "R09": (.50, .0),
    "R10": (.46, .0), "S01": (.50, .009), "S02": (.46, .012),
    "S03": (.48, .005), "S04": (.50, .011), "S05": (.48, .0),
}


def source_bytes(path):
    return subprocess.check_output(["git", "show", f"{REVISION}:{SOURCE}/{path}"], cwd=REPO)


# 512px 기준. 무기·의상에 둘러싸여 외곽과 닿지 않는 흰 배경을 원화별로 확인했다.
INNER_BACKGROUND = {
    "C01": [(197, 469)],
    "C02": [(202, 458), (71, 79)],
    "C03": [(243, 481)],
    "C04": [(253, 486)],
    "C05": [(209, 114), (267, 481)],
    "G01": [(232, 472)],
    "G02": [(219, 455)],
    "G03": [(173, 460)],
    "G04": [(196, 465)],
    "M01": [(155, 458)],
    "M03": [(170, 426)],
    "M05": [(226, 470)],
    "M06": [(244, 458)],
    "M07": [(246, 480)],
    "M08": [(292, 480), (139, 480)],
    "M09": [(243, 465)],
    "R01": [(227, 472)],
    "R02": [(186, 454)],
    "R03": [(269, 458)],
    "R04": [(264, 466)],
    "R05": [(229, 470)],
    "R06": [(226, 462)],
    "R07": [(208, 457)],
    "R08": [(230, 480)],
    "R09": [(223, 456)],
    "R10": [(214, 471)],
    "S01": [(179, 462)],
    "S02": [(184, 461)],
    "S04": [(199, 468)],
    "S05": [(197, 470)],
}


def prepare_sd(im, key):
    # 흰 의상을 보호하기 위해 화면 외곽과 연결된 밝은 중성 배경만 분리한다.
    image = im.convert("RGBA").resize((512, 512), Image.Resampling.LANCZOS)
    rgba = np.array(image)
    if rgba[:, :, 3].min() < 255:
        return image
    rgb = rgba[:, :, :3].astype(np.float32)
    low, high = rgb.min(axis=2), rgb.max(axis=2)
    mask = Image.fromarray(((low >= 234) & (high - low <= 17)).astype("uint8") * 255)
    # 패딩을 넣어 서로 분리된 네 변도 같은 바깥 배경으로 연결한다.
    padded = Image.new("L", (514, 514), 255)
    padded.paste(mask, (1, 1))
    ImageDraw.floodfill(padded, (0, 0), 128)
    for x, y in INNER_BACKGROUND.get(key, ()):
        if padded.getpixel((x+1,y+1)) == 255:
            ImageDraw.floodfill(padded,(x+1,y+1),128)
    exterior = np.asarray(padded)[1:-1, 1:-1] == 128
    near = np.asarray(Image.fromarray(exterior.astype("uint8") * 255).filter(ImageFilter.MaxFilter(3))) > 0
    alpha = np.where(exterior, 0, np.where(near, np.clip((255-low)/55, 0, 1), 1))
    rgba[:, :, :3] = np.clip(255+(rgb-255)/np.maximum(.01, alpha[:, :, None]), 0, 255).astype("uint8")
    rgba[:, :, 3] = np.rint(alpha*255).astype("uint8")
    return Image.fromarray(rgba)


def main():
    from io import BytesIO
    manifest = json.loads(source_bytes("manifest.json"))
    roster = json.loads((PROJECT/"Content/HeroData/Roster.json").read_text("utf-8"))
    names = {hero["id"]: hero["name"] for hero in roster["heroes"]}
    seen = set()
    for entry in manifest["files"]:
        key, variant = entry["character_id"], entry["variant"]
        assert key in names and entry["character_name"] == names[key]
        assert variant in ("sd", "standing") and (key, variant) not in seen
        seen.add((key, variant))
        data = source_bytes(entry["path"])
        assert len(data) == entry["size_bytes"] and hashlib.sha256(data).hexdigest() == entry["sha256"]
        image = Image.open(BytesIO(data))
        assert image.size == (entry["width"], entry["height"])
        if key == "M02":
            original = REPO/"reference/HeroArt/M02/Source"/("SD.png" if variant=="sd" else "Standing.png")
            assert hashlib.sha256(original.read_bytes()).digest() == hashlib.sha256(data).digest()
            continue
        target = OUT/key
        target.mkdir(parents=True, exist_ok=True)
        if variant == "sd":
            prepare_sd(image, key).save(target/"sd.png")
        else:
            w, h = image.size
            cx, top = FACES[key]
            size = int(.31*w)
            x, y = int(cx*w-size/2), int(top*h)
            image.crop((x,y,x+size,y+size)).resize((256,256),Image.Resampling.LANCZOS).save(target/"portrait.png")
            # UI에서 원본 구도 전체를 보존하고 렌더용 해상도만 제한한다.
            image.convert("RGB").resize((768,1024),Image.Resampling.LANCZOS).save(target/"standing.png")
    assert seen == {(key, kind) for key in names for kind in ("standing","sd")}
    (OUT/"Verified.json").write_text(json.dumps({"revision": REVISION, "prepared": sorted(set(names)-{"M02"})}), "utf-8")


if __name__ == "__main__":
    main()
