"""Prove unchanged dependencies before reusing isolated indoor trials."""
import ast
import gzip
import hashlib
import json
from pathlib import Path


def segments(text, selected):
    out={};tree=ast.parse(text)
    for node in tree.body:
        if isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef)) and node.name in selected:
            out[node.name]=ast.get_source_segment(text,node)
        if isinstance(node,ast.ClassDef):
            for method in node.body:
                key=f'{node.name}.{getattr(method,"name","")}'
                if key in selected:out[key]=ast.get_source_segment(text,method)
    assert set(out)==set(selected)
    return out


def validate_reuse(root,source):
    source=Path(source).resolve()
    raw=gzip.decompress(source.read_bytes()) if source.suffix=='.gz' else source.read_bytes()
    previous=json.loads(raw)
    archive=root/'archive'
    for file in ('model.py','run.py','indoor.py','profile.json'):
        saved=archive/(file+'.before_review.txt')
        assert hashlib.sha256(saved.read_bytes()).hexdigest()==previous['hashes'][file],file
    assert (root/'profile.json').read_bytes()==(archive/'profile.json.before_review.txt').read_bytes()
    assert (root/'indoor.py').read_bytes()==(archive/'indoor.py.before_review.txt').read_bytes()
    selected=('Battle.__init__','Battle.make_unit','Battle.add_unit','Battle.living','Ship.__init__','Ship.alive','cost')
    current=segments((root/'model.py').read_text(),selected)
    old=segments((archive/'model.py.before_review.txt').read_text(),selected)
    assert current==old,'Indoor initialization dependency changed'
    assert segments((root/'run.py').read_text(),('indoor_trial',))==segments((archive/'run.py.before_review.txt').read_text(),('indoor_trial',))
    digest=hashlib.sha256(json.dumps(current,sort_keys=True).encode()).hexdigest()
    metadata={'source':str(source.relative_to(root)),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
              'initialization_dependency_sha256':digest,'indoor_sha256':previous['hashes']['indoor.py'],
              'reason':'Fleet-event fixes do not alter indoor.py, profile, indoor_trial or its ship/unit initialization dependencies.'}
    return previous,metadata
