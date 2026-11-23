import math

# Screen: 1280 x 720
# Camera: FOV = 42 deg (Horizontal)
# 16:9 -> Aspect = 1.77777778
w = 1280.0
h = 720.0
fov_h = math.radians(42.0)
tan_half_h = math.tan(fov_h / 2.0) # ~0.383864
tan_half_v = tan_half_h / (16.0 / 9.0) # ~0.2159235

def project_point(fwd, right, up):
    if fwd <= 0:
        return None, None
    ndc_x = (right / fwd) / tan_half_h
    ndc_y = (up / fwd) / tan_half_v
    px = (ndc_x + 1.0) * 0.5 * w
    py = (1.0 - ndc_y) * 0.5 * h
    return px, py

# For a sphere of radius R at (fwd, right, up)
# In camera space, center is C = (fwd, right, up).
# We want the sphere to appear in the lower-right of the screen.
# Reference wireframe (media_1795099219552_61a8f429.jpg):
# - Left edge of Earth limb enters screen at bottom: x ~ 400px (out of 1280), y = 720px
# - Earth limb curves up and exits right edge: x = 1280px, y ~ 200px ~ 300px
# - Top-left apex of the curve reaches around (x ~ 800px, y ~ 350px)

# Let's test candidate positions:
# Suppose fwd = 3500.
# Apparent angular radius alpha: sin(alpha) = R / dist.
# In perspective projection, the limb of the sphere projects to an ellipse on screen.
# For simplicity, if dist is large compared to R, projected radius in pixels is approximately:
# r_px_x = (R / dist) / tan_half_h * (w / 2)
# r_px_y = (R / dist) / tan_half_v * (h / 2) = r_px_x (since (h/2)/tan_v == (w/2)/tan_h)
# So projected radius is circularly symmetric in pixels!

# If projected radius is R_px, and center is at (cx, cy):
# We want the circle (x - cx)^2 + (y - cy)^2 = R_px^2 to pass through:
# Point A: bottom edge (x = 450, y = 720)
# Point B: right edge (x = 1280, y = 280)
# Let's find the circle passing through A and B with apex around (850, 420)!

xA, yA = 450.0, 720.0
xB, yB = 1280.0, 260.0
# Midpoint of AB:
xM, yM = (xA + xB) / 2.0, (yA + yB) / 2.0 # (865, 490)
# Normal to chord AB:
dx = xB - xA # 830
dy = yB - yA # -460
chord_len = math.hypot(dx, dy)
# Unit normal pointing down-right (away from apex):
nx = -dy / chord_len # 460 / 949 = 0.4847
ny = dx / chord_len # 830 / 949 = 0.8746

for sagitta in [120, 160, 200, 250, 300]:
    # Circle with sagitta s:
    # R = (chord_len^2 / (8*s)) + s/2
    R_px = (chord_len**2) / (8.0 * sagitta) + sagitta / 2.0
    # Center distance from midpoint: d = R - s
    d = R_px - sagitta
    cx = xM + nx * d
    cy = yM + ny * d
    
    # Check apex (closest point on circle to top-left):
    apex_x = xM - nx * sagitta
    apex_y = yM - ny * sagitta
    
    # Calculate 3D camera coords for fwd = 4000:
    fwd = 4000.0
    # px = (ndc_x + 1) * 0.5 * w -> ndc_x = (px / (0.5 * w)) - 1
    ndc_x = (cx / (0.5 * w)) - 1.0
    ndc_y = 1.0 - (cy / (0.5 * h))
    right = ndc_x * tan_half_h * fwd
    up = ndc_y * tan_half_v * fwd
    
    # Radius in 3D:
    # R_px = (R_3d / fwd) / tan_half_h * (w / 2)
    R_3d = (R_px / (0.5 * w)) * tan_half_h * fwd
    scale = R_3d / 50.0 # since SM_Earth_Planet radius = 50
    
    print(f"Sagitta {sagitta}: R_px={R_px:.0f}, Center_px=({cx:.0f}, {cy:.0f}), Apex=({apex_x:.0f}, {apex_y:.0f})")
    print(f"  -> At fwd={fwd}: Right={right:.1f}, Up={up:.1f}, Radius3D={R_3d:.1f}, Scale={scale:.1f}")
