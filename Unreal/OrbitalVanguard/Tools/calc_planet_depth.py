import math

w = 1280.0
h = 720.0
fov_h = math.radians(42.0)
tan_half_h = math.tan(fov_h / 2.0)
tan_half_v = tan_half_h / (16.0 / 9.0)

# We want the circle on screen to have:
# Center: cx = 1160, cy = 1000
# Radius: r_px = 800
# At any forward distance `fwd`, perspective projection gives:
# cx = (ndc_x + 1) * 0.5 * w -> ndc_x = (cx / 640) - 1
# cy = (1 - ndc_y) * 0.5 * h -> ndc_y = 1 - (cy / 360)
# Right = ndc_x * tan_half_h * fwd
# Up = ndc_y * tan_half_v * fwd
# Radius_3D = (r_px / 640) * tan_half_h * fwd
# Scale = Radius_3D / 50.0

for fwd in [6000.0, 7000.0, 8000.0, 9000.0]:
    cx = 1160.0
    cy = 1000.0
    r_px = 800.0
    
    ndc_x = (cx / (w * 0.5)) - 1.0
    ndc_y = 1.0 - (cy / (h * 0.5))
    
    right = ndc_x * tan_half_h * fwd
    up = ndc_y * tan_half_v * fwd
    radius_3d = (r_px / (w * 0.5)) * tan_half_h * fwd
    scale = radius_3d / 50.0
    
    front_dist = fwd - radius_3d
    
    print(f"At fwd={fwd:.0f}: Right={right:.1f}, Up={up:.1f}, Radius3D={radius_3d:.1f}, Scale={scale:.1f}, FrontPlaneDist={front_dist:.1f}")
