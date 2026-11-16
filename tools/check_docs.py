"""Check repository documentation paths without executing models or report generators.

Run from any directory: python /path/to/A1/tools/check_docs.py
Checks local inline Markdown links (including images), explicit R anchors, the move
map, and Markdown links/output paths embedded in the three report generators.
External URLs and historical Windows paths are deliberately not fetched.
"""
import ast
import json
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"\]\(([^\s)]+)\)")
errors = []
checked = 0


def check_links(text, source, label):
    global checked
    for match in LINK.finditer(text):
        url = match[1]
        parts = urlsplit(url)
        if parts.scheme or parts.netloc or url.startswith('/'):
            continue
        target = (source.parent / unquote(parts.path)).resolve()
        checked += 1
        if not target.exists():
            errors.append(f'{label}: missing {url}')
        elif re.fullmatch(r'r\d+', parts.fragment) and target.suffix == '.md':
            if f'id="{parts.fragment}"' not in target.read_text(encoding='utf-8'):
                errors.append(f'{label}: missing anchor {url}')


paths = [ROOT / 'README.md', *sorted((ROOT / 'docs').rglob('*.md')),
         *sorted((ROOT / 'reference').rglob('*.md'))]
for path in paths:
    check_links(path.read_text(encoding='utf-8'), path, str(path.relative_to(ROOT)))

moves = json.loads((ROOT / 'docs/DOCUMENT_PATHS.json').read_text(encoding='utf-8'))['moved']
assert len(moves) == len(set(moves.values())), 'Duplicate document destinations'
for old, new in moves.items():
    if (ROOT / old).exists() or not (ROOT / new).is_file():
        errors.append(f'Invalid move: {old} -> {new}')

reports = {
    'reference/CombatSimulation/write_report.py': 'docs/archive/analysis/COMBAT_SIMULATION.md',
    'reference/FleetEndurance/report.py': 'docs/04_analysis/FLEET_ENDURANCE_RESULTS.md',
    'reference/BoardingReview/report.py': 'docs/04_analysis/BOARDING_REVIEW.md',
}
for script, output in reports.items():
    tree = ast.parse((ROOT / script).read_text(encoding='utf-8'), filename=script)
    strings = [node.value for node in ast.walk(tree)
               if isinstance(node, ast.Constant) and isinstance(node.value, str)]
    if output not in strings or not (ROOT / output).is_file():
        errors.append(f'{script}: report output path mismatch')
    for text in strings:
        check_links(text, ROOT / output, script)

if errors:
    raise SystemExit('\n'.join(errors))
print(f'PASS: {len(paths)} Markdown files, {checked} local link/anchor checks, '
      f'{len(moves)} document moves, {len(reports)} report generators. No simulations run.')
