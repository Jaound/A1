"""Verify saved artifacts and invariants; never executes a battle."""
import hashlib
import json
from collections import Counter
from pathlib import Path
from run import summary
from dependencies import validate_reuse

R=Path(__file__).parent;r=json.loads((R/'results.json').read_text())
for key,digest in r['hashes'].items():assert hashlib.sha256((R/key).read_bytes()).hexdigest()==digest,key
profile=json.loads((R/'profile.json').read_text())
for path,digest in profile['provenance'].items():assert hashlib.sha256((R.parent.parent/path).read_bytes()).hexdigest()==digest,path
assert not r['exploratory'] and r['seed_base']==810000
assert r['summary']==summary(r['trials'])
by_case=Counter(x['case'] for x in r['trials'])
assert by_case=={c['id']:c['n'] for c in r['cases']}
seen_hulls=set()
for x in r['trials']:
    if 'initial_heroes' in x:
        assert all(n<=15 for n in x['initial_heroes'])
        for side,n in enumerate(x['initial_heroes']):assert sum(v for k,v in x['hero_states'].items() if k.startswith(str(side)+':'))==n
    if 'loadout' in x:
        for loadout in x['loadout'].values():seen_hulls.update(loadout)
    for op in x.get('operation_timeline',[]):
        ids=[u['id'] for u in op['units']];assert len(ids)==len(set(ids))==12
        if op['entry'] is not None:assert op['launch']<=op['entry']
        if op['objective'] is not None:assert op['entry']<=op['objective']
        if op['exit'] is not None:assert op['entry']<=op['exit']
assert len(seen_hulls)==16,seen_hulls
if r.get('reused_indoor'):
    meta=r['reused_indoor'];previous,checked=validate_reuse(R,R/meta['source'])
    assert checked['source_sha256']==meta['source_sha256']
    assert checked['initialization_dependency_sha256']==meta['initialization_dependency_sha256']
    assert sum(bool(x.get('reused_indoor')) for x in r['trials'])==meta['rows']
    old={x['case']+':'+str(x['seed']):x for x in previous['trials']}
    for x in r['trials']:
        if x.get('reused_indoor'):
            clean=dict(x);clean.pop('reused_indoor');clean.pop('reused_unaffected',None);assert clean==old[x['case']+':'+str(x['seed'])]
if r.get('extraction_refinement'):
    import gzip
    from refine import stripped
    meta=r['extraction_refinement'];source=R/meta['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest()==meta['source_sha256']
    previous=json.loads(gzip.decompress(source.read_bytes()))
    assert stripped((R/'model.py').read_text())==stripped((source.parent/'model.py.txt').read_text())
    old={x['case']+':'+str(x['seed']):x for x in previous['trials']}
    for x in r['trials']:
        if x.get('reused_unaffected'):
            clean=dict(x);clean.pop('reused_unaffected');assert clean==old[x['case']+':'+str(x['seed'])]
    assert sum(not bool(x.get('reused_unaffected')) for x in r['trials'])==meta['rerun_trials']
print('PASS: source/input hashes, regenerated summaries, case counts, faction hero cap, 16 hulls, transport IDs and operation chronology.')
extra=R/'continuation_results.json'
if extra.exists():
    c=json.loads(extra.read_text())
    for key,digest in c['hashes'].items():assert hashlib.sha256((R/key).read_bytes()).hexdigest()==digest,key
    for row in c['trials']:
        if row['second'] is None:continue
        assert row['second']['initial_cost'][0]<=row['first']['initial_cost'][0]+row['first']['initial_cost'][1]
        assert row['second']['seconds']>=row['first']['seconds']
        assert all(u['hp']>0 for s in row['persistent_start'] for u in s['heroes'])
    print('PASS: consecutive-encounter hashes, chronology and live persistent hero records.')
extra=R/'supplement_results.json'
if extra.exists():
    s=json.loads(extra.read_text())
    for key,digest in s['hashes'].items():assert hashlib.sha256((R/key).read_bytes()).hexdigest()==digest,key
    assert len(s['trials'])==40
    for row in s['trials']:
        if row['case'].startswith('active_'):assert row['initial_heroes']==[int(row['case'].split('_')[1])]*2
    print('PASS: supplemental hashes and faction-wide 6/10/15 deployment budgets.')
