"""Optional unattended wrapper. Original validators still leave PIE open when run directly."""
import os, unreal, time
_validator = os.environ.get('OV_VALIDATOR', 'ValidatePhase2C.py')
_tool_dir = os.path.join(unreal.Paths.project_dir(), 'Tools')
with open(os.path.join(_tool_dir, _validator), encoding='utf-8') as _input:
    exec(compile(_input.read(), _validator, 'exec'), globals())
_started_wrapper = time.monotonic()
def _finish_wrapper(dt):
    if globals().get('report', {}).get('passed') is not None or time.monotonic()-_started_wrapper > 360:
        unreal.unregister_slate_post_tick_callback(_wrapper_handle)
        unreal.SystemLibrary.quit_editor()
_wrapper_handle = unreal.register_slate_post_tick_callback(_finish_wrapper)
