import math

# Camera frustum at D=6000:
H_half = 6000 * math.tan(math.radians(21)) # 2303.2
V_half = 6000 * math.tan(math.radians(21)) * (9/16) # 1295.5

print(f"Frustum at D=6000: H_half={H_half:.1f}, V_half={V_half:.1f}")

# We want planet circle to pass through:
# Point A: bottom edge, say Right = +400 (screen 59%), Up = -1295 (screen 100%)
# Point B: right edge, say Right = +2303 (screen 100%), Up = +200 (screen 42%)
# Let's find Center (R_c, U_c) and Radius R:

# Let's test several (R_c, U_c, Radius) configurations:
for r_c in [2800, 3200, 3600]:
    for u_c in [-2400, -2800, -3200]:
        for rad in [3800, 4200, 4600]:
            # Check if bottom-left (0, -1295) is outside:
            d_bl = math.sqrt((0 - r_c)**2 + (-1295 - u_c)**2)
            # Check where limb crosses bottom edge (Up = -1295):
            # (R - r_c)^2 + (-1295 - u_c)^2 = rad^2
            dy = -1295 - u_c
            if rad**2 > dy**2:
                r_cross_bottom = r_c - math.sqrt(rad**2 - dy**2)
            else:
                r_cross_bottom = -9999
                
            # Check where limb crosses right edge (Right = 2303):
            dx = 2303 - r_c
            if rad**2 > dx**2:
                u_cross_right = u_c + math.sqrt(rad**2 - dx**2)
            else:
                u_cross_right = -9999
                
            # Screen fraction:
            # X_frac = 0.5 + 0.5 * (R / H_half)
            # Y_frac = 0.5 - 0.5 * (U / V_half)
            if 0 < r_cross_bottom < H_half and -V_half < u_cross_right < V_half:
                x_frac = 0.5 + 0.5 * (r_cross_bottom / H_half)
                y_frac = 0.5 - 0.5 * (u_cross_right / V_half)
                print(f"R_c={r_c}, U_c={u_c}, Radius={rad} (scale {rad/50:.0f}x): Crosses bottom at X={x_frac*100:.1f}%, Crosses right at Y={y_frac*100:.1f}%")
