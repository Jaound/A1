"""Re-evaluate only assault transport cases after extraction-path correction."""
import ast
import copy
import gzip
import hashlib
import json
import time
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path
from run import run_case,summary

R=Path(__file__).parent;A=R/'archive/post_guard_review'


def stripped(text):
    tree=ast.parse(text)
    for node in ast.walk(tree):
        if isinstance(node,ast.ClassDef) and node.name=='Battle':
            for method in node.body:
                if isinstance(method,ast.FunctionDef) and method.name in ('operations_step','update_motion'):
                    method.body=[ast.Pass()]
    return ast.dump(tree,include_attributes=False)


def main():
    raw=gzip.decompress((A/'results.json.gz').read_bytes());old=json.loads(raw)
    for name in ('model.py','run.py','indoor.py','profile.json'):
        assert hashlib.sha256((A/(name+'.txt')).read_bytes()).hexdigest()==old['hashes'][name]
    for name in ('run.py','indoor.py','profile.json'):assert (A/(name+'.txt')).read_bytes()==(R/name).read_bytes()
    assert stripped((A/'model.py.txt').read_text())==stripped((R/'model.py').read_text())
    affected=[c for c in old['cases'] if c['options'].get('mode')=='assault']
    ids={c['id'] for c in affected}
    rows=[{**x,'reused_unaffected':True} for x in old['trials'] if x['case'] not in ids]
    before=len(rows);start=time.monotonic()
    with ProcessPoolExecutor(max_workers=2) as pool:
        for i,(case,result) in enumerate(pool.map(run_case,[(c,c['n'],old['seed_base']) for c in affected]),1):
            rows+=result;print('extraction refinement',i,'/',len(affected),case['id'],round(time.monotonic()-start,1),flush=True)
    new=copy.deepcopy(old);new['trials']=rows;new['summary']=summary(rows)
    new['hashes']={name:hashlib.sha256((R/name).read_bytes()).hexdigest() for name in (*old['hashes'],'refine.py')}
    new['extraction_refinement']={'source':'archive/post_guard_review/results.json.gz',
                                 'source_sha256':hashlib.sha256((A/'results.json.gz').read_bytes()).hexdigest(),
                                 'rerun_trials':len(rows)-before,'reused_trials':before,'affected_cases':sorted(ids),
                                 'proof':'AST unchanged except operations_step assault-only return branch and update_motion guarded by extracting, set only in that branch. Profile, indoor.py and runner unchanged.'}
    (R/'results.json').write_text(json.dumps(new,ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
