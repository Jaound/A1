import math

# Camera
cam_loc = (-3400.0, -1500.0, 760.0)
cam_rot_p = -10.0
cam_rot_y = 21.0
w = 1280.0
h = 720.0
fov_h = math.radians(42.0)
tan_half_h = math.tan(fov_h / 2.0)
tan_half_v = tan_half_h / (16.0 / 9.0)

# Camera basis vectors
# Pitch -10, Yaw 21
cp = math.cos(math.radians(cam_rot_p))
sp = math.sin(math.radians(cam_rot_p))
cy = math.cos(math.radians(cam_rot_y))
sy = math.sin(math.radians(cam_rot_y))

# CamForward = (cp*cy, cp*sy, sp)
cfwd = (cp*cy, cp*sy, sp)
# CamRight = (-sy, cy, 0)
cright = (-sy, cy, 0.0)
# CamUp = cright x cfwd = (-cy*sp, -sy*sp, cp)
cup = (-cy*sp, -sy*sp, cp)

def world_to_screen(loc):
    dx = loc[0] - cam_loc[0]
    dy = loc[1] - cam_loc[1]
    dz = loc[2] - cam_loc[2]
    
    fwd = dx * cfwd[0] + dy * cfwd[1] + dz * cfwd[2]
    right = dx * cright[0] + dy * cright[1] + dz * cright[2]
    up = dx * cup[0] + dy * cup[1] + dz * cup[2]
    
    if fwd <= 0:
        return None
    ndc_x = (right / fwd) / tan_half_h
    ndc_y = (up / fwd) / tan_half_v
    px = (ndc_x + 1.0) * 0.5 * w
    py = (1.0 - ndc_y) * 0.5 * h
    return px, py, fwd

# Current ships:
ships = [
    ("Flagship (Dreadnought)", (-1522.4, -158.0, 183.3), 3.6),
    ("Battlecarrier", (34.3, 54.0, 218.3), 2.8),
    ("Battleship", (-597.3, 561.3, 209.0), 2.5),
    ("Carrier", (886.5, 659.6, 202.2), 2.4),
    ("Cruiser", (-699.7, -35.0, 182.3), 1.9),
    ("Bastion", (78.5, 1067.1, 207.9), 1.8),
    ("Vector", (627.7, 774.5, 217.3), 1.7),
    ("AirDefense", (221.3, 232.8, 419.9), 1.6),
    ("Frigate1", (-973.2, 267.0, -76.1), 1.4),
    ("Frigate2", (944.3, 1078.1, 206.9), 1.3),
    ("Fighter1", (-1834.2, -406.2, 331.6), 1.0),
    ("Fighter2", (-1712.9, -295.4, 335.1), 0.9),
    ("Fighter3", (-1679.7, -411.2, 276.0), 0.9),
]

print("Current ships screen projection:")
for name, loc, scale in ships:
    res = world_to_screen(loc)
    if res:
        px, py, fwd = res
        # approximate screen size in px (hull length ~ 600 * scale)
        app_size = (600.0 * scale / fwd) / tan_half_h * (w / 2.0)
        print(f"  {name:25s}: px=({px:6.1f}, {py:6.1f}), dist={fwd:6.1f}, size={app_size:5.1f}px")
    else:
        print(f"  {name:25s}: BEHIND CAMERA")
