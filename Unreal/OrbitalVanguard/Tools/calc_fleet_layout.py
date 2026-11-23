import math

cam_loc = (-3400.0, -1500.0, 760.0)
cam_rot_p = -10.0
cam_rot_y = 21.0
w = 1280.0
h = 720.0
fov_h = math.radians(42.0)
tan_half_h = math.tan(fov_h / 2.0)
tan_half_v = tan_half_h / (16.0 / 9.0)

# Camera basis vectors
cp = math.cos(math.radians(cam_rot_p))
sp = math.sin(math.radians(cam_rot_p))
cy = math.cos(math.radians(cam_rot_y))
sy = math.sin(math.radians(cam_rot_y))

cfwd = (cp*cy, cp*sy, sp)
cright = (-sy, cy, 0.0)
cup = (-cy*sp, -sy*sp, cp)

def screen_to_world(target_px, target_py, fwd_dist):
    # px = (ndc_x + 1) * 0.5 * w -> ndc_x = (px / (0.5 * w)) - 1
    ndc_x = (target_px / (0.5 * w)) - 1.0
    ndc_y = 1.0 - (target_py / (0.5 * h))
    
    right = ndc_x * tan_half_h * fwd_dist
    up = ndc_y * tan_half_v * fwd_dist
    
    # World = CamLoc + CamForward * fwd_dist + CamRight * right + CamUp * up
    wx = cam_loc[0] + cfwd[0] * fwd_dist + cright[0] * right + cup[0] * up
    wy = cam_loc[1] + cfwd[1] * fwd_dist + cright[1] * right + cup[1] * up
    wz = cam_loc[2] + cfwd[2] * fwd_dist + cright[2] * right + cup[2] * up
    return (wx, wy, wz)

# We want ships positioned in the right 55% of the screen (x from 580 to 1200):
# Looking at reference media_1795099219552_61a8f429.jpg:
# - Center Foreground Hero: Dreadnought at px=(950, 480), dist=3400, target size ~440px
# - Center-Left Capital: Battlecarrier at px=(680, 360), dist=3800, target size ~360px
# - Center-Right Capital: Battleship at px=(1100, 380), dist=3900, target size ~320px
# - High-Left Escort: Cruiser 1 at px=(580, 260), dist=4500, target size ~220px
# - Mid-High Carrier: Carrier at px=(820, 240), dist=4800, target size ~280px
# - High-Right Escort: Cruiser 2 at px=(1020, 220), dist=5000, target size ~200px
# - Distant Destroyer 1: Bastion at px=(720, 180), dist=5600, target size ~140px
# - Distant Destroyer 2: Vector at px=(920, 170), dist=5800, target size ~130px
# - Flank Escort: AirDefense at px=(1180, 280), dist=4600, target size ~160px
# - Low Flank Frigate: Frigate 1 at px=(1120, 560), dist=3600, target size ~180px
# - Mid-Low Frigate: Frigate 2 at px=(820, 520), dist=4200, target size ~140px
# - Fighter Wing (3 ships flying in wedge near hero ship):
#   Lead: px=(840, 430), dist=3200, size ~90px
#   Wing L: px=(870, 410), dist=3300, size ~80px
#   Wing R: px=(860, 450), dist=3300, size ~80px

desired_fleet = [
    # (Role, Label, MeshPath, target_px, target_py, dist, target_size_px, rot)
    ("Flagship", "구축전함 (Dreadnought)", "/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull", 960.0, 490.0, 3400.0, 460.0, (12.0, 65.0, -10.0)),
    ("Battlecarrier", "전투항공모함 (Battlecarrier)", "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull", 680.0, 360.0, 3800.0, 360.0, (12.0, 65.0, -10.0)),
    ("Battleship", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 1120.0, 380.0, 3900.0, 330.0, (12.0, 65.0, -10.0)),
    ("Cruiser1", "순양함 1 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 580.0, 270.0, 4500.0, 230.0, (12.0, 65.0, -10.0)),
    ("Carrier", "항공모함 (Carrier)", "/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull", 830.0, 240.0, 4800.0, 280.0, (12.0, 65.0, -10.0)),
    ("Cruiser2", "순양함 2 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 1040.0, 230.0, 5000.0, 210.0, (12.0, 65.0, -10.0)),
    ("Bastion", "바스티온급 구축함 (Bastion)", "/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull", 710.0, 190.0, 5500.0, 160.0, (12.0, 65.0, -10.0)),
    ("Vector", "벡터급 구축함 (Vector)", "/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull", 910.0, 175.0, 5800.0, 150.0, (12.0, 65.0, -10.0)),
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 1180.0, 280.0, 4600.0, 180.0, (12.0, 65.0, -10.0)),
    ("Frigate1", "호위함 1 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 1140.0, 540.0, 3600.0, 190.0, (12.0, 65.0, -10.0)),
    ("Frigate2", "호위함 2 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 800.0, 510.0, 4100.0, 150.0, (12.0, 65.0, -10.0)),
    ("Fighter1", "함재기 1 (Lead)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 830.0, 430.0, 3100.0, 95.0, (14.0, 68.0, -14.0)),
    ("Fighter2", "함재기 2 (Wing L)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 860.0, 410.0, 3200.0, 85.0, (14.0, 68.0, -14.0)),
    ("Fighter3", "함재기 3 (Wing R)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 850.0, 450.0, 3200.0, 85.0, (14.0, 68.0, -14.0)),
]

print("GENERATED FLEET SLOTS:")
cpp_lines = []
for role, label, mesh, px, py, dist, target_size, rot in desired_fleet:
    loc = screen_to_world(px, py, dist)
    # size = (base_len * scale / dist) / tan_half_h * (w / 2)
    # base_len for ships is ~500-600, let's normalize scale
    scale = (target_size / (w / 2.0)) * tan_half_h * dist / 550.0
    scale = round(scale, 2)
    loc_str = f"FVector({loc[0]:.1f}f, {loc[1]:.1f}f, {loc[2]:.1f}f)"
    rot_str = f"FRotator({rot[0]:.1f}f, {rot[1]:.1f}f, {rot[2]:.1f}f)"
    cpp_lines.append(f'            AddSlot(TEXT("{role}"), TEXT("{label}"), TEXT("{mesh}"), {loc_str}, {rot_str}, {scale:.2f}f);')
    print(f"  {role:15s}: Screen=({px:.0f}, {py:.0f}), dist={dist:.0f}, Scale={scale:.2f} -> Loc={loc_str}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/gen_slots.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(cpp_lines))
