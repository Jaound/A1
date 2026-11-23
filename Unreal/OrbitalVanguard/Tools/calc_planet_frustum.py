import math

# Camera FOV 42 deg
fov_h = 42.0 * math.pi / 180.0
# In Unreal, if FOV is horizontal:
half_fov_h = fov_h / 2.0
aspect = 16.0 / 9.0
half_fov_v = math.atan(math.tan(half_fov_h) / aspect)

print(f"Half FOV H: {math.degrees(half_fov_h):.2f} deg, Half FOV V: {math.degrees(half_fov_v):.2f} deg")

# Planet center relative to camera:
# Fwd = 6000, Right = 3600, Up = -3200
fwd = 6000.0
right = 3600.0
up = -3200.0
radius = 3800.0 # 76 * 50

# Center projected to normalized screen coords [-1, 1]:
screen_x = (right / fwd) / math.tan(half_fov_h)
screen_y = (up / fwd) / math.tan(half_fov_v)
print(f"Center on screen: X = {screen_x:.2f}, Y = {screen_y:.2f} (screen is [-1, 1])")

# Angular distance from center
dist = math.sqrt(fwd**2 + right**2 + up**2)
ang_center_r = math.atan2(right, fwd)
ang_center_u = math.atan2(up, fwd)
ang_radius = math.asin(radius / dist)

print(f"Distance = {dist:.1f}, Angular radius = {math.degrees(ang_radius):.2f} deg")
print(f"Right angle = {math.degrees(ang_center_r):.2f} deg (FOV edge = {math.degrees(half_fov_h):.2f} deg)")
print(f"Up angle = {math.degrees(ang_center_u):.2f} deg (FOV edge = -{math.degrees(half_fov_v):.2f} deg)")

# Closest point of sphere to camera view axis:
# Does the sphere intersect the frustum?
# Angle from camera forward vector (0, 0, 1):
cos_ang = fwd / dist
ang_to_axis = math.acos(cos_ang)
print(f"Angle from camera forward axis: {math.degrees(ang_to_axis):.2f} deg")
print(f"Sphere covers angles from {math.degrees(ang_to_axis - ang_radius):.2f} to {math.degrees(ang_to_axis + ang_radius):.2f} deg")
