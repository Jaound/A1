import math

# Camera specs
CamLoc = (-3400.0, -1500.0, 760.0)
CamPitch = -10.0
CamYaw = 21.0
CamRoll = 0.0

p = math.radians(CamPitch)
y = math.radians(CamYaw)

# True UE FRotationMatrix axes
Fwd = (math.cos(p)*math.cos(y), math.cos(p)*math.sin(y), math.sin(p))
Right = (-math.sin(y), math.cos(y), 0.0)
Up = (
    Fwd[1]*Right[2] - Fwd[2]*Right[1],
    Fwd[2]*Right[0] - Fwd[0]*Right[2],
    Fwd[0]*Right[1] - Fwd[1]*Right[0]
)

print(f"Cam Fwd: ({Fwd[0]:.3f}, {Fwd[1]:.3f}, {Fwd[2]:.3f})")
print(f"Cam Right: ({Right[0]:.3f}, {Right[1]:.3f}, {Right[2]:.3f})")
print(f"Cam Up: ({Up[0]:.3f}, {Up[1]:.3f}, {Up[2]:.3f})")

# Desired flight direction in camera space:
# Flying towards camera (negative Fwd), towards screen left (negative Right), slightly down (negative Up)
# Like Reference 01: ships flying from top-right background towards bottom-left foreground.
v_cam_fwd = -0.75
v_cam_r = -0.55
v_cam_u = -0.25

# Normalize
mag = math.sqrt(v_cam_fwd**2 + v_cam_r**2 + v_cam_u**2)
v_cam_fwd /= mag
v_cam_r /= mag
v_cam_u /= mag

# Flight vector in world coordinates:
V_fly = (
    v_cam_fwd * Fwd[0] + v_cam_r * Right[0] + v_cam_u * Up[0],
    v_cam_fwd * Fwd[1] + v_cam_r * Right[1] + v_cam_u * Up[1],
    v_cam_fwd * Fwd[2] + v_cam_r * Right[2] + v_cam_u * Up[2]
)

print(f"V_fly (Bow heading): ({V_fly[0]:.3f}, {V_fly[1]:.3f}, {V_fly[2]:.3f})")

# In mesh local space:
# Bow is at -X: (-1, 0, 0).
# So the mesh's +X axis must point along -V_fly!
V_mesh_x = (-V_fly[0], -V_fly[1], -V_fly[2])

# Pitch and Yaw of +X axis:
# In Unreal: +X axis vector is (cos(pitch)*cos(yaw), cos(pitch)*sin(yaw), sin(pitch))
mesh_pitch = math.degrees(math.asin(V_mesh_x[2]))
mesh_yaw = math.degrees(math.atan2(V_mesh_x[1], V_mesh_x[0]))

print(f"Calculated Mesh Rotation: Pitch={mesh_pitch:.1f}, Yaw={mesh_yaw:.1f}")

# Let's verify Bow direction from this rotator:
p_m = math.radians(mesh_pitch)
y_m = math.radians(mesh_yaw)
mesh_fwd = (math.cos(p_m)*math.cos(y_m), math.cos(p_m)*math.sin(y_m), math.sin(p_m))
bow_dir = (-mesh_fwd[0], -mesh_fwd[1], -mesh_fwd[2])

print(f"Actual Bow Dir: ({bow_dir[0]:.3f}, {bow_dir[1]:.3f}, {bow_dir[2]:.3f})")

# Projected on Camera screen axes:
dot_cam_fwd = bow_dir[0]*Fwd[0] + bow_dir[1]*Fwd[1] + bow_dir[2]*Fwd[2]
dot_cam_r = bow_dir[0]*Right[0] + bow_dir[1]*Right[1] + bow_dir[2]*Right[2]
dot_cam_u = bow_dir[0]*Up[0] + bow_dir[1]*Up[1] + bow_dir[2]*Up[2]

print(f"Bow in Camera Space: Fwd={dot_cam_fwd:.3f} (toward viewer if < 0), Right={dot_cam_r:.3f} (left if < 0), Up={dot_cam_u:.3f} (down if < 0)")
