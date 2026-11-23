import math

# Camera: "살짝 더 뒤로 가서 위로 살짝 올려야할거 같아"
# Original: cam_loc = [-3900.0, -1700.0, 960.0], rot = [-12.5, 22.0, 0.0]
# New camera: Moved back and up, looking down with -14.5 pitch for expansive orbital perspective
cam_loc = [-4600.0, -1950.0, 1350.0]
cam_rot = [-14.0, 22.0, 0.0]
fov = 46.0

cp = math.radians(cam_rot[0])
cy = math.radians(cam_rot[1])
cf = [math.cos(cp)*math.cos(cy), math.cos(cp)*math.sin(cy), math.sin(cp)]
cr = [-math.sin(cy), math.cos(cy), 0]
cup = [cf[1]*cr[2] - cf[2]*cr[1], cf[2]*cr[0] - cf[0]*cr[2], cf[0]*cr[1] - cf[1]*cr[0]]

aspect = 16.0 / 9.0
half_fov_x = math.radians(fov * 0.5)
tan_half_fov_x = math.tan(half_fov_x)
tan_half_fov_y = tan_half_fov_x / aspect

def unproject(u, v, dist):
    y_cam = (u - 0.5) * 2.0 * dist * tan_half_fov_x
    z_cam = (0.5 - v) * 2.0 * dist * tan_half_fov_y
    x_cam = dist
    pos = [cam_loc[i] + x_cam*cf[i] + y_cam*cr[i] + z_cam*cup[i] for i in range(3)]
    return pos

# Ship orientation: "방향을 살짝 돌려야해 위 레퍼런스를 바탕으로 수정해줘"
# In reference: ships fly forward-right with level/slight downward orbital pitch, broadside-three-quarter view
# Pitch = -3.0f (level orbital plane with subtle down dip), Yaw = 58.0f (sleek 3/4 quartering angle), Roll = -4.0f
ship_rot = (-3.0, 58.0, -4.0)

# Fleet 200 / Title Screen Formation
f2_slots = [
    # Hero Flagship: Foreground right, commanding presence over orbital horizon
    ('Flagship', '구축전함 (Dreadnought)', 'DreadnoughtFull', 0.65, 0.54, 3300, 2.60),
    
    # Escort Frigates: Wide perimeter picket positions, completely clear of Flagship
    ('Frigate1', '호위함 1 (Frigate)', 'FrigateFull', 0.89, 0.54, 3500, 1.30),
    ('Frigate2', '호위함 2 (Frigate)', 'FrigateFull', 0.38, 0.62, 3400, 1.30),
    
    # Capital Carrier: Massive flat-top battlecarrier dominating the left-center opening
    ('Battlecarrier', '전투항공모함 (Battlecarrier)', 'BattlecarrierFull', 0.38, 0.36, 4200, 2.50),
    
    # Midground Cruisers & Battleships: Staggered across empty celestial zones
    ('Cruiser1', '순양함 1 (Cruiser)', 'CruiserFull', 0.78, 0.36, 4500, 2.00),
    ('Bastion', '바스티온급 구축함 (Bastion)', 'BastionFull', 0.55, 0.28, 4800, 1.90),
    ('Carrier', '항공모함 (Carrier)', 'CarrierFull', 0.42, 0.18, 5500, 2.10),
    ('Battleship', '전함 (Battleship)', 'BattleshipFull', 0.70, 0.18, 5400, 2.20),
    
    # Deep-space Vanguard Line:
    ('Vector', '벡터급 구축함 (Vector)', 'VectorFull', 0.56, 0.10, 6200, 1.80),
    ('AirDefense', '방공함 (Air Defense)', 'Air_DefenseFull', 0.86, 0.12, 6000, 1.80),
    
    # Scout Fighters: Patrolling the open sky high above the Title Menu
    ('Fighter1', '함재기 편대 1 (Scout Lead)', 'FighterFull', 0.22, 0.08, 4200, 0.75),
    ('Fighter2', '함재기 편대 2 (Scout Wing L)', 'FighterFull', 0.13, 0.06, 4400, 0.70),
    ('Fighter3', '함재기 편대 3 (Scout Wing R)', 'FighterFull', 0.31, 0.07, 4300, 0.70),
]

print("=== Screen Distance Check (min pixel separation at 1280x720) ===")
min_dist = 999999
for i in range(len(f2_slots)):
    for j in range(i+1, len(f2_slots)):
        u1, v1 = f2_slots[i][3], f2_slots[i][4]
        u2, v2 = f2_slots[j][3], f2_slots[j][4]
        dx = (u1 - u2) * 1280
        dy = (v1 - v2) * 720
        d = math.hypot(dx, dy)
        if d < min_dist:
            min_dist = d
            min_pair = (f2_slots[i][0], f2_slots[j][0])

print(f"Minimum screen distance: {min_dist:.1f} pixels between {min_pair[0]} and {min_pair[1]}")

print("\n=== Formation 2 Code ===")
for role, label, mesh, u, v, dist, scale in f2_slots:
    p = unproject(u, v, dist)
    mesh_str = f"/Game/Presentation/Ships/{mesh}/SM_Hull.SM_Hull"
    print(f'            AddSlot(TEXT("{role}"), TEXT("{label}"), TEXT("{mesh_str}"), FVector({p[0]:.1f}f, {p[1]:.1f}f, {p[2]:.1f}f), FRotator({ship_rot[0]:.1f}f, {ship_rot[1]:.1f}f, {ship_rot[2]:.1f}f), {scale:.2f}f);')

# Formation 1: Fleet100
f1_slots = [
    ('LeadBattleship', '전함 (Battleship)', 'BattleshipFull', 0.65, 0.52, 3400, 2.60),
    ('Cruiser', '순양함 (Cruiser)', 'CruiserFull', 0.38, 0.36, 4200, 2.30),
    ('AirDefense', '방공함 (Air Defense)', 'Air_DefenseFull', 0.82, 0.28, 4600, 2.10),
    ('Vector', '벡터급 구축함 (Vector)', 'VectorFull', 0.58, 0.14, 5400, 2.00),
]
print("\n=== Formation 1 Code ===")
for role, label, mesh, u, v, dist, scale in f1_slots:
    p = unproject(u, v, dist)
    mesh_str = f"/Game/Presentation/Ships/{mesh}/SM_Hull.SM_Hull"
    print(f'            AddSlot(TEXT("{role}"), TEXT("{label}"), TEXT("{mesh_str}"), FVector({p[0]:.1f}f, {p[1]:.1f}f, {p[2]:.1f}f), FRotator({ship_rot[0]:.1f}f, {ship_rot[1]:.1f}f, {ship_rot[2]:.1f}f), {scale:.2f}f);')

# Formation 3: CarrierStrike (Fleet300)
f3_slots = [
    ('Flagship', '항공순양함 (Battlecarrier)', 'BattlecarrierFull', 0.65, 0.52, 3400, 2.60),
    ('FireSupport', '전함 (Battleship)', 'BattleshipFull', 0.38, 0.36, 4200, 2.30),
    ('AirDefense', '방공함 (Air Defense)', 'Air_DefenseFull', 0.82, 0.28, 4600, 2.00),
    ('Scout', '초계함 (PC14)', 'SM_PC14_FullMesh', 0.22, 0.08, 4200, 1.20),
]
print("\n=== Formation 3 Code ===")
for role, label, mesh, u, v, dist, scale in f3_slots:
    p = unproject(u, v, dist)
    mesh_str = f"/Game/Presentation/Ships/{mesh}/SM_Hull.SM_Hull" if "PC14" not in mesh else f"/Game/Presentation/Ships/{mesh}.{mesh}"
    print(f'            AddSlot(TEXT("{role}"), TEXT("{label}"), TEXT("{mesh_str}"), FVector({p[0]:.1f}f, {p[1]:.1f}f, {p[2]:.1f}f), FRotator({ship_rot[0]:.1f}f, {ship_rot[1]:.1f}f, {ship_rot[2]:.1f}f), {scale:.2f}f);')
