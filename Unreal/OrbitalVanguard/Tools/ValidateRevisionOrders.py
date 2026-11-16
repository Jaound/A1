"""R1 commands/protection API acceptance plus unchanged 2B50. No physical input."""
import unreal, os
base = os.path.join(unreal.Paths.project_dir(), 'Tools', 'ValidatePhase2B.py')
with open(base, encoding='utf-8') as stream:
    source = stream.read().replace("'Phase2B.json'", "'Phase2BRCommands.json'")
exec(compile(source, base, 'exec'), globals())
report['planning'] = '035a4fba81d26f1d67daa1f44bded0f7e8d3a434'
report['scope'] = 'R1 commands/protection plus original 2B regression'
original_common = run_common

def run_common(pc, world):
    pc.set_editor_property("developer_mode", True)
    pc.get_hud().set_details_open(True)
    combat, a, support, target = fixture(pc)
    systems = p(target, 'systems')
    pc.action('GunIntent')
    check('R1 intent selection never fires', p(combat, 'shot_count') == 0 and 'DISABLE' in combat.get_order_status(a))
    systems.set_interior_presence(1, True, False, True)
    check('R1 enemy residents are not own-faction protection', not combat.is_target_protected(a, target))
    pc.action('DevInterior')
    check('R1 DEV state is explicit and faction-specific', combat.is_target_protected(a, target) and systems.is_interior_developer_input(0))
    pc.action('Fire'); frames(pc, 90)
    check('R1 protection blocks real fire request', p(combat, 'shot_count') == 0)
    combat.set_protection_policy(a, unreal.OVProtectionPolicy.UNTIL_OBJECTIVE)
    pc.action('DevObjective')
    check('R1 completed objective releases goal policy', not combat.is_target_protected(a, target))
    combat.set_protection_policy(a, unreal.OVProtectionPolicy.UNTIL_EXIT)
    check('R1 exit policy retains completed residents', combat.is_target_protected(a, target))
    pc.action('ForceGun'); frames(pc, 180)
    check('R1 explicit forced gun uses actual fire', p(combat, 'shot_count') > 0 and 'RISK' in combat.get_order_status(a))
    pc.action('Stop')
    check('R1 different movement order revokes force', str(p(combat.get_ship_state(a), 'forced_mount')) == 'None')
    pc.action('CancelAttack')
    check('R1 cancel clears attack target', p(a, 'attack_target') is None)
    systems.set_interior_presence(0, False, False, True)
    pc.set_target(target)
    check('R1 interior exit releases protection', not combat.is_target_protected(a, target))
    original_common(pc, world)
