import math

w = 1280.0
h = 720.0
fov_h = math.radians(42.0)
tan_half_h = math.tan(fov_h / 2.0)
tan_half_v = tan_half_h / (16.0 / 9.0)

fwd = 3800.0
right = 1180.0
up = -1450.0
radius = 38.0 * 50.0 # 1900.0

dist = math.sqrt(fwd**2 + right**2 + up**2)
ang_r = math.asin(radius / dist)

ndc_x = (right / fwd) / tan_half_h
ndc_y = (up / fwd) / tan_half_v
cx = (ndc_x + 1.0) * 0.5 * w
cy = (1.0 - ndc_y) * 0.5 * h

r_px = (radius / dist) / math.sin(fov_h / 2.0) * (w / 2.0) # approximate projected radius

print(f"Center pixel: ({cx:.1f}, {cy:.1f}), Radius px: {r_px:.1f}")
# Find intersections with screen borders:
# Bottom border: y = 720 -> (x - cx)^2 + (720 - cy)^2 = r_px^2
dy_bot = 720.0 - cy
if abs(dy_bot) < r_px:
    dx_bot = math.sqrt(r_px**2 - dy_bot**2)
    print(f"Bottom edge intersection: x = {cx - dx_bot:.1f} and x = {cx + dx_bot:.1f} (screen is 0..1280)")
else:
    print("Does not intersect bottom edge")

# Right border: x = 1280 -> (1280 - cx)^2 + (y - cy)^2 = r_px^2
dx_right = 1280.0 - cx
if abs(dx_right) < r_px:
    dy_right = math.sqrt(r_px**2 - dx_right**2)
    print(f"Right edge intersection: y = {cy - dy_right:.1f} and y = {cy + dy_right:.1f} (screen is 0..720)")
else:
    print("Does not intersect right edge")

# Apex: (cx - nx * r_px, cy - ny * r_px) along ray to (0,0):
dist_center = math.hypot(cx, cy)
apex_x = cx - (cx / dist_center) * r_px
apex_y = cy - (cy / dist_center) * r_px
print(f"Apex (closest to screen center): ({apex_x:.1f}, {apex_y:.1f})")
