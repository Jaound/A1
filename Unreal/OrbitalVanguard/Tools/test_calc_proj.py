import math

# Camera properties
cam_fov_h_deg = 42.0
cam_fov_h = math.radians(cam_fov_h_deg)
aspect = 16.0 / 9.0
# In Unreal Engine UCameraComponent:
# If AspectRatioAxisConstraint is AspectRatio_MaintainYFOV (common in vertical FOV locked games):
# Let's check both MaintainXFOV and MaintainYFOV
tan_half_h = math.tan(cam_fov_h / 2.0)
tan_half_v_from_h = tan_half_h / aspect

# What if 42 deg is vertical FOV?
tan_half_v = math.tan(math.radians(21.0))
tan_half_h_from_v = tan_half_v * aspect

for mode, th, tv in [("MaintainXFOV (42 deg H)", tan_half_h, tan_half_v_from_h),
                     ("MaintainYFOV (42 deg V)", tan_half_h_from_v, tan_half_v)]:
    print(f"--- Mode: {mode} ---")
    # For a range of planet positions:
    # Let's see where (fwd, right, up) projects to pixel coords on 1280x720:
    # Pixel coords: (0,0) top-left, (1280, 720) bottom-right
    # NDC: X in [-1, 1] (left to right), Y in [-1, 1] (bottom to top)
    # Screen X = (NDC_X + 1) * 0.5 * 1280
    # Screen Y = (1 - NDC_Y) * 0.5 * 720
    
    # Current code:
    fwd = 6000.0
    right = 3600.0
    up = -3200.0
    radius = 3800.0 # 76 * 50
    
    ndc_x = (right / fwd) / th
    ndc_y = (up / fwd) / tv
    px = (ndc_x + 1.0) * 0.5 * 1280.0
    py = (1.0 - ndc_y) * 0.5 * 720.0
    
    # Apparent radius in pixels:
    # Along horizontal:
    r_px_h = (radius / fwd) / th * 0.5 * 1280.0
    r_px_v = (radius / fwd) / tv * 0.5 * 720.0
    
    print(f"Current setting:")
    print(f"  Center: NDC=({ndc_x:.2f}, {ndc_y:.2f}) -> Pixels=({px:.1f}, {py:.1f})")
    print(f"  Radius pixels: H={r_px_h:.1f}, V={r_px_v:.1f}")
    # Distance from center to (1280, 720):
    dist_corner = math.sqrt((px - 1280)**2 + (py - 720)**2)
    print(f"  Distance to bottom-right corner (1280, 720): {dist_corner:.1f} px vs radius {r_px_h:.1f} px")
    print(f"  Overlaps screen? {dist_corner < r_px_h}")
