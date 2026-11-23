import math

# Camera specs
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

# Proposed slots (shifted up, pushed back, well-spaced)
# Screen is 1920x1080.
# Ships:
# Flagship (Dreadnought): hero ship, foreground-right, but not touching edges!
#   screen_x=0.76, screen_y=0.56, dist=2400, scale=3.2
# Battlecarrier: center-left capital, beautiful carrier deck
#   screen_x=0.50, screen_y=0.40, dist=3300, scale=2.7
# Battleship: right capital ship
#   screen_x=0.86, screen_y=0.42, dist=3300, scale=2.5
# Carrier: mid-top capital
#   screen_x=0.66, screen_y=0.28, dist=4200, scale=2.4
# Cruiser1: mid-left escort
#   screen_x=0.42, screen_y=0.32, dist=3800, scale=2.2
# Cruiser2: mid-right escort
#   screen_x=0.82, screen_y=0.26, dist=4000, scale=2.1
# Bastion: deep space left
#   screen_x=0.54, screen_y=0.20, dist=4800, scale=1.9
# Vector: deep space center
#   screen_x=0.72, screen_y=0.18, dist=4900, scale=1.9
# AirDefense: upper right perimeter
#   screen_x=0.91, screen_y=0.30, dist=3700, scale=1.9
# Frigate1: lower right screen flank (small, lower than flagship)
#   screen_x=0.90, screen_y=0.60, dist=2600, scale=1.4
# Frigate2: lower center screen flank
#   screen_x=0.63, screen_y=0.55, dist=3000, scale=1.4
# Fighters: widely separated patrol positions! ZERO clumping!
# Fighter 1 (Scout lead, flying below and ahead of Battlecarrier):
#   screen_x=0.58, screen_y=0.66, dist=2100, scale=0.75
# Fighter 2 (High escort, above Battlecarrier):
#   screen_x=0.46, screen_y=0.48, dist=2600, scale=0.70
# Fighter 3 (Starboard escort, far right):
#   screen_x=0.94, screen_y=0.48, dist=2400, scale=0.70

ships = [
    ("Flagship", "구축전함 (Dreadnought)", "/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull", 2400, 0.76, 0.56, 3.2),
    ("Battlecarrier", "전투항공모함 (Battlecarrier)", "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull", 3300, 0.50, 0.40, 2.7),
    ("Battleship", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 3300, 0.86, 0.42, 2.5),
    ("Carrier", "항공모함 (Carrier)", "/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull", 4200, 0.66, 0.28, 2.4),
    ("Cruiser1", "순양함 1 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 3800, 0.42, 0.32, 2.2),
    ("Cruiser2", "순양함 2 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 4000, 0.82, 0.26, 2.1),
    ("Bastion", "바스티온급 구축함 (Bastion)", "/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull", 4800, 0.54, 0.20, 1.9),
    ("Vector", "벡터급 구축함 (Vector)", "/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull", 4900, 0.72, 0.18, 1.9),
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 3700, 0.91, 0.30, 1.9),
    ("Frigate1", "호위함 1 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 2600, 0.90, 0.60, 1.4),
    ("Frigate2", "호위함 2 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 3000, 0.63, 0.55, 1.4),
    ("Fighter1", "함재기 편대 1 (Scout)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2100, 0.58, 0.66, 0.75),
    ("Fighter2", "함재기 편대 2 (Escort L)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2600, 0.46, 0.48, 0.70),
    ("Fighter3", "함재기 편대 3 (Escort R)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2400, 0.94, 0.48, 0.70),
]

# Pairwise pixel distance check on 1920x1080
print("=== PAIRWISE DISTANCES (1920x1080) ===")
min_dist = 999999
closest_pair = None
for i in range(len(ships)):
    for j in range(i+1, len(ships)):
        s1 = ships[i]
        s2 = ships[j]
        px1 = (s1[4]*1920, s1[5]*1080)
        px2 = (s2[4]*1920, s2[5]*1080)
        d = math.hypot(px1[0]-px2[0], px1[1]-px2[1])
        if d < min_dist:
            min_dist = d
            closest_pair = (s1[0], s2[0], d)

print(f"Closest pair: {closest_pair[0]} <-> {closest_pair[1]}: {closest_pair[2]:.1f} pixels")

rot_standard = "FRotator(15.f, 82.f, -12.f)"
rot_fighter = "FRotator(16.f, 84.f, -14.f)"

print("\n=== GENERATED CODE SLOTS ===")
for tag, name, mesh, fwd, sx, sy, scale in ships:
    pos = screen_to_world(fwd, sx, sy)
    rot = rot_fighter if "Fighter" in tag else rot_standard
    print(f'            AddSlot(TEXT("{tag}"), TEXT("{name}"), TEXT("{mesh}"), FVector({pos[0]}f, {pos[1]}f, {pos[2]}f), {rot}, {scale}f);')
