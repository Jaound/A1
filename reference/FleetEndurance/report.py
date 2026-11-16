import json,statistics,math,hashlib
from pathlib import Path
from collections import Counter
R=Path(__file__).parent
p=json.loads((R/'profile.json').read_text());r=json.loads((R/'results.json').read_text());controls=json.loads((R/'paired_controls.json').read_text());sens=json.loads((R/'sensitivity.json').read_text());sm=r['summary']
for artifact in (r,controls,sens):
 assert artifact['profile_sha256']==hashlib.sha256((R/'profile.json').read_bytes()).hexdigest()
 assert artifact['model_sha256']==hashlib.sha256((R/'model.py').read_bytes()).hexdigest()
def key(b,policy='none',right='mixed'):return f'{b}_mixed_{right}_{policy}_new_screen'
def ci(k,n):
 z=1.96;q=k/n;den=1+z*z/n;c=(q+z*z/(2*n))/den;h=z*math.sqrt(q*(1-q)/n+z*z/(4*n*n))/den
 return f'{100*(c-h):.1f}~{100*(c+h):.1f}%'
lines=['# 장기 함대전·전 함종 코스트 시험 결과 v0.3','', '> 사용자 목표: 1배속 일반 함대전10~15분. 주 평가912회(38조건×24회), 동시드 대조72회, 시간 간격 비교18회. 핵심1002회·추가 반례48회·선내 방어224회, 경로 스트레스36회를 더해 총1310개 유효 결과이며 탐색/이전 후보를 합산하지 않았다. **통계 집계 모형 결과**이며 UE/Godot 실행 결과가 아니다. [수치](../02_balance/FLEET_ENDURANCE_BALANCE.md) · [재현](../../reference/FleetEndurance/README.md) · [원시 데이터](../../reference/FleetEndurance/results.json).','', '## 일반 혼성 함대전 시간','', '| 코스트 | 구 HP/쉴드 대조 | 새 수치 완파 종료 중앙값 | 최초 함선 완파 중앙값 | 잔여 코스트30% 시각 |','| --- | --- | --- | --- | --- |']
for b in (100,200,300):
 s=sm[key(b)];o=sm[f'{b}_mixed_mixed_none_old_screen'];lines.append(f"| {b} | {o['median_seconds']/60:.2f}분 | {s['median_seconds']/60:.2f}분 | {s['median_first_loss']:.1f}초 | {s['median_main_end']/60:.2f}분 |")
lines+=['', '새 일반 혼성전의 중앙값은 세 코스트 모두 목표10~15분 안에 있다. 개별 시행/모든 편성에10~15분을 보장하지 않는다. 완파 종료 시간을 사용했으며 잔여30%를 종료로 바꿔 목표를 맞추지 않았다. 구 대조군은 같은 새 집계 모형에 기존13함종의v0.2 HP/쉴드만 넣었다. 관통·표적AI·함재기 등의 조건은 동일하므로 과거 물리 모형이나 실제 프로토타입의 재현 결과가 아니다. 신규 임무3종에는 구 수치가 없어 새 값을 유지했다.','', '## 정확한 편성과 커버리지','', '100: 소형20·중형6·대형2. 200: 소형20·중형16·대형4. 300: 소형25·중형10·대형4·초대형1. 함재기는 항모 패키지40대/항공순양함80대다. 모든 편성은 한도와 정확히 같고100에는 초대형이 없다.','', '| 코스트 | 기본 혼성 함종:척수 |','| --- | --- |']
for b in (100,200,300):
 rs=r['rosters'][str(b)]['mixed'];lines.append(f"| {b} | "+', '.join(p['ships'][k]['name']+f' {n}' for k,n in rs.items())+' |')
lines+=['', '추가로 순수 전함/소형/미사일함/항모, 초대형 집중, 방공함 대체, 채굴·수송 호위 편성을 비교했다. 16종 전부가 전체 시험에 등장한다. 채굴/수송/강습선의 체급·스탯은 임시 제안이며 채굴/경제 기능을 전투 모델이 실행하지 않는다.','', '## 혼성 대 집중 편성','', '| 코스트 | 상대 | 혼성 승리/24 | 승률95% Wilson 구간 | 중앙 종료 분 | 시간초과 |','| --- | --- | --- | --- | --- | --- |']
for b in (100,200,300):
 for kind,label in [('heavy','전함 집중'),('light','초계/호위 물량'),('missile','전략 미사일함 집중'),('carrier','항모 집중'),('xl','초대형 집중')]:
  if kind=='xl' and b==100:continue
  v=sm[key(b,right=kind)];lines.append(f"| {b} | {label} | {v['wins']}/24 | {ci(v['wins'],24)} | {v['median_seconds']/60:.2f} | {v['timeouts']} |")
lines+=['', '초대형 집중200은 항공순양함1+전함3, 300은 구축전함2+전함2다. 이 두 편성에 혼성이 강했지만 공간을 근사한 AI/추적계수에 의존한다. 실제 측면/상하 기동·플레이어 집중 공격으로 일반화하지 않는다. 소형 물량의 우위가 줄었는지 이전 후보와 비교해야 하며,300코스트의 시간 제한 여부도 위 표에 별도로 기록했다. **혼성의 범용 우위/전체 밸런스는 미완**이다. 소형 물량을 억지 코스트 초과로 배제하지 않았다.','', '## 주력의 화력 비중','', '| 코스트 | 소형 | 중형 | 대형 | 초대형 |','| --- | --- | --- | --- | --- |']
for b in (100,200,300):
 rows=[x for x in r['trials'] if x['budget']==b and x['left']==x['right']=='mixed' and x['policy']=='none' and not x['legacy']];damage={i:sum(x['damage_by_size'].get(f'0:{i}',0) for x in rows) for i in (1,2,3,4)};total=sum(damage.values());lines.append(f"| {b} | "+' | '.join(f'{damage[i]/total*100:.1f}%' for i in (1,2,3,4))+' |')
lines+=['', '100/200에서는 대형이 주 화력이고300에서는 초대형1척이67.5%를 담당했다. 후속 사용자 확정D113은 이 강한 화력을 의도된 결전병기 성능으로 유지한다. 이를 이유로 전용 주포 피해/주기/추적을 하향하는 이전 추천은 철회한다.67.5%는 특정 편성의 관측값이며 고정 점유율이 아니다.','', '## 강습 생존·작전 성과','', '사격 보류 없이 실험했다. 출격은90초 이후, 목표는 살아 있는 중형 이상이며 쉴드 비활성일 때 선택한다. 함대 전체PD의 동시 요격 대신 국소 그룹과 실제 노출 구간을 근사했다.','', '| 코스트 | 수단/목표 | 평균 진입 영웅/4 | 점령 성공/24 | 전투 종료 귀환 생존/4 | 평균 영웅 사망 | 출격→목표 달성 평균 초 |','| --- | --- | --- | --- | --- | --- | --- |']
for b in (100,200,300):
 for policy,label in [('weapon','강습선→무기실'),('pod','포드 직접→무기실'),('capture','강습선→함교')]:
  rows=[x for x in r['trials'] if x['budget']==b and x['policy']==policy and not x['smart']];v=sm[key(b,policy)];times=[x['objective_at']-x['launch_at'] for x in rows if x.get('objective')];lines.append(f"| {b} | {label} | {v['mean_entered']:.2f} | {round(v['objective_rate']*24)}/24 | {v['mean_heroes_home']:.2f} | {v['mean_hero_dead']:.2f} | {statistics.mean(times):.1f} |")
lines+=['', '귀환 도착과 종료 생존은 다르다. 귀환 함선이 이후 완파되면 사망, 재나포됐으면 생환에서 제외했다. 종료 생존은 패배 함대의 FTL/별도 후송이 없는 가정이라 낮아질 수 있다. target_wreck_while_aboard는 내부 완파 사망의 별도 기록이다. 포드 비행 중 표적 소실은 회항을 끝까지 시뮬레이션하지 않고 구조/미해결 귀환 범주로 단순화한다.','', '**정밀도 제한:** 요격 뒤 영웅 숫자별 대표 편성의 선내 표본을 사용했다. 실제로 어떤 유형이 격추됐는지까지 동일 로스터로 이어가지 않아 역할별 영구 손실 균형을 인증하지 않는다. 강습선 파괴 시 구조 대기,700m 운송/80m 포드 이송,55초 자동 재탈환은 미정 운용의 실험 가정이다. 실제 선내 경비 이동/포로·재탈환을 구현한 결과가 아니다.','', '## 영웅이 전투를 단축했는가','', '영웅 실험70000번대와 **동일 시드**의 함대전 대조72회를 추가했다. 같은 시드라도 전황 변화 뒤 RNG 소비 순서는 달라질 수 있다. 양쪽 모두 아군 승리인 쌍의 시간 차이만 시간 단축으로 표시하고, 패배가 빨라진 경우를 성과로 세지 않는다.','', '| 코스트 | 작전 | 동시드 승리 증가/감소 | 양쪽 승리 쌍 | 그 쌍의 중앙 단축 초(음수=늘어남) |','| --- | --- | --- | --- | --- |']
for b in (100,200,300):
 base={x['seed']:x for x in controls['trials'] if x['budget']==b}
 for policy in ('weapon','capture','pod'):
  rows=[x for x in r['trials'] if x['budget']==b and x['policy']==policy and not x['smart']];gain=sum(x['winner']==0 and base[x['seed']]['winner']!=0 for x in rows);loss=sum(x['winner']!=0 and base[x['seed']]['winner']==0 for x in rows);deltas=[base[x['seed']]['seconds']-x['seconds'] for x in rows if x['winner']==base[x['seed']]['winner']==0];lines.append(f"| {b} | {policy} | +{gain}/−{loss} | {len(deltas)} | {statistics.median(deltas) if deltas else '대상 없음'} |")
lines+=['', '일반전 시간을 늘려 작전 창은 확보했지만, 점령 성공이 항상 빠른 승리로 이어지지는 않는다. 무기실 단일55초 제압의 영향이 함대 규모가 커질수록 희석되고, 나포는 재명령/재탈환에 좌우된다. 시간 단축의 일관성이 부족하면 반복 출격·강습 목표 선택·공급/엔진 공작·재탈환 저지·점령 유지의 전술을 먼저 모델링해야 한다. 모든 영웅에게 무조건 큰 피해 보너스를 주는 것으로 해결하지 않는다.','', '## 추가 반례와 해상도','']
if (R/'counter_checks.json').exists():
 extra=json.loads((R/'counter_checks.json').read_text());lines+=['소형 물량에 대한 손실이 소형의 모든 상성 우위를 뜻하는지 별도90000번대48회를 확인했다.','', '| 코스트 | 편성 | 승리/8 | 시간초과 |','| --- | --- | --- | --- |']
 for b in (100,200,300):
  for case in ('carrier_vs_light','mixed_anti_screen'):
   rows=[x for x in extra['trials'] if x['budget']==b and x['case']==case];lines.append(f"| {b} | {case} | {sum(x['winner']==0 for x in rows)}/8 | {sum(x['winner'] is None for x in rows)} |")
 lines+=['', 'mixed_anti_screen은 기존 혼성의 전함1척을 항모1척으로 바꾸며 코스트는 같다.8회뿐인 탐색 진단이므로 전체 메타 확정과 구분한다.','']
changes=0
for b in (100,200,300):
 for seed in sorted(set(x['seed'] for x in sens['trials'] if x['budget']==b)):
  pair=[x for x in sens['trials'] if x['budget']==b and x['seed']==seed];changes+=pair[0]['winner']!=pair[1]['winner']
lines += [f'1초/0.5초 비교9쌍 중 승자 변경{changes}쌍. 집계 모형의1초 스텝은0.9초 빔/0.22초 살보를 정밀 재현하지 못한다. 시간·성공률은 근사이며 실제게임은 고정 스텝/사건별 스케줄·공간 판정으로 재측정해야 한다.','', '## 확인한 오류 위험과 후속 순서','', '1. 전체HP만 늘리고 코어/함교/장갑/유폭을 그대로 두면 실제 함선은 여전히 빨리 마비/완파된다. 새 비율로 시설HP까지 재설정한 실제XYZ 교전을 먼저 검증한다.','2. 전체 함대PD를 단일 포드에 합산하거나, 귀환 내내 적PD가 닿는 것으로 계산하면 수송이 과도하게 약해진다. 반대로 국소12기 근사가 방공함 집중을 누락할 수 있어 공간별 교차 사격/포화 시험이 필요하다.','3. v0.3a 포드240HP는 요격 그룹을 너무 쉽게 통과해b에서160HP로 줄였다. b의 소형 물량 강세를 확인해c에서는 초계/호위HP를v0.2의6배로 낮췄다. 최종 일반전은 새50000번대, 연동은70000번대 시드에서 전부 다시 실행했다. 이전 후보는archive에 보존하고 유효912회에 더하지 않았다.','4. 소형 물량/항모 집중/방공함 대체 결과에서 범용 혼성 구성이 최적이라는 가정은 성립하지 않았다. PD/격납고/기체 손실·지역 커버를 실제 전장에 옮긴 뒤 역할별 효용을 조정한다. 소형·중형의 수를 강제하는 편성 의무를 임의 추가하지 않는다.','5. D113으로 현재 초대형의 강한 결전 화력은 유지한다. 하향 조정 과제에서 제외하고 영웅/수송/방어 조건의 검증에 집중한다.','6. 강습 대상이 버티는 시간은 확보됐지만 승전 시간 단축·사망·최종 후송은 별개의 지표다. 사전 후퇴·구조 인양·예비대/보급·다중 방어/반격을 넣어 캠페인 손실률을 재산정한다.','', '## 적용 범위','', '모든 함종을 포함한 비용/생존 시간 설계는 진행했지만 UE 프로토타입의 전투 시간이 검증됐다는 뜻은 아니다. XYZ/상하 사각/아군 사선·충돌·레이더 최고 공유·시설/승무원/재난·실제 경비 경로·함재기 궤적·FTL·전략경제는 집계 근사 또는 미포함이다. 현재 숫자는 데이터 후보이며 실제2B→2C→선내전 단계에서 검증한다.','']
defense=json.loads((R/'defense_checks.json').read_text())
assert defense['profile_sha256']==r['profile_sha256']
lines+=['', '추가 선내 방어224회는 함대 연동과 분리했다. 일반 경비8명에 방어 영웅1명 추가, 또는 경비12명+방어 영웅2명의 강한 방어를 비교했다. 두 조건 사이에는 경비 수까지 달라 영웅 수만의 순수 효과로 해석하지 않는다.','', '| 방어 편성 | 공격 혼합4 승리/32 | 교전 중앙 초 | 평균 공격자 생존 |','| --- | --- | --- | --- |']
for role,v in defense['summary'].items():lines.append(f"| {role} | {v['attacker_wins']}/32 | {v['median_seconds']} | {v['mean_attacker_survivors']:.2f} |")

routes=json.loads((R/'route_checks.json').read_text());assert routes['profile_sha256']==r['profile_sha256'];assert routes['model_sha256']==r['model_sha256']
lines+=['', '## 깊은 내부 경로 스트레스36회','', '실제 초대형 그래프를 구현한 것이 아니라 선내/복귀 이동 시간을3배로 늘린 별도 진단이다. 시드120000번대·각6회이며 본 평가의성공률에 합산하지 않는다.','', '| 코스트 | 이동 배수 | 점령 성공/6 | 내부 완파 사건/6 |','| --- | --- | --- | --- |']
for budget in (100,200,300):
 for mult in (1,3):
  rr=[x for x in routes['trials'] if x['budget']==budget and x['travel_multiplier']==mult];lines.append(f"| {budget} | {mult} | {sum(x.get('objective',0)>0 for x in rr)}/6 | {sum(x.get('target_wreck_while_aboard',0)>0 for x in rr)}/6 |")
lines+=['', '깊은 경로에서 내부 완파가 남으면 전체 전투가 길다는 이유로 영웅 생존을 보장하지 않는다. 실제 슬롯 그래프별 왕복시간과 개별 표적의 집중 피격 생존 시간을 함께 검증한다.','']
lines[4:4]=['![장기전 결과 그래프](../../reference/FleetEndurance/results.png)','']
lines[2:2]=['> **적용 범위:** 아래는 당시 v0.3 모형 결과다. 현재 [영웅 v0.5](../01_design/boarding/HERO_SYSTEMS.md)·[수리 v0.5](../01_design/fleet/REPAIR_LOGISTICS.md) 및 [D113 강습 재검토](BOARDING_REVIEW.md)의 새 조건을 모두 통합한 검증 결과가 아니다.', '']
Path('docs/04_analysis/FLEET_ENDURANCE_RESULTS.md').write_text('\n'.join(lines))
print('Report generated')
