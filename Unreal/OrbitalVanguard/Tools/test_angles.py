import math

CamPitch = -10.0
CamYaw = 21.0
CamRoll = 0.0

p = math.radians(CamPitch)
y = math.radians(CamYaw)

Fwd = (math.cos(p)*math.cos(y), math.cos(p)*math.sin(y), math.sin(p))
Right = (-math.sin(y), math.cos(y), 0.0)
Up = (
    Fwd[1]*Right[2] - Fwd[2]*Right[1],
    Fwd[2]*Right[0] - Fwd[0]*Right[2],
    Fwd[0]*Right[1] - Fwd[1]*Right[0]
)

for yaw_val in [45, 55, 65, 75]:
    for pitch_val in [4, 8, 12, 16]:
        # Mesh +X axis from rotator (pitch, yaw, 0):
        # UE Rotator(P, Y, R):
        # +X axis = (cos(P)*cos(Y), cos(P)*sin(Y), sin(P))
        # Bow is -X axis = - (+X axis)
        pm = math.radians(pitch_val)
        ym = math.radians(yaw_val)
        bow = (-math.cos(pm)*math.cos(ym), -math.cos(pm)*math.sin(ym), -math.sin(pm))
        
        # Screen projection:
        dot_fwd = bow[0]*Fwd[0] + bow[1]*Fwd[1] + bow[2]*Fwd[2]
        dot_r = bow[0]*Right[0] + bow[1]*Right[1] + bow[2]*Right[2]
        dot_u = bow[0]*Up[0] + bow[1]*Up[1] + bow[2]*Up[2]
        
        # Screen angle in degrees (0 = right, 90 = up, 180 = left, 270 = down)
        # Screen X = dot_r, Screen Y = dot_u
        angle_deg = math.degrees(math.atan2(dot_u, dot_r))
        print(f"Yaw={yaw_val:2d}, Pitch={pitch_val:2d} -> Screen(R={dot_r:+.2f}, U={dot_u:+.2f}, Fwd={dot_fwd:+.2f}) Angle={angle_deg:.1f}°")
