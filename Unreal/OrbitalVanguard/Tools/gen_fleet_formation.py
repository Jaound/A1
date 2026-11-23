import math

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

def calc_pos(fwd_dist, r_dist, u_dist):
    x = CamLoc[0] + Fwd[0]*fwd_dist + Right[0]*r_dist + Up[0]*u_dist
    y_pos = CamLoc[1] + Fwd[1]*fwd_dist + Right[1]*r_dist + Up[1]*u_dist
    z = CamLoc[2] + Fwd[2]*fwd_dist + Right[2]*r_dist + Up[2]*u_dist
    return (round(x, 1), round(y_pos, 1), round(z, 1))

rot_str = "FRotator(12.f, 65.f, -10.f)"
rot_fighter = "FRotator(14.f, 68.f, -14.f)"

fleet = [
    ("Flagship", "구축전함 (Dreadnought)", "/Game/Presentation/Ships/DreadnoughtFull/SM_Hull.SM_Hull", 2300, 580, -180, rot_str, 3.6),
    ("Battlecarrier", "전투항공모함 (Battlecarrier)", "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull.SM_Hull", 3800, 220, 120, rot_str, 2.8),
    ("Battleship", "전함 (Battleship)", "/Game/Presentation/Ships/BattleshipFull/SM_Hull.SM_Hull", 3400, 920, 40, rot_str, 2.5),
    ("Carrier", "항공모함 (Carrier)", "/Game/Presentation/Ships/CarrierFull/SM_Hull.SM_Hull", 4800, 480, 280, rot_str, 2.4),
    ("Cruiser", "순양함 (Cruiser)", "/Game/Presentation/Ships/CruiserFull/SM_Hull.SM_Hull", 3100, 400, -40, rot_str, 1.9),
    ("Bastion", "바스티온급 구축함 (Bastion)", "/Game/Presentation/Ships/BastionFull/SM_Hull.SM_Hull", 4200, 1150, 180, rot_str, 1.8),
    ("Vector", "벡터급 구축함 (Vector)", "/Game/Presentation/Ships/VectorFull/SM_Hull.SM_Hull", 4600, 680, 260, rot_str, 1.7),
    ("AirDefense", "방공함 (Air Defense)", "/Game/Presentation/Ships/Air_DefenseFull/SM_Hull.SM_Hull", 4000, 320, 360, rot_str, 1.6),
    ("Frigate1", "호위함 1 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 3000, 780, -320, rot_str, 1.4),
    ("Frigate2", "호위함 2 (Frigate)", "/Game/Presentation/Ships/FrigateFull/SM_Hull.SM_Hull", 5000, 850, 320, rot_str, 1.3),
    ("Fighter1", "함재기 편대 1 (Lead)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 1900, 460, -100, rot_fighter, 1.0),
    ("Fighter2", "함재기 편대 2 (Wingman L)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2050, 520, -70, rot_fighter, 0.9),
    ("Fighter3", "함재기 편대 3 (Wingman R)", "/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull", 2050, 400, -130, rot_fighter, 0.9),
]

for tag, name, mesh, fwd, r, u, rot, scale in fleet:
    pos = calc_pos(fwd, r, u)
    print(f'            AddSlot(TEXT("{tag}"), TEXT("{name}"), TEXT("{mesh}"), FVector({pos[0]}f, {pos[1]}f, {pos[2]}f), {rot}, {scale}f);')
