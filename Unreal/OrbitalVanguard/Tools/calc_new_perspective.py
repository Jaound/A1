import math

# Camera
CamLoc = (-3400.0, -1500.0, 760.0)
CamPitch = -10.0
CamYaw = 21.0
CamRoll = 0.0

p = math.radians(CamPitch)
y = math.radians(CamYaw)

Fwd = (math.cos(p)*math.cos(y), math.cos(p)*math.sin(y), math.sin(p))
Right = (-math.sin(y), math.cos(y), 0.0)
Up = (
    Fwd[1]*Right[2] - Fwd[2]*Right[1],
    Fwd[2]*Right[0] - Fwd[0]*Right[2],
    Fwd[0]*Right[1] - Fwd[1]*Right[0]
)

CameraFov = 48.0
fov_x = math.radians(CameraFov)
aspect = 16.0 / 9.0
fov_y = 2.0 * math.atan(math.tan(fov_x/2.0) / aspect)

tan_half_x = math.tan(fov_x / 2.0)
tan_half_y = math.tan(fov_y / 2.0)

def screen_to_cam_pos(fwd_dist, screen_x, screen_y):
    ndc_x = (screen_x - 0.5) * 2.0
    ndc_y = (0.5 - screen_y) * 2.0
    r_dist = ndc_x * fwd_dist * tan_half_x
    u_dist = ndc_y * fwd_dist * tan_half_y
    x = CamLoc[0] + Fwd[0]*fwd_dist + Right[0]*r_dist + Up[0]*u_dist
    y_pos = CamLoc[1] + Fwd[1]*fwd_dist + Right[1]*r_dist + Up[1]*u_dist
    z = CamLoc[2] + Fwd[2]*fwd_dist + Right[2]*r_dist + Up[2]*u_dist
    return (round(x, 1), round(y_pos, 1), round(z, 1)), r_dist, u_dist

# Targets:
# Hero Flagship: screen (0.74, 0.70), fwd_dist=1750, scale=4.2
# Battlecarrier: screen (0.53, 0.50), fwd_dist=2700, scale=3.2
# Battleship:    screen (0.88, 0.54), fwd_dist=2650, scale=3.0
# Carrier:       screen (0.66, 0.35), fwd_dist=3800, scale=2.8
# Cruiser1:      screen (0.44, 0.39), fwd_dist=3300, scale=2.4
# Cruiser2:      screen (0.82, 0.33), fwd_dist=3700, scale=2.4
# Bastion:       screen (0.55, 0.27), fwd_dist=4400, scale=2.0
# Vector:        screen (0.72, 0.25), fwd_dist=4600, scale=2.0
# AirDefense:    screen (0.91, 0.39), fwd_dist=3500, scale=1.9
# Frigate1:      screen (0.89, 0.76), fwd_dist=2500, scale=1.6
# Frigate2:      screen (0.64, 0.72), fwd_dist=2900, scale=1.5
# Fighter1:      screen (0.61, 0.61), fwd_dist=1550, scale=0.85
# Fighter2:      screen (0.63, 0.58), fwd_dist=1620, scale=0.80
# Fighter3:      screen (0.62, 0.64), fwd_dist=1620, scale=0.80

ships_spec = [
    ("Flagship", "구축전함 (Dreadnought)", "/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull", 1750, 0.74, 0.70, 4.2),
    ("Battlecarrier", "전투항공모함 (Battlecarrier)", "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull", 2700, 0.53, 0.50, 3.2),
    ("Battleship", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 2650, 0.88, 0.54, 3.0),
    ("Carrier", "항공모함 (Carrier)", "/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull", 3800, 0.66, 0.35, 2.8),
    ("Cruiser1", "순양함 1 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 3300, 0.44, 0.39, 2.4),
    ("Cruiser2", "순양함 2 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 3700, 0.82, 0.33, 2.4),
    ("Bastion", "바스티온급 구축함 (Bastion)", "/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull", 4400, 0.55, 0.27, 2.0),
    ("Vector", "벡터급 구축함 (Vector)", "/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull", 4600, 0.72, 0.25, 2.0),
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 3500, 0.91, 0.39, 1.9),
    ("Frigate1", "호위함 1 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 2500, 0.89, 0.76, 1.6),
    ("Frigate2", "호위함 2 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 2900, 0.64, 0.72, 1.5),
    ("Fighter1", "함재기 편대 1 (Lead)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 1550, 0.61, 0.61, 0.85),
    ("Fighter2", "함재기 편대 2 (Wingman L)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 1620, 0.63, 0.58, 0.80),
    ("Fighter3", "함재기 편대 3 (Wingman R)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 1620, 0.62, 0.64, 0.80),
]

rot_standard = "FRotator(12.f, 65.f, -10.f)"
rot_fighter = "FRotator(14.f, 68.f, -14.f)"

print("=== CALCULATED SLOTS ===")
for tag, name, mesh, fwd, sx, sy, scale in ships_spec:
    pos, r_dist, u_dist = screen_to_cam_pos(fwd, sx, sy)
    rot = rot_fighter if "Fighter" in tag else rot_standard
    apparent = (scale / fwd) * 1000.0
    print(f'            AddSlot(TEXT("{tag}"), TEXT("{name}"), TEXT("{mesh}"), FVector({pos[0]}f, {pos[1]}f, {pos[2]}f), {rot}, {scale}f); // Apparent={apparent:.2f}')
