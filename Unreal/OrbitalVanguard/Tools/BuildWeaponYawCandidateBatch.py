"""9종 모두의 단면 기반 회전 분리 후보를 순차 생성한다.

최소 단면은 시험용 추정값이다. 자동으로 게임에 반영하거나 승인하지 않는다.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

project=Path(__file__).resolve().parents[1]
root=Path.home()/'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponFullSources_v1'
destination=Path.home()/'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponYawCandidates_v1'
parser=argparse.ArgumentParser()
parser.add_argument('--resume',action='store_true',help='완료 후보는 보존하고 미완료 후보만 다시 시도')
args=parser.parse_args()
if destination.exists() and not args.resume:raise FileExistsError(destination)
source_report=json.loads((root/'report.json').read_text(encoding='utf8'))
slices=json.loads((project/'Saved/Validation/WeaponSourceSlices.json').read_text(encoding='utf8'))
source_by_id={m['id']:m for m in source_report['models']}
slice_by_id={m['id']:m for m in slices['models']}
if len(source_by_id)!=9 or set(source_by_id)!=set(slice_by_id):raise RuntimeError('원본과 단면 9종이 일치하지 않습니다')
blender=Path(r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe')
if not blender.is_file():raise FileNotFoundError(blender)
destination.mkdir(parents=True,exist_ok=True)
report_file=destination/('batch_report_resume.json' if args.resume else 'batch_report.json')
if report_file.exists():raise FileExistsError(report_file)
rows=[]
for ident in sorted(source_by_id):
    model=source_by_id[ident];section=slice_by_id[ident];path=Path(model['prepared_fbx'])
    with path.open('rb') as stream:digest=hashlib.file_digest(stream,'sha256').hexdigest()
    if digest!=model['prepared_sha256'] or digest!=section['source_sha256']:raise RuntimeError('SOURCE HASH '+ident)
    bins=[x for x in section['slices'][20:41] if x['count']>=500 and x['min_xy'] and x['max_xy']]
    if not bins:raise RuntimeError('NO NECK CANDIDATE '+ident)
    chosen=min(bins,key=lambda x:(x['max_xy'][0]-x['min_xy'][0])*(x['max_xy'][1]-x['min_xy'][1]))
    cut=(chosen['z_min']+chosen['z_max'])/2
    center=[(chosen['min_xy'][i]+chosen['max_xy'][i])/2 for i in range(2)]
    valid=[x for x in section['slices'] if x['count'] and x['min_xy'] and x['max_xy']]
    low=[min(x['min_xy'][i] for x in valid) for i in range(2)]+[section['z_min']]
    high=[max(x['max_xy'][i] for x in valid) for i in range(2)]+[section['z_max']]
    evidence=destination/(ident+'_evidence.json')
    evidence_data={'scope':'회전 분리 후보용 단면 추정. 관절 확정 아님.','source_unchanged':True,
        'source_sha256_before':digest,'bounds_min':low,'bounds_max':high,'chosen_slice':chosen}
    if evidence.exists():
        if json.loads(evidence.read_text(encoding='utf8'))!=evidence_data:raise RuntimeError('EVIDENCE CHANGED '+ident)
    else:evidence.write_text(json.dumps(evidence_data,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    target=destination/ident;log=destination/(ident+'.log')
    if target.exists():
        report=target/'report.json'
        if not report.is_file():raise RuntimeError('PARTIAL OUTPUT; preserve and inspect '+str(target))
        old=json.loads(report.read_text(encoding='utf8'))
        if old.get('source_before')!=digest or old.get('source_after')!=digest:raise RuntimeError('OUTPUT HASH '+ident)
        rows.append({'id':ident,'source_sha256':digest,'cut_z':cut,'pivot_xy':center,'returncode':0,'report_present':True,'resumed':True})
        print('OV_YAW_CANDIDATE',ident,'preserved',flush=True)
        continue
    command=[str(blender),'--background','--factory-startup','--python',str(project/'Tools/PrepareTurretYawTrial.py'),'--',
        '--source',str(path),'--output',str(target),'--evidence',str(evidence),'--name',ident,
        '--cut-z='+str(cut),'--pivot-x='+str(center[0]),'--pivot-y='+str(center[1])]
    if log.exists():raise RuntimeError('EXISTING LOG; preserve and inspect '+str(log))
    with log.open('w',encoding='utf8') as stream:
        run=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,check=False)
    row={'id':ident,'source_sha256':digest,'cut_z':cut,'pivot_xy':center,'returncode':run.returncode,
         'report_present':(target/'report.json').is_file(),'log':str(log)}
    rows.append(row);report_file.write_text(json.dumps({'scope':'미승인 회전 분리 후보.',
        'models':rows},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('OV_YAW_CANDIDATE',ident,run.returncode,row['report_present'],flush=True)

report_file.write_text(json.dumps({'scope':'미승인 회전 분리 후보.', 'models':rows},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
