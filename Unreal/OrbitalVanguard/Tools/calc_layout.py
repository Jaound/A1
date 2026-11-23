import math

cam_loc = [-3900.0, -1700.0, 960.0]
cam_rot = [-12.5, 22.0, 0.0]
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

# Formation 2: FlagshipVanguard (Default Title & Fleet200)
f2_slots = [
    ('Flagship', '구축전함 (Dreadnought)', 'DreadnoughtFull', 0.68, 0.52, 3200, 2.70),
    ('Battlecarrier', '전투항공모함 (Battlecarrier)', 'BattlecarrierFull', 0.44, 0.38, 3800, 2.50),
    ('Battleship', '전함 (Battleship)', 'BattleshipFull', 0.86, 0.38, 4100, 2.30),
    ('Carrier', '항공모함 (Carrier)', 'CarrierFull', 0.62, 0.24, 4800, 2.20),
    ('Cruiser1', '순양함 1 (Cruiser)', 'CruiserFull', 0.32, 0.28, 4400, 2.00),
    ('Cruiser2', '순양함 2 (Cruiser)', 'CruiserFull', 0.82, 0.20, 5000, 2.00),
    ('Bastion', '바스티온급 구축함 (Bastion)', 'BastionFull', 0.48, 0.14, 5600, 1.80),
    ('Vector', '벡터급 구축함 (Vector)', 'VectorFull', 0.66, 0.12, 5900, 1.80),
    ('AirDefense', '방공함 (Air Defense)', 'Air_DefenseFull', 0.92, 0.15, 5200, 1.80),
    ('Frigate1', '호위함 1 (Frigate)', 'FrigateFull', 0.88, 0.56, 3300, 1.30),
    ('Frigate2', '호위함 2 (Frigate)', 'FrigateFull', 0.56, 0.56, 3600, 1.30),
    ('Fighter1', '함재기 편대 1 (Scout Lead)', 'FighterFull', 0.24, 0.48, 2800, 0.70),
    ('Fighter2', '함재기 편대 2 (Scout Wing L)', 'FighterFull', 0.15, 0.38, 3100, 0.65),
    ('Fighter3', '함재기 편대 3 (Scout Wing R)', 'FighterFull', 0.16, 0.56, 2900, 0.65),
]

print("=== Formation 2 Code ===")
for role, label, mesh, u, v, dist, scale in f2_slots:
    p = unproject(u, v, dist)
    mesh_str = f"/Game/Presentation/Ships/{mesh}/SM_Hull.SM_Hull" if "PC14" not in mesh else f"/Game/Presentation/Ships/{mesh}.{mesh}"
    print(f'            AddSlot(TEXT("{role}"), TEXT("{label}"), TEXT("{mesh_str}"), FVector({p[0]:.1f}f, {p[1]:.1f}f, {p[2]:.1f}f), FRotator(12.0f, 76.0f, -8.0f), {scale:.2f}f);')

# Formation 1: Fleet100
f1_slots = [
    ('LeadBattleship', '전함 (Battleship)', 'BattleshipFull', 0.70, 0.50, 3200, 2.60),
    ('Cruiser', '순양함 (Cruiser)', 'CruiserFull', 0.46, 0.36, 3800, 2.30),
    ('AirDefense', '방공함 (Air Defense)', 'Air_DefenseFull', 0.84, 0.34, 4200, 2.10),
    ('Vector', '벡터급 구축함 (Vector)', 'VectorFull', 0.62, 0.22, 4800, 2.00),
]
print("\n=== Formation 1 Code ===")
for role, label, mesh, u, v, dist, scale in f1_slots:
    p = unproject(u, v, dist)
    mesh_str = f"/Game/Presentation/Ships/{mesh}/SM_Hull.SM_Hull"
    print(f'            AddSlot(TEXT("{role}"), TEXT("{label}"), TEXT("{mesh_str}"), FVector({p[0]:.1f}f, {p[1]:.1f}f, {p[2]:.1f}f), FRotator(12.0f, 76.0f, -8.0f), {scale:.2f}f);')

# Formation 3: CarrierStrike (Fleet300)
f3_slots = [
    ('Flagship', '항공순양함 (Battlecarrier)', 'BattlecarrierFull', 0.68, 0.48, 3200, 2.60),
    ('FireSupport', '전함 (Battleship)', 'BattleshipFull', 0.44, 0.36, 3800, 2.30),
    ('AirDefense', '방공함 (Air Defense)', 'Air_DefenseFull', 0.85, 0.32, 4200, 2.00),
    ('Scout', '초계함 (PC14)', 'SM_PC14_FullMesh', 0.24, 0.48, 2800, 1.20),
]
print("\n=== Formation 3 Code ===")
for role, label, mesh, u, v, dist, scale in f3_slots:
    p = unproject(u, v, dist)
    mesh_str = f"/Game/Presentation/Ships/{mesh}/SM_Hull.SM_Hull" if "PC14" not in mesh else f"/Game/Presentation/Ships/{mesh}.{mesh}"
    print(f'            AddSlot(TEXT("{role}"), TEXT("{label}"), TEXT("{mesh_str}"), FVector({p[0]:.1f}f, {p[1]:.1f}f, {p[2]:.1f}f), FRotator(12.0f, 76.0f, -8.0f), {scale:.2f}f);')
