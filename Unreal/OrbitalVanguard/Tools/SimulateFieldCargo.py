"""D126 시험장 적재 후보. 저장소의 표준 패키지와 현장 수리 비용으로 재생성한다."""
from pathlib import Path
import math
import random
import re
import statistics
import subprocess

root = Path(__file__).resolve().parents[3]
revision = "6d32a1f9dd96349563e30a51eb4d457a7397a836"
table = subprocess.check_output(["git", "show", revision + ":docs/02_balance/BASE_BALANCE_DRAFT.md"], cwd=root).decode("utf8")
names = {"corvette":"초계함","frigate":"호위함","vector":"백터급 구축함","bastion":"바스티온급 구축함","cruiser":"순양함","air_defense":"방공함","radar":"레이더함","battleship":"전함","carrier":"항공모함","missile_ship":"전략 미사일함","dreadnought":"구축전함","battlecarrier":"항공순양함","repair":"보급수리함","mining":"채굴선","transport":"수송선","assault":"강습선"}
quotes = {}
for key, name in names.items():
    match = re.search(r"\| " + name + r" \| (\d+)/(\d+)/(\d+)", table)
    if match:
        quotes[key] = tuple(map(int, match.groups()))
# 견적 미정인 수송·강습형에만 동급 운용 후보 견적을 사용한다.
for key, peer in [("transport", "vector"), ("assault", "frigate"), ("mining", "repair")]:
    quotes.setdefault(key, quotes[peer])
rng = random.Random(126)
samples = []
for _ in range(4096):
    # 중형 2척, 대형 1척, 소형 2척의 선체가 각각 10~45% 손상된 보급 주기.
    demand = [0., 0., 0.]
    for target in ("vector","bastion","battleship","frigate","corvette"):
        fraction = rng.uniform(.10, .45) * .30 * 1.5
        for i, amount in enumerate(quotes[target]):
            demand[i] += fraction * amount
    samples.append(demand)
stock = [math.ceil(statistics.quantiles([s[i] for s in samples], n=4)[2]) for i in range(3)]
print("4096회 / 상위 25% 경계", stock, "가중 용량", sum(a*b for a,b in zip(stock,[1,3,8])))
out = ["// SimulateFieldCargo.py / D126 시험 후보", "static const FVector RepairStock(%d, %d, %d);" % tuple(stock), "static FVector PackageCost(FName Hull)", "{"]
# 채굴선 1척은 초당 5, 수송선은 같은 채굴선 2척의 인계를 받는다.
# 시험 교대 간격은 삼각분포(초)다. 중형 선창의 체급 상승 여유는 체급 부피비 5의 세제곱근으로 제한한다.
mine_batches = [5*rng.triangular(9,19,14) for _ in range(4096)]
shuttle_batches = [10*rng.triangular(8,16,11) for _ in range(4096)]
def capacity(batches):
    return 5*math.ceil(statistics.quantiles(batches,n=4)[2]/5)
miner = capacity(mine_batches)
shuttle = min(capacity(shuttle_batches), 5*math.floor(miner*5**(1/3)/5))
out[2:2] = [f"static constexpr int32 MiningStock={miner}, TransportStock={shuttle};"]
print("채굴·수송 적재 후보",miner,shuttle)
for key, cost in quotes.items():
    out.append('    if (Hull == TEXT("%s")) return FVector(%d,%d,%d);' % (key, *cost))
out += ["    return FVector::ZeroVector;", "}"]
(root/"Unreal/OrbitalVanguard/Source/OrbitalVanguard/OVFieldCargo.inl").write_text(chr(10).join(out)+chr(10), encoding="utf8")
