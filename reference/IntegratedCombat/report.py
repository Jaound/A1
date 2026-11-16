"""Render saved results; does not execute battles."""
import hashlib
import json
import math
import statistics
from pathlib import Path

R=Path(__file__).parent;ROOT=R.parent.parent
r=json.loads((R/'results.json').read_text());summary=r['summary'];rows=r['trials']
for name,digest in r['hashes'].items():assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name


def wilson(k,n):
    z=1.96;p=k/n;den=1+z*z/n;mid=(p+z*z/(2*n))/den;half=z*math.sqrt(p*(1-p)/n+z*z/(4*n*n))/den
    return f'{max(0,mid-half)*100:.0f}–{min(1,mid+half)*100:.0f}%'


def minutes(value):return '미도달' if value is None else f'{value/60:.2f}'
def med(values):return statistics.median(values) if values else None


lines=['# D116 통합 전투 시험 결과 v0.6','',
       f'> 평가 **{len(rows):,}회 / {len(r["cases"])}조건**, 시드{r["seed_base"]}번대. Python 설계 모형이며 UE/최종 에셋 실행 결과가 아니다. [기준과 수정](../02_balance/INTEGRATED_COMBAT_BALANCE.md) · [재현](../../reference/IntegratedCombat/README.md) · [전체 원시 결과](../../reference/IntegratedCombat/results.json). 탐색 실행은 평가 표본에 합산하지 않았다.','',
       '각 조건의 같은 시드를 짝지어 비교한다. 반복 시드는 조준/초기 위상/표적·교전 변동이며 실제 플레이어/맵 전체의 표집이 아니다. 선내전·함대전·민감도 실행 수를 합쳐 하나의 승률 표본으로 해석하지 않는다. 개발용 입력/로그의 적 편제는 상대AI나HUD에 공개되지 않는다.','',
       '## 일반 혼성 함대전 — 영웅 없음','',
       '| 코스트 | 표본 | 주력 교전 중앙 분 | 최종 종료 중앙 분 | 최초 완파 중앙 분 | 좌/우 승리 | 미해결 |','| --- | --- | --- | --- | --- | --- | --- |']
for budget in (100,200,300):
    s=summary[f'fleet_mixed_{budget}'];lines.append(f'| {budget} | {s["n"]} | {minutes(s["median_main_seconds"])} | {minutes(s["median_seconds"])} | {minutes(s["median_first_loss_seconds"])} | {s["left_wins"]}/{s["right_wins"]} | {s["unresolved"]} |')
lines+=['','주력 교전은 한쪽 잔여 코스트30% 도달이다. 최종 종료에는 추격·잔존함 처리·개별 퇴각이 포함된다.10~15분은 일반 혼성전의 목표이며 모든 압도적/특수 편성을 그 구간에 강제로 넣지 않는다.','',
        '## 모든 함종과 편성 대응','',
        '| 우측 편성 | 코스트 | 좌측 혼성 승리/표본 | 95% 구간 | 최종 중앙 분 | 미해결 |','| --- | --- | --- | --- | --- | --- |']
names={'heavy':'전함 집중','missile':'미사일 집중','carrier':'항모 집중','xl':'초대형 우선(100은 대형)','logistics':'채굴/수송 호위','no_pd':'방공함→순양함','light':'소형 다수'}
for budget in (100,200,300):
    for kind,name in names.items():
        s=summary[f'fleet_{kind}_{budget}'];lines.append(f'| {name} | {budget} | {s["left_wins"]}/{s["n"]} | {wilson(s["left_wins"],s["n"])} | {minutes(s["median_seconds"])} | {s["unresolved"]} |')
lines+=['','함체 코스트만 같고 건조 자원/기체 구매/연구비가 같다는 뜻은 아니다. 임무 선박은 호위군 안에서 평가하며 단독 결투 승률로 채굴/수송 효용을 판정하지 않는다. 소형 다수 표본은4회로 특히 작다. PD/함재기는 거리와 공유 요격 예산을 반영한 집계 근사다. 초대형 함체·전용 주포 수치는 유지했다.','',
        '## 강습·보류 정책 — 양측 최종 편제 한도15 / L10','',
        '| 코스트 | 수송 | 사격 정책 | 영웅 진입 시행 | 목표 성공 시행 | 평균 진입 영웅 | 평균 귀환 영웅 | 목표 완파/탈출 전 | 종료 중앙 분 |','| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
for budget in (100,200,300):
    for method in ('pod','assault'):
        for policy in ('off','until_objective','until_exit'):
            s=summary[f'linked_{budget}_{method}_{policy}'];lines.append(f'| {budget} | {"개인 포드" if method=="pod" else "강습선"} | {policy} | {s["entry_trials"]}/{s["n"]} | {s["objective_trials"]}/{s["n"]} | {s["mean_entered_heroes"]:.2f} | {s["mean_returned_heroes"]:.2f} | {s["wreck_before_exit_trials"]}/{s["n"]} | {minutes(s["median_seconds"])} |')
lines+=['','계속 사격(off), 목표 달성까지 보류(until_objective), 내부 이탈까지 보류(until_exit)다. 신규 공격만 차단하며 기존탄은 유지한다. 귀환 도착과 전투 종료 시 생존은 별개다. 강습선/포드는 적의 서로 다른 무장에 노출되므로 단순 수치만으로 모든 상황의 우열을 보장하지 않는다.','',
        '| 코스트/수송·기본 보호 | 진입→목표 중앙 초(성공만) | 진입→내부 이탈 중앙 초(완료만) | 이탈 완료/진입 | 작전 영웅 전투종료 사망 |','| --- | --- | --- | --- | --- |']
for budget in (100,200,300):
    for method in ('pod','assault'):
        rs=[x for x in rows if x['case']==f'linked_{budget}_{method}_until_exit']
        timelines=[o for x in rs for o in x.get('operation_timeline',[]) if o['entry'] is not None]
        objectives=[o['objective']-o['entry'] for o in timelines if o['objective'] is not None]
        exits=[o['exit']-o['entry'] for o in timelines if o['exit'] is not None]
        dead=sum(u['kind']!='soldier' and u['hp']<=0 for x in rs for o in x.get('operation_timeline',[]) for u in o['units'])
        fmt=lambda x:'없음' if x is None else f'{x:.1f}'
        lines.append(f'| {budget}/{method} | {fmt(med(objectives))} | {fmt(med(exits))} | {len(exits)}/{len(timelines)} | {dead} |')
lines+=['','위 시간은 완료 사례에 한정한다. 실패/미완료를0초로 섞지 않는다. 목표가 살아서 끝난 경우의 생존 시간은 우측 중도절단이며 원시 `operation_timeline.observed_survival_after_entry`에 관측 하한을 남겼다. 작전 영웅 사망에는 정상 귀환 후 아군 함선 완파도 포함되므로 진입 직후 사망률로 해석하지 않는다.','',
        '## 선내전 — 공격/방어 영웅 수·성장','',
        '양측 일반 병력8명, 초대형 임시 경로, 같은 레벨/장비 없는 조건. 아래는 이동을 포함한 교전 승리이며 함교 점령/함대 승리와 다르다.','',
        '| 공격 영웅 | 방어 영웅 | 레벨 | 공격 승리 | 방어 승리 | 미해결 | 중앙 초 |','| --- | --- | --- | --- | --- | --- | --- |']
for attack in (1,2,4):
    for defense in (0,2,4,6):
        for level in (1,10,20):
            s=summary[f'indoor_{attack}_{defense}_L{level}'];lines.append(f'| {attack} | {defense} | {level} | {s["left_wins"]}/{s["n"]} | {s["right_wins"]}/{s["n"]} | {s["unresolved"]} | {s["median_seconds"]:.1f} |')
lines+=['','유형 집중/중간 레벨/장비/레벨 격차는 아래 표다. 조건명은 입력 키와 대응한다.','',
        '| 조건 | 공격 승리/표본 | 방어 승리 | 미해결 | 중앙 초 |','| --- | --- | --- | --- | --- |']
for key in summary:
    if key.startswith(('indoor_focus','indoor_growth','indoor_gear','indoor_level_gap')):
        s=summary[key];lines.append(f'| {key} | {s["left_wins"]}/{s["n"]} | {s["right_wins"]} | {s["unresolved"]} | {s["median_seconds"]:.1f} |')
lines+=['','## 연구·무장 조준·비대칭 임무','',
        '| 조건 | 좌측 승리 | 우측 승리 | 미해결 | 종료 중앙 분 |','| --- | --- | --- | --- | --- |']
prefixes=('hangar_','fighter_','mission_','weapon_aim','baseline_hull_aim','linked_weapon_override','bridge_capture','facility_sabotage','linked_growth')
for key,s in summary.items():
    if key.startswith(prefixes):lines.append(f'| {key} | {s["left_wins"]}/{s["n"]} | {s["right_wins"]} | {s["unresolved"]} | {minutes(s["median_seconds"])} |')
lines+=['','연구/개조는 좌측만 적용하고 우측은 미연구/미개조 대조다. 용량 확대군은 추가 기체를 구매해 만재하므로 무료 강화 비교가 아니다. 실제 초기 기체/구매 가격은 원시 `initial_fighters` / `fighter_package_cost`에 기록한다. 임무의 적75/125/150은 개발용 조건이며 적 코스트 표시가 아니다.','',
        '## 시간 간격·실행 순서 민감도','',
        '| 조건 | 좌/우 승리 | 미해결 | 중앙 분 |','| --- | --- | --- | --- |']
for key,s in summary.items():
    if key.startswith(('dt_','reverse_order','deep_layout')):lines.append(f'| {key} | {s["left_wins"]}/{s["right_wins"]} | {s["unresolved"]} | {minutes(s["median_seconds"])} |')
lines+=['','시간 간격/순서 비교는 작은 표본의 민감도 진단이다. 결과가 비슷해도 메시 충돌·수치 수렴·공정성을 인증하지 않는다. 선내전 자체는0.25초 하위 단계를 사용한다.','',
        '## 적용 범위와 남은 검증','',
        '- 통합 규칙: 비활성 무지연/인원 기반 쉴드, 개별 운송과 실제 임시 방 이동/교전, 관측 기반 표적, 아군 내부 잔류 보호, 무장HP 단독 피해, 실제 점령 기반 재탈환, 유한 자원/단일 수리 작업을 적용했다.',
        '- 실제3D 포탑 선회·메시 광선/사선·선체 롤/충각·문 폭/봉쇄, 기체 개별 궤도, 포로 확보/비상탈출 모든 경로·동시 구조 경쟁, 기지/5시간 경제는 완성된 범위가 아니다.',
        '- 패배/미해결·강습 손실·전투시간 목표 이탈은 그대로 보고한다. 결과에 맞추어 원시 데이터를 고치거나 모든 편성을10~15분으로 강제하지 않았다.',
        '- 실제UE/에셋 배치가 들어오면 동일 시나리오를 다시 실행한다. 현재 결과로 최종 밸런스 완료를 선언하지 않는다.','']
# Keep Korean report terminology consistent.
text='\n'.join(lines)
reuse=r.get('reused_indoor')
if reuse:
    count=reuse['rows']
    notice=f'\n\n**수정 후 실행 범위:** {len(rows)-count}회 함대/연동/민감도를 새로 실행하고, 변경 영향이 없는 독립 선내전{count}회를 재사용했다. 재사용 전에 선내 코드/프로필·초기화 의존 함수·실행 함수가 동일함을 AST/해시로 검사했다. 수정 전1328회와 합산하지 않는다. [이전 입력/결과와 수정 근거](../../reference/IntegratedCombat/archive/README.md).'
    text=text.replace('\n\n## 일반 혼성 함대전',notice+'\n\n## 일반 혼성 함대전',1)
if r.get('extraction_refinement'):
    m=r['extraction_refinement']
    notice=f'\n\n**강습선 귀환 후속 보정:** 강습선으로 진입한 인원을 포드로 귀환시키던 처리를 고쳤다. 생존 강습선으로 회수하고 수송선 상실 시 외곽15초 준비 후 구조 대기를 적용했다. 영향 있는{m["rerun_trials"]}회를 재실행하고 나머지{m["reused_trials"]}회는 입력/코드 변경 범위를 확인해 재사용했다. 각 단계의 출처/해시는 원시 결과에 보존하며 중복 합산하지 않는다.'
    text=text.replace('\n\n## 일반 혼성 함대전',notice+'\n\n## 일반 혼성 함대전',1)

controls=['## 영웅 보직 대비 강습의 교환','',
          '| 코스트 | 보직/수호15명, 강습 없음 종료 분 | 포드/이탈 보호 종료 분 | 강습선/이탈 보호 종료 분 |','| --- | --- | --- | --- |']
for b in (100,200,300):
    controls.append(f'| {b} | {minutes(summary[f"heroes_only_{b}"]["median_seconds"])} | {minutes(summary[f"linked_{b}_pod_until_exit"]["median_seconds"])} | {minutes(summary[f"linked_{b}_assault_until_exit"]["median_seconds"])} |')
controls+=['','양측 세력 예산15명·L10을 유지한다. 강습4명을 보직/수호에서 운송으로 전환하는 기회비용을 포함한다. 단순히 영웅 없는 함대와 비교해 강습만의 효과라고 주장하지 않는다. 표적/결과에 따른 전투 분기가 달라져 차이는 조건부 정책 비교다.','']
text=text.replace('## 선내전 — 공격/방어 영웅 수·성장','\n'.join(controls)+'\n## 선내전 — 공격/방어 영웅 수·성장')

extra=R/'continuation_results.json'
if extra.exists():
    consecutive=json.loads(extra.read_text())
    for name,digest in consecutive['hashes'].items():assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
    table=['## 연속2회 교전 — 무상 정비/재보급 없음','',
           '| 첫 편제 | 첫 전투 승리 | 다음 전투 진입 | 다음 전투 승리 | 첫 전투 선체 복구 평균 |','| --- | --- | --- | --- | --- |']
    for repair in (False,True):
        rs=[x for x in consecutive['trials'] if x['repair']==repair]
        reached=[x for x in rs if x['second'] is not None]
        table.append(f'| {"수리선1" if repair else "수리선 자리에 호위함1"} | {sum(x["first"]["winner"]==0 for x in rs)}/{len(rs)} | {len(reached)}/{len(rs)} | {sum(x["second"]["winner"]==0 for x in reached)}/{len(reached)} | {statistics.mean(x["first"]["metrics"].get("repair_hull_hp",0) for x in rs):.1f} |')
    table+=['','별도16개 연속 시나리오(최대32교전)다. 플레이어100 대 적75의 교전 후 살아남은 실제 함선/인원/기체/체력/쿨타임/화물을 다음 적100 임무로 넘겼다. 수리/보충 없이 강행하므로 적 전력을 다시 동등하게 맞추지 않는다. 사망/포로/적함 잔류 영웅을 순간 귀환시키지 않는다. 첫 원정 전체의 최적 함대/경제나 기지 정비의 효과를 검증한 것은 아니다.','']
    text=text.replace('## 적용 범위와 남은 검증','\n'.join(table)+'\n## 적용 범위와 남은 검증')

supplement=R/'supplement_results.json'
if supplement.exists():
    data=json.loads(supplement.read_text())
    for name,digest in data['hashes'].items():assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
    table=['## 배치 수와 좌측 무장 명령의 보조 비교','',
           '| 조건 | 좌/우 승리 | 미해결 | 종료 중앙 분 | 좌측 영웅 사망 평균 | 목표 성공 시행 | 적 공격 무력화 시행 |','| --- | --- | --- | --- | --- | --- | --- |']
    for key in data['cases']:
        rs=[x for x in data['trials'] if x['case']==key]
        dead=statistics.mean(sum(v for k,v in x['hero_states'].items() if k=='0:dead') for x in rs)
        table.append(f'| {key} | {sum(x["winner"]==0 for x in rs)}/{sum(x["winner"]==1 for x in rs)} | {sum(x["winner"] is None for x in rs)} | {statistics.median(x["seconds"] for x in rs)/60:.2f} | {dead:.2f} | {sum(x["metrics"].get("objectives",0)>0 for x in rs)}/{len(rs)} | {sum(x["enemy_weapon_disabled"] for x in rs)}/{len(rs)} |')
    table+=['','별도40회·930000번대다. active_6/10/15는 양측 같은 배치 한도에서 강습4명을 포함한 분배다. 좌측 무장 명령은 실제 관측된 살아 있는 포탑만 고르고 없으면 해당 명령을 완료한다. 우측은 선체 공격을 유지한다. 공격 무력화는 함대 격파/승리와 별도 목표다. 명령 의도의 차이와 조준 오차를 함께 평가하며100% 무장만 맞는다고 가정하지 않는다.','']
    text=text.replace('## 적용 범위와 남은 검증','\n'.join(table)+'\n## 적용 범위와 남은 검증')

text=text.replace('## 일반 혼성 함대전','![통합 전투 결과 요약](../../reference/IntegratedCombat/results.png)\n\n## 일반 혼성 함대전',1)
findings=['## 판정과 후속 조정 우선순위','',
          '일반 혼성전의 최종 종료 중앙값은 '+', '.join(f'{b}코스트 {summary[f"fleet_mixed_{b}"]["median_seconds"]/60:.2f}분' for b in (100,200,300))+'이다. **목표10~15분의 최종 밸런스에 도달했다고 판정하지 않는다.** 특히100/200의 잔존함 처리와 시설 마비/퇴각 조건을 먼저 실제UE에서 대조할 필요가 있다.',
          '',
          '- 포드/강습선 진입 성공, 시설 목표 성공, 내부 이탈, 아군 함선 재탑승, 전투 종료 생존은 각각 다른 지표다. 강습선 귀환 경로 보정 후에도 수송선 완파·내부 전투·탈출 뒤 피격의 손실은 남을 수 있다.',
          '- 영웅 없는 상대의 높은 강습 성공률을 일반 승률로 쓰지 않는다. L10/4영웅 공격의 방어0/2/4/6명 조건을 따로 읽는다. 유형 집중군은 양측 같은 유형이므로 그 표만으로 유형별 절대 우열을 판정하지 않는다.',
          '- 무장 명령은 적을 살린 채 무력화할 수 있다. 격파가 아닌 무력화 임무의 성공과 일반전 종료를 분리하고, 후속 나포/철수 명령을 연결해야 한다.',
          '-15명 분산 보직 시험은 상주 영웅의 자동 대피/최적 배치를 모두 구현한 것이 아니다. 손실률을 보고 영웅HP부터 올리거나 영구 사망을 폐기하지 않는다. 후속 실제 공간 시험에서 보직 집중/예비대·아군 함선 대피 정책을 함께 검증한다.',
          '- 연속 원정에서 수리량이 증가해도 부족한 승무원·손실 전력·수리 화물·치료를 모두 대체하지는 못한다. 두 번째 임무의 승리 보장으로 해석하지 않는다.',
          '-0.5/1/2초 비교의 전투 시간 차이는 수치 민감도다. 미사일/포드0.25초 사건과 PD 공유 예산·사격 주기를 실제UE의 고정 단계/이벤트 시각에서 다시 검증하기 전 작은 승률 차이를 최종 수치 근거로 쓰지 않는다.','']
text=text.replace('## 적용 범위와 남은 검증','\n'.join(findings)+'\n## 적용 범위와 남은 검증')
(ROOT/'docs/04_analysis/INTEGRATED_COMBAT_RESULTS.md').write_text(text)
print('Report generated from',len(rows),'saved trials; no battles executed.')
