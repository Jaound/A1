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

def world_to_screen(world_pt):
    rel = (world_pt[0] - CamLoc[0], world_pt[1] - CamLoc[1], world_pt[2] - CamLoc[2])
    fwd_dist = rel[0]*CamFwd[0] + rel[1]*CamFwd[1] + rel[2]*CamFwd[2]
    if fwd_dist <= 0:
        return None
    r_dist = rel[0]*CamRight[0] + rel[1]*CamRight[1] + rel[2]*CamRight[2]
    u_dist = rel[0]*CamUp[0] + rel[1]*CamUp[1] + rel[2]*CamUp[2]
    ndc_x = r_dist / (fwd_dist * tan_half_x)
    ndc_y = u_dist / (fwd_dist * tan_half_y)
    screen_x = (ndc_x * 0.5) + 0.5
    screen_y = 0.5 - (ndc_y * 0.5)
    return screen_x * 1920.0, screen_y * 1080.0, fwd_dist

def get_ship_endpoints(center, pitch, yaw, roll, length, scale):
    rad_p = math.radians(pitch)
    rad_y = math.radians(yaw)
    cp, sp = math.cos(rad_p), math.sin(rad_p)
    cy, sy = math.cos(rad_y), math.sin(rad_y)
    bow_dir = (-cp*cy, -cp*sy, -sp)
    half_l = (length * scale) * 0.5
    bow_pt = (center[0] + bow_dir[0]*half_l, center[1] + bow_dir[1]*half_l, center[2] + bow_dir[2]*half_l)
    stern_pt = (center[0] - bow_dir[0]*half_l, center[1] - bow_dir[1]*half_l, center[2] - bow_dir[2]*half_l)
    return bow_pt, stern_pt

def dist_point_to_segment(p, a, b):
    px, py = p[0], p[1]
    ax, ay = a[0], a[1]
    bx, by = b[0], b[1]
    abx, aby = bx - ax, by - ay
    l2 = abx*abx + aby*aby
    if l2 == 0:
        return math.hypot(px-ax, py-ay)
    t = max(0.0, min(1.0, ((px-ax)*abx + (py-ay)*aby) / l2))
    projx = ax + t*abx
    projy = ay + t*aby
    return math.hypot(px-projx, py-projy)

def dist_segment_to_segment(a1, a2, b1, b2):
    return min(
        dist_point_to_segment(a1, b1, b2),
        dist_point_to_segment(a2, b1, b2),
        dist_point_to_segment(b1, a1, a2),
        dist_point_to_segment(b2, a1, a2),
    )

# Optimized configuration:
# Flagship: dist=2600, scale=2.8, screen=(0.74, 0.56)
# Battleship: dist=3500, scale=2.3, screen=(0.88, 0.38)
# Battlecarrier: dist=3300, scale=2.5, screen=(0.47, 0.38)
# Carrier: dist=4500, scale=2.2, screen=(0.68, 0.22)
# Cruiser1: dist=4000, scale=2.0, screen=(0.36, 0.28)
# Cruiser2: dist=4200, scale=2.0, screen=(0.84, 0.22)
# Bastion: dist=5200, scale=1.8, screen=(0.52, 0.14)
# Vector: dist=5400, scale=1.8, screen=(0.70, 0.12)
# AirDefense: dist=3900, scale=1.9, screen=(0.93, 0.28)
# Frigate1: dist=2700, scale=1.3, screen=(0.91, 0.60)
# Frigate2: dist=3100, scale=1.3, screen=(0.60, 0.56)
# Fighter1: dist=2000, scale=0.75, screen=(0.40, 0.56) (patrol forward-left)
# Fighter2: dist=2600, scale=0.70, screen=(0.30, 0.40) (high port flank)
# Fighter3: dist=2400, scale=0.70, screen=(0.96, 0.46) (starboard perimeter)

ships = [
    ("Flagship", "구축전함 (Dreadnought)", "/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull", 350.0, 2.8, 2600, 0.74, 0.56, (15.0, 82.0, -12.0)),
    ("Battlecarrier", "전투항공모함 (Battlecarrier)", "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull", 233.0, 2.5, 3300, 0.47, 0.38, (15.0, 82.0, -12.0)),
    ("Battleship", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 350.0, 2.3, 3600, 0.88, 0.34, (15.0, 82.0, -12.0)),
    ("Carrier", "항공모함 (Carrier)", "/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull", 272.0, 2.2, 4500, 0.66, 0.22, (15.0, 82.0, -12.0)),
    ("Cruiser1", "순양함 1 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 272.0, 2.0, 4000, 0.36, 0.28, (15.0, 82.0, -12.0)),
    ("Cruiser2", "순양함 2 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 272.0, 2.0, 4400, 0.84, 0.18, (15.0, 82.0, -12.0)),
    ("Bastion", "바스티온급 구축함 (Bastion)", "/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull", 350.0, 1.8, 5200, 0.50, 0.14, (15.0, 82.0, -12.0)),
    ("Vector", "벡터급 구축함 (Vector)", "/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull", 324.0, 1.8, 5400, 0.66, 0.12, (15.0, 82.0, -12.0)),
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 285.0, 1.8, 4500, 0.96, 0.16, (15.0, 82.0, -12.0)),
    ("Frigate1", "호위함 1 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 324.0, 1.3, 2700, 0.91, 0.60, (15.0, 82.0, -12.0)),
    ("Frigate2", "호위함 2 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 324.0, 1.3, 3100, 0.60, 0.56, (15.0, 82.0, -12.0)),
    ("Fighter1", "함재기 편대 1 (Scout)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 80.0, 0.75, 2000, 0.40, 0.56, (16.0, 84.0, -14.0)),
    ("Fighter2", "함재기 편대 2 (Escort L)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 80.0, 0.70, 2600, 0.30, 0.40, (16.0, 84.0, -14.0)),
    ("Fighter3", "함재기 편대 3 (Escort R)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 80.0, 0.70, 2400, 0.96, 0.46, (16.0, 84.0, -14.0)),
]

segs = {}
for name, label, mesh, l, s, dist, sx, sy, rot in ships:
    c = screen_to_world(dist, sx, sy)
    bow, stern = get_ship_endpoints(c, rot[0], rot[1], rot[2], l, s)
    s_c = world_to_screen(c)
    s_bow = world_to_screen(bow)
    s_stern = world_to_screen(stern)
    segs[name] = (s_c, s_bow, s_stern, c, rot, s)
    print(f"{name:14s}: Center=({s_c[0]:6.1f}, {s_c[1]:6.1f}), Bow=({s_bow[0]:6.1f}, {s_bow[1]:6.1f}), Stern=({s_stern[0]:6.1f}, {s_stern[1]:6.1f})")

min_d = 999999
closest = None
for i in range(len(ships)):
    for j in range(i+1, len(ships)):
        n1 = ships[i][0]
        n2 = ships[j][0]
        _, b1, s1, _, _, _ = segs[n1]
        _, b2, s2, _, _, _ = segs[n2]
        d = dist_segment_to_segment(b1, s1, b2, s2)
        if d < min_d:
            min_d = d
            closest = (n1, n2, d)
        if d < 70.0:
            print(f"CLOSE: {n1} <-> {n2}: {d:.1f} px")

print(f"\nOVERALL MINIMUM CLEARANCE: {closest[0]} <-> {closest[1]} = {closest[2]:.1f} pixels")

print("\n=== GENERATED C++ CODE SLOTS ===")
for name, label, mesh, l, s, dist, sx, sy, rot in ships:
    _, _, _, c, r, scale = segs[name]
    rot_str = f"FRotator({r[0]:.1f}f, {r[1]:.1f}f, {r[2]:.1f}f)"
    print(f'            AddSlot(TEXT("{name}"), TEXT("{label}"), TEXT("{mesh}"), FVector({c[0]}f, {c[1]}f, {c[2]}f), {rot_str}, {scale:.2f}f);')
