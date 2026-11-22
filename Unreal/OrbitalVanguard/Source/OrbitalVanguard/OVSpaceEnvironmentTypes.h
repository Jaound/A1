#pragma once

#include "CoreMinimal.h"
#include "OVSpaceEnvironmentTypes.generated.h"

/**
 * Unified space-environment hazard types matching work:docs/02_balance/SPACE_ENVIRONMENT_BALANCE.md (E1 v0.1).
 * Extensible across Black Hole, Solar Flare, Radiation Zone, and Destructible Asteroid Field.
 */
UENUM(BlueprintType)
enum class EOVSpaceHazardKind : uint8
{
    None,
    BlackHole,
    SolarFlare,
    RadiationZone,
    AsteroidField
};

/**
 * Common E1 numerical configuration shared by space-environment hazard actors.
 * All distance/speed/acceleration values use UE cm units (1 m = 100 cm).
 */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVSpaceEnvironmentProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    EOVSpaceHazardKind Kind = EOVSpaceHazardKind::None;

    /** Outer effect radius in cm (e.g. Black Hole 1,000 m = 100,000 cm; Radiation 850 m = 85,000 cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    float InfluenceRadiusCm = 100000.0f;

    /** Inner critical radius in cm (e.g. Black Hole singularity 45 m = 4,500 cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    float CoreHazardRadiusCm = 4500.0f;

    /** Maximum gravitational acceleration in cm/s^2 (Black Hole E1: 0.8 m/s^2 = 80 cm/s^2). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    float MaxAccelerationCmPerSec2 = 80.0f;

    /** Normalized E1 base damage fraction per second or per wave (Radiation 0.006*B/s, Flare 0.35*B/wave). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    float NormalizedDamageFactor = 0.0f;

    /** Wave interval in seconds (Solar Flare E1: 210 s after initial 45 s warning). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    float WavePeriodSeconds = 210.0f;

    /** Wave travel speed in cm/s (Solar Flare E1: 1,200 m/s = 120,000 cm/s). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    float WaveSpeedCmPerSec = 120000.0f;

    /** When true, crossing CoreHazardRadiusCm wrecks a vessel immediately regardless of HP/Shield. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    bool bInstantWreckInCore = false;

    /** When true, ships charging FTL remain affected by external displacement and core destruction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpaceEnvironment")
    bool bAffectsFTLCharging = true;

    static FOVSpaceEnvironmentProfile MakeBlackHoleE1Profile()
    {
        FOVSpaceEnvironmentProfile P;
        P.Kind = EOVSpaceHazardKind::BlackHole;
        P.InfluenceRadiusCm = 100000.0f;      // 1,000 m
        P.CoreHazardRadiusCm = 4500.0f;       // 45 m
        P.MaxAccelerationCmPerSec2 = 80.0f;   // 0.8 m/s^2
        P.bInstantWreckInCore = true;
        P.bAffectsFTLCharging = true;
        return P;
    }

    static FOVSpaceEnvironmentProfile MakeRadiationE1Profile()
    {
        FOVSpaceEnvironmentProfile P;
        P.Kind = EOVSpaceHazardKind::RadiationZone;
        P.InfluenceRadiusCm = 85000.0f;       // 850 m
        P.CoreHazardRadiusCm = 0.0f;
        P.MaxAccelerationCmPerSec2 = 0.0f;
        P.NormalizedDamageFactor = 0.006f;    // 0.006 * B / sec
        P.bInstantWreckInCore = false;
        P.bAffectsFTLCharging = true;
        return P;
    }

    static FOVSpaceEnvironmentProfile MakeSolarFlareE1Profile()
    {
        FOVSpaceEnvironmentProfile P;
        P.Kind = EOVSpaceHazardKind::SolarFlare;
        P.InfluenceRadiusCm = 0.0f;
        P.CoreHazardRadiusCm = 6000.0f;       // 60 m wave thickness
        P.MaxAccelerationCmPerSec2 = 0.0f;
        P.NormalizedDamageFactor = 0.35f;     // 0.35 * B / wave
        P.WavePeriodSeconds = 210.0f;
        P.WaveSpeedCmPerSec = 120000.0f;      // 1,200 m/s
        P.bInstantWreckInCore = false;
        P.bAffectsFTLCharging = true;
        return P;
    }

    static FOVSpaceEnvironmentProfile MakeAsteroidFieldE1Profile()
    {
        FOVSpaceEnvironmentProfile P;
        P.Kind = EOVSpaceHazardKind::AsteroidField;
        P.InfluenceRadiusCm = 8500.0f;        // up to 85 m asteroid radius
        P.CoreHazardRadiusCm = 5000.0f;       // 50 m reference radius (1,800 HP)
        P.MaxAccelerationCmPerSec2 = 0.0f;
        P.NormalizedDamageFactor = 0.006f;    // collision input scale
        P.bInstantWreckInCore = false;
        P.bAffectsFTLCharging = true;
        return P;
    }

    /**
     * Exact E1 Black Hole gravity attenuation:
     * a(r) = MaxAccel * [1 - max(0, r - CoreRadius) / (InfluenceRadius - CoreRadius)]^2, or 0 for r >= InfluenceRadius.
     */
    static float ComputeBlackHoleGravityAccelerationCm(
        float DistanceCm,
        float CoreRadiusCm = 4500.0f,
        float InfluenceRadiusCm = 100000.0f,
        float MaxAccelCmPerSec2 = 80.0f)
    {
        if (!FMath::IsFinite(DistanceCm) || DistanceCm >= InfluenceRadiusCm || InfluenceRadiusCm <= CoreRadiusCm)
        {
            return 0.0f;
        }
        const float Span = FMath::Max(1.0f, InfluenceRadiusCm - CoreRadiusCm);
        const float Falloff = FMath::Clamp(1.0f - FMath::Max(0.0f, DistanceCm - CoreRadiusCm) / Span, 0.0f, 1.0f);
        return MaxAccelCmPerSec2 * Falloff * Falloff;
    }
};
