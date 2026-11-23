import math

CamLoc = (-3400.0, -1500.0, 760.0)
CamPitch = -10.0
CamYaw = 21.0
p = math.radians(CamPitch)
y = math.radians(CamYaw)
CamFwd = (math.cos(p)*math.cos(y), math.cos(p)*math.sin(y), math.sin(p))
CamRight = (-math.sin(y), math.cos(y), 0.0)
CamUp = (
    CamFwd[1]*CamRight[2] - CamFwd[2]*CamRight[1],
    CamFwd[2]*CamRight[0] - CamFwd[0]*CamRight[2],
    CamFwd[0]*CamRight[1] - CamFwd[1]*CamRight[0]
)
CameraFov = 48.0
fov_x = math.radians(CameraFov)
aspect = 16.0 / 9.0
fov_y = 2.0 * math.atan(math.tan(fov_x/2.0) / aspect)
tan_half_x = math.tan(fov_x / 2.0)
tan_half_y = math.tan(fov_y / 2.0)

def screen_to_world(fwd_dist, screen_x, screen_y):
    ndc_x = (screen_x - 0.5) * 2.0
    ndc_y = (0.5 - screen_y) * 2.0
    r_dist = ndc_x * fwd_dist * tan_half_x
    u_dist = ndc_y * fwd_dist * tan_half_y
    x = CamLoc[0] + CamFwd[0]*fwd_dist + CamRight[0]*r_dist + CamUp[0]*u_dist
    y_pos = CamLoc[1] + CamFwd[1]*fwd_dist + CamRight[1]*r_dist + CamUp[1]*u_dist
    z = CamLoc[2] + CamFwd[2]*fwd_dist + CamRight[2]*r_dist + CamUp[2]*u_dist
    return (round(x, 1), round(y_pos, 1), round(z, 1))

print("=== TASK FORCE 100 SLOTS ===")
ships100 = [
    ("LeadBattleship", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 2800, 0.74, 0.52, 2.7),
    ("Cruiser", "순양함 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 3500, 0.48, 0.38, 2.4),
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 3800, 0.88, 0.36, 2.1),
    ("Vector", "백터급 구축함 (Vector)", "/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull", 4500, 0.66, 0.24, 2.0),
]
for tag, name, mesh, dist, sx, sy, scale in ships100:
    pos = screen_to_world(dist, sx, sy)
    print(f'            AddSlot(TEXT("{tag}"), TEXT("{name}"), TEXT("{mesh}"), FVector({pos[0]}f, {pos[1]}f, {pos[2]}f), FRotator(15.f, 82.f, -12.f), {scale}f);')

print("\n=== FLAGSHIP VANGUARD SLOTS ===")
ships200 = [
    ("Flagship", "구축전함 (Dreadnought)", "/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull", 2600, 0.74, 0.56, 2.8, (15.0, 82.0, -12.0)),
    ("Battlecarrier", "전투항공모함 (Battlecarrier)", "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull", 3300, 0.47, 0.38, 2.5, (15.0, 82.0, -12.0)),
    ("Battleship", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 3600, 0.88, 0.34, 2.3, (15.0, 82.0, -12.0)),
    ("Carrier", "항공모함 (Carrier)", "/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull", 4500, 0.66, 0.22, 2.2, (15.0, 82.0, -12.0)),
    ("Cruiser1", "순양함 1 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 4000, 0.36, 0.28, 2.0, (15.0, 82.0, -12.0)),
    ("Cruiser2", "순양함 2 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 4400, 0.84, 0.18, 2.0, (15.0, 82.0, -12.0)),
    ("Bastion", "바스티온급 구축함 (Bastion)", "/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull", 5200, 0.50, 0.14, 1.8, (15.0, 82.0, -12.0)),
    ("Vector", "벡터급 구축함 (Vector)", "/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull", 5400, 0.66, 0.12, 1.8, (15.0, 82.0, -12.0)),
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 4500, 0.96, 0.16, 1.8, (15.0, 82.0, -12.0)),
    ("Frigate1", "호위함 1 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 2700, 0.91, 0.60, 1.3, (15.0, 82.0, -12.0)),
    ("Frigate2", "호위함 2 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 3100, 0.60, 0.56, 1.3, (15.0, 82.0, -12.0)),
    ("Fighter1", "함재기 편대 1 (Scout)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2000, 0.40, 0.56, 0.75, (16.0, 84.0, -14.0)),
    ("Fighter2", "함재기 편대 2 (Escort L)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2600, 0.30, 0.40, 0.70, (16.0, 84.0, -14.0)),
    ("Fighter3", "함재기 편대 3 (Escort R)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2400, 0.96, 0.46, 0.70, (16.0, 84.0, -14.0)),
]
for tag, name, mesh, dist, sx, sy, scale, rot in ships200:
    pos = screen_to_world(dist, sx, sy)
    rot_str = f"FRotator({rot[0]:.1f}f, {rot[1]:.1f}f, {rot[2]:.1f}f)"
    print(f'            AddSlot(TEXT("{tag}"), TEXT("{name}"), TEXT("{mesh}"), FVector({pos[0]}f, {pos[1]}f, {pos[2]}f), {rot_str}, {scale:.2f}f);')

print("\n=== CARRIER STRIKE SLOTS ===")
# Fleet300: Camera is at (-3550, -1420, 840), Rot=(-12, 20, 0), Fov=42
# Ships: Battlecarrier, Battleship, AirDefense, PC14
ships300 = [
    ("Flagship", "항공순양함 (Battlecarrier)", "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull", 3000, 0.72, 0.50, 2.5),
    ("FireSupport", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 3800, 0.48, 0.36, 2.2),
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 4400, 0.88, 0.30, 1.9),
    ("Scout", "초계함 (PC14)", "/Game/Presentation/Ships/SM_PC14_FullMesh.SM_PC14_FullMesh", 2400, 0.38, 0.52, 1.2),
]
# For ships300, camera params:
CamLoc300 = (-3550.0, -1420.0, 840.0)
p300 = math.radians(-12.0)
y300 = math.radians(20.0)
Fwd300 = (math.cos(p300)*math.cos(y300), math.cos(p300)*math.sin(y300), math.sin(p300))
Right300 = (-math.sin(y300), math.cos(y300), 0.0)
Up300 = (
    Fwd300[1]*Right300[2] - Fwd300[2]*Right300[1],
    Fwd300[2]*Right300[0] - Fwd300[0]*Right300[2],
    Fwd300[0]*Right300[1] - Fwd300[1]*Right300[0]
)
fov_x300 = math.radians(42.0)
fov_y300 = 2.0 * math.atan(math.tan(fov_x300/2.0) / aspect)
tan_half_x300 = math.tan(fov_x300 / 2.0)
tan_half_y300 = math.tan(fov_y300 / 2.0)

for tag, name, mesh, dist, sx, sy, scale in ships300:
    ndc_x = (sx - 0.5) * 2.0
    ndc_y = (0.5 - sy) * 2.0
    r_dist = ndc_x * dist * tan_half_x300
    u_dist = ndc_y * dist * tan_half_y300
    x = CamLoc300[0] + Fwd300[0]*dist + Right300[0]*r_dist + Up300[0]*u_dist
    y_pos = CamLoc300[1] + Fwd300[1]*dist + Right300[1]*r_dist + Up300[1]*u_dist
    z = CamLoc300[2] + Fwd300[2]*dist + Right300[2]*r_dist + Up300[2]*u_dist
    print(f'            AddSlot(TEXT("{tag}"), TEXT("{name}"), TEXT("{mesh}"), FVector({round(x,1)}f, {round(y_pos,1)}f, {round(z,1)}f), FRotator(15.f, 82.f, -12.f), {scale:.2f}f);')
