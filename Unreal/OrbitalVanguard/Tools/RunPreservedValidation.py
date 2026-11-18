"""기존 2A/D116 검사를 수정하지 않고 별도 보고서로 실행한다."""
import os,time,unreal
name=os.environ.get('OV_PRESERVED_SUITE','')
run=os.environ.get('OV_PRESERVED_RUN','')
if name not in ('2A','D116') or not run or not run.replace('_','').isalnum():raise ValueError('검사와 실행 ID 확인 필요')
folder=os.path.join(unreal.Paths.project_dir(),'Tools')
filename='ValidatePhase2A.py' if name=='2A' else 'ValidateD116Calibration.py'
with open(os.path.join(folder,filename),encoding='utf8') as stream: source=stream.read()
old='Phase2AControls.json' if name=='2A' else 'Phase2BRCalibration.json'
new=('Phase2AControls_' if name=='2A' else 'Phase2BRCalibration_')+run+'.json'
target=os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),'Validation',new)
if source.count(old)!=1 or os.path.exists(target):raise RuntimeError('기존 보고서 보호 검사 실패')
source=source.replace(old,new)
if name=='D116':
    needle='exec(compile(stream.read(), "ValidateRevisionOrders.py", "exec"), globals())'
    if source.count(needle)!=1:raise RuntimeError('중첩 보고서 경로 확인 실패')
    source=source.replace(needle,'exec(compile(stream.read().replace("Phase2BRCommands.json", "Phase2BRCommands_'+run+'.json"), "ValidateRevisionOrders.py", "exec"), globals())')
exec(compile(source,filename,'exec'),globals())
started=time.monotonic()
def finish(dt):
    if globals().get('report',{}).get('passed') is not None or time.monotonic()-started>360:
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(finish)
