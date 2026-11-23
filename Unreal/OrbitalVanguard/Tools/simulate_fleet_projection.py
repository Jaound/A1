import math

# Camera
CamLoc = (-3400.0, -1500.0, 760.0)
CamPitch = -10.0
CamYaw = 21.0
p = math.radians(CamPitch)
y = math.radians(CamYaw)

CamFwd = (math.cos(p)*math.cos(y), math.cos(p)*math.sin(y), math.sin(p))
CamRight = (-math.sin(y), math.cos(y), 0.0)
CamUp = (
    CamFwd[1]*CamRight[2] - CamFwd[2]*CamRight[1],
    CamFwd[2]*CamRight[0] - CamFwd[0]*CamRight[2],
    CamFwd[0]*CamRight[1] - CamFwd[1]*CamRight[0]
)

CameraFov = 48.0
fov_x = math.radians(CameraFov)
aspect = 16.0 / 9.0
fov_y = 2.0 * math.atan(math.tan(fov_x/2.0) / aspect)
tan_half_x = math.tan(fov_x / 2.0)
tan_half_y = math.tan(fov_y / 2.0)

def world_to_screen(world_pt):
    # Vector from camera to point
    rel = (world_pt[0] - CamLoc[0], world_pt[1] - CamLoc[1], world_pt[2] - CamLoc[2])
    fwd_dist = rel[0]*CamFwd[0] + rel[1]*CamFwd[1] + rel[2]*CamFwd[2]
    if fwd_dist <= 0:
        return None
    r_dist = rel[0]*CamRight[0] + rel[1]*CamRight[1] + rel[2]*CamRight[2]
    u_dist = rel[0]*CamUp[0] + rel[1]*CamUp[1] + rel[2]*CamUp[2]
    
    ndc_x = r_dist / (fwd_dist * tan_half_x)
    ndc_y = u_dist / (fwd_dist * tan_half_y)
    
    screen_x = (ndc_x * 0.5) + 0.5
    screen_y = 0.5 - (ndc_y * 0.5)
    return screen_x * 1920.0, screen_y * 1080.0, fwd_dist

def get_ship_endpoints(center, pitch, yaw, roll, length, scale):
    rad_p = math.radians(pitch)
    rad_y = math.radians(yaw)
    rad_r = math.radians(roll)
    cp, sp = math.cos(rad_p), math.sin(rad_p)
    cy, sy = math.cos(rad_y), math.sin(rad_y)
    # Bow is -X axis in local mesh coordinates
    bow_dir = (-cp*cy, -cp*sy, -sp)
    half_l = (length * scale) * 0.5
    bow_pt = (center[0] + bow_dir[0]*half_l, center[1] + bow_dir[1]*half_l, center[2] + bow_dir[2]*half_l)
    stern_pt = (center[0] - bow_dir[0]*half_l, center[1] - bow_dir[1]*half_l, center[2] - bow_dir[2]*half_l)
    return bow_pt, stern_pt

# Let's test the slots from layout_fleet_v2.py
slots = [
    ("Flagship", 350.0, 3.1, (-1404.3, -138.7, 272.2), (15.0, 82.0, -12.0)),
    ("Battlecarrier", 233.0, 2.6, (-339.2, -325.1, 349.7), (15.0, 82.0, -12.0)),
    ("Battleship", 350.0, 2.5, (-723.7, 660.5, 317.2), (15.0, 82.0, -12.0)),
    ("Carrier", 272.0, 2.4, (322.1, 569.7, 486.5), (15.0, 82.0, -12.0)),
    ("Cruiser1", 272.0, 2.2, (246.3, -390.3, 437.5), (15.0, 82.0, -12.0)),
    ("Cruiser2", 272.0, 2.1, (-52.9, 1005.7, 538.9), (15.0, 82.0, -12.0)),
    ("Bastion", 350.0, 1.9, (1068.8, 398.5, 636.8), (15.0, 82.0, -12.0)),
    ("Vector", 324.0, 1.9, (888.4, 1174.4, 682.6), (15.0, 82.0, -12.0)),
    ("AirDefense", 285.0, 1.9, (-422.2, 1090.0, 482.5), (15.0, 82.0, -12.0)),
    ("Frigate1", 324.0, 1.4, (-1362.6, 274.1, 180.3), (15.0, 82.0, -12.0)),
    ("Frigate2", 324.0, 1.4, (-778.4, -121.7, 165.1), (15.0, 82.0, -12.0)),
    ("Fighter1", 80.0, 0.75, (-1550.2, -629.7, 229.6), (16.0, 84.0, -14.0)),
    ("Fighter2", 80.0, 0.70, (-972.2, -667.2, 334.2), (16.0, 84.0, -14.0)),
    ("Fighter3", 80.0, 0.70, (-1526.5, 226.4, 366.9), (16.0, 84.0, -14.0)),
]

print("=== PROJECTED SHIP SCREEN SEGMENTS ===")
screen_segs = {}
for name, l, s, c, rot in slots:
    bow, stern = get_ship_endpoints(c, rot[0], rot[1], rot[2], l, s)
    s_c = world_to_screen(c)
    s_bow = world_to_screen(bow)
    s_stern = world_to_screen(stern)
    screen_segs[name] = (s_c, s_bow, s_stern, l*s)
    print(f"{name:14s}: Center=({s_c[0]:6.1f}, {s_c[1]:6.1f}), Bow=({s_bow[0]:6.1f}, {s_bow[1]:6.1f}), Stern=({s_stern[0]:6.1f}, {s_stern[1]:6.1f})")

# Segment-to-segment minimum distance
def dist_point_to_segment(p, a, b):
    px, py = p[0], p[1]
    ax, ay = a[0], a[1]
    bx, by = b[0], b[1]
    abx, aby = bx - ax, by - ay
    l2 = abx*abx + aby*aby
    if l2 == 0:
        return math.hypot(px-ax, py-ay)
    t = max(0.0, min(1.0, ((px-ax)*abx + (py-ay)*aby) / l2))
    projx = ax + t*abx
    projy = ay + t*aby
    return math.hypot(px-projx, py-projy)

def dist_segment_to_segment(a1, a2, b1, b2):
    return min(
        dist_point_to_segment(a1, b1, b2),
        dist_point_to_segment(a2, b1, b2),
        dist_point_to_segment(b1, a1, a2),
        dist_point_to_segment(b2, a1, a2),
    )

print("\n=== MINIMUM SEGMENT DISTANCE (HULL CLEARANCE IN PIXELS) ===")
min_clearance = 999999
closest = None
for i in range(len(slots)):
    for j in range(i+1, len(slots)):
        n1 = slots[i][0]
        n2 = slots[j][0]
        _, b1, s1, _ = screen_segs[n1]
        _, b2, s2, _ = screen_segs[n2]
        d = dist_segment_to_segment(b1, s1, b2, s2)
        if d < min_clearance:
            min_clearance = d
            closest = (n1, n2, d)
        if d < 50.0:
            print(f"WARNING: Close clearance between {n1} and {n2}: {d:.1f} px")

print(f"Overall Minimum Hull-to-Hull Clearance: {closest[0]} <-> {closest[1]} = {closest[2]:.1f} pixels")
