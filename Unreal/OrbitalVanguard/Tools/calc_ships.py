import math

# Camera specs in Title
CamLoc = (-3400.0, -1500.0, 760.0)
CamPitch = -10.0
CamYaw = 21.0
CamRoll = 0.0

p = math.radians(CamPitch)
y = math.radians(CamYaw)

# True UE FRotationMatrix axes:
# Pitch = -10 deg, Yaw = 21 deg, Roll = 0 deg
Fwd = (math.cos(p)*math.cos(y), math.cos(p)*math.sin(y), math.sin(p))
Right = (-math.sin(y), math.cos(y), 0.0)
# Up = Forward cross Right in UE left-handed coordinates:
Up = (
    Fwd[1]*Right[2] - Fwd[2]*Right[1],
    Fwd[2]*Right[0] - Fwd[0]*Right[2],
    Fwd[0]*Right[1] - Fwd[1]*Right[0]
)

print(f"// Camera: Loc={CamLoc}, Rot=({CamPitch}, {CamYaw}, {CamRoll})")
print(f"// Fwd=({Fwd[0]:.4f}, {Fwd[1]:.4f}, {Fwd[2]:.4f})")
print(f"// Right=({Right[0]:.4f}, {Right[1]:.4f}, {Right[2]:.4f})")
print(f"// Up=({Up[0]:.4f}, {Up[1]:.4f}, {Up[2]:.4f})")

def calc_pos(fwd_dist, r_dist, u_dist):
    x = CamLoc[0] + Fwd[0]*fwd_dist + Right[0]*r_dist + Up[0]*u_dist
    y_pos = CamLoc[1] + Fwd[1]*fwd_dist + Right[1]*r_dist + Up[1]*u_dist
    z = CamLoc[2] + Fwd[2]*fwd_dist + Right[2]*r_dist + Up[2]*u_dist
    return (round(x, 1), round(y_pos, 1), round(z, 1))

# Echelon Fleet Formation:
# Reference 01: Lead Flagship (Dreadnought) prominent in foreground right.
# Escorts stepped back and to the right in echelon, fighters flanking.
# Rotation:
# Mesh bow is -X. With Yaw = 32 deg, bow points at 32 + 180 = 212 deg (toward camera 201 deg, angled 11 deg left to show port flank).
# Pitch = 6 deg -> bow dips slightly down (-sin(6 deg) = -0.10).
# Roll = -8 deg -> banking turn.
ship_rot = "FRotator(6.f, 32.f, -8.f)"

ships = [
    # Hero Flagship
    ("Flagship", "구축전함 (Dreadnought)", "/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull", 2200, 520, -100, 3.8),
    
    # Capital Escorts (Battleship, Battlecarrier, Carrier echeloning back-right)
    ("Battleship", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 3000, 850, 80, 2.8),
    ("Battlecarrier", "전투항공모함 (Battlecarrier)", "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull", 3800, 1150, 260, 2.6),
    ("Carrier", "항공모함 (Carrier)", "/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull", 4600, 1450, 440, 2.4),
    
    # Cruisers and Destroyers guarding inner flank
    ("Cruiser", "순양함 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 2600, 320, -260, 2.2),
    ("Bastion", "바스티온급 구축함 (Bastion)", "/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull", 3300, 650, -120, 1.8),
    ("Vector", "벡터급 구축함 (Vector)", "/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull", 4100, 920, 60, 1.7),
    
    # Outer Screen (Air Defense and Frigates)
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 2900, 1100, 180, 1.6),
    ("Frigate1", "호위함 1 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 3500, 1350, 360, 1.3),
    ("Frigate2", "호위함 2 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 4300, 1650, 540, 1.3),
    
    # Fighter screen escorting close to the Flagship
    ("Fighter1", "함재기 편대 1", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 1900, 380, 40, 1.2),
    ("Fighter2", "함재기 편대 2", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2050, 440, 80, 1.2),
    ("Fighter3", "함재기 편대 3", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2400, 680, 140, 1.2),
]

for tag, name, mesh, fwd, r, u, scale in ships:
    pos = calc_pos(fwd, r, u)
    print(f'            AddSlot(TEXT("{tag}"), TEXT("{name}"), TEXT("{mesh}"), FVector({pos[0]}f, {pos[1]}f, {pos[2]}f), {ship_rot}, {scale}f);')
