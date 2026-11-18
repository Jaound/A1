"""표시면 수직 방향에서 후보 상자가 표시 무장의 앞을 가리는지 근사한다."""
import json
from pathlib import Path

project = Path(__file__).resolve().parents[1]
repo = project.parents[1]
marked = json.loads((repo / 'docs/validation/PlayUIR2/JX_무장표시_표면투영후보.json').read_text(encoding='utf8'))
candidates = json.loads((repo / 'docs/validation/PlayUIR2/KQ_선체충돌_후보.json').read_text(encoding='utf8'))
output = project / 'Saved/Validation/MarkedHullCollisionCandidateAudit_v2.json'
if output.exists():
    raise FileExistsError(output)
table = {s['hull']: s for s in candidates['ships']}
rows = []
for ship in marked['ships']:
    if ship['id'] not in table:
        continue
    for slot in ship['candidates']:
        axis, sign = {'top': (2, 1), 'bottom': (2, -1), 'left': (1, -1), 'right': (1, 1)}[slot['view']]
        point = slot['local_cm']
        surfaces = []
        for box in table[ship['id']]['boxes']:
            center, extent = box['local_center_cm'], box['half_extent_cm']
            if all(abs(point[i]-center[i]) <= extent[i] for i in range(3) if i != axis):
                surfaces.append(sign * (center[axis]+sign*extent[axis]))
        blocked = bool(surfaces) and max(surfaces) > sign*point[axis]+18+1e-4
        rows.append({'hull': ship['id'], 'slot': f"{slot['view']}_{slot['color']}_{slot['region']}",
                     'candidate_box_precedes_mount_sphere': blocked})
result = {'scope': '원본 블렌더 표면 샘플 후보. 표시면 수직 입사에 대한 단순 상자/구 기하 비교이며 UE 공격·피해 검사가 아님.',
          'hulls': len(table), 'marks': len(rows), 'blocked': sum(r['candidate_box_precedes_mount_sphere'] for r in rows),
          'heldout_covered': sum(x['heldout_endpoints_covered'] for x in table.values()),
          'heldout_total': sum(x['heldout_hit_columns'] for x in table.values()), 'cases': rows}
output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
print('후보 함체', result['hulls'], '표시점', result['marks'], '수직 입사 차폐', result['blocked'])
