"""Verify saved data provenance, aggregates and selected deterministic replays."""
import hashlib,json
from pathlib import Path
import run
ROOT=Path(__file__).parent
r=json.loads((ROOT/'results.json').read_text())
for file,key in [('profile.json','profile_sha256'),('engine.py','engine_sha256'),('run.py','runner_sha256')]:
 assert hashlib.sha256((ROOT/file).read_bytes()).hexdigest()==r[key],file
assert len(r['trials'])==1136 and len(r['resolution_checks'])==12
assert len({(t['case'],t['seed'],t['dt']) for t in r['trials']})==1136
assert run.aggregate(r['trials'])==r['summary']
for t in r['trials']:
 if t['case'].startswith('M'):
  assert sum(t[k] for k in ('heroes_extracted','heroes_dead','heroes_rescue','heroes_aboard'))<=4,t
for case,seed in [('F01',1000),('H01',4000),('M05',4000)]:
 old=next(t for t in r['trials'] if t['case']==case and t['seed']==seed)
 assert run.trial((case,seed,.1))==old,(case,seed)
print('PASS: source hashes, 1136 unique trials, 12 resolution pairs, aggregates, hero counts and 3 end-to-end replays')
