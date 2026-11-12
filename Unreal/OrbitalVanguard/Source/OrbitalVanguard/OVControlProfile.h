#pragma once
#include "CoreMinimal.h"

struct FOVPlayGame3Profile
{
    // Fit the existing test hulls at roughly their original on-screen size.
    static constexpr double UnitsPerReferenceUnit = 14.0;
    static constexpr float VerticalFov = 52.f;
    static constexpr float InitialDistance = 290.f * UnitsPerReferenceUnit;
    static constexpr float MaxDistance = 680.f * UnitsPerReferenceUnit;

    static float HorizontalFov(double Aspect)
    {
        return FMath::RadiansToDegrees(2.0 * FMath::Atan(
            FMath::Tan(FMath::DegreesToRadians(VerticalFov * 0.5)) * Aspect));
    }

    static double PanSpeed(double Distance)
    {
        const double ReferenceDistance = Distance / UnitsPerReferenceUnit;
        return 180.0 * UnitsPerReferenceUnit * FMath::Clamp(ReferenceDistance / 240.0, 0.45, 2.5);
    }
};

enum class EOVOrbitSurface : uint8 { None, World, Preview };

struct FOVOrbitGesture
{
    EOVOrbitSurface Surface = EOVOrbitSurface::None;
    bool bAltLeft = false;
    FVector2D Previous = FVector2D::ZeroVector;

    void Begin(EOVOrbitSurface Owner, bool AltLeft, FVector2D Position)
    {
        if (Surface != EOVOrbitSurface::None) return;
        Surface = Owner;
        bAltLeft = AltLeft;
        Previous = Position;
    }

    FVector2D Sample(FVector2D Position)
    {
        const FVector2D Delta = Position - Previous;
        Previous = Position;
        return Delta;
    }

    void End(bool AltLeft)
    {
        if (bAltLeft == AltLeft) *this = {};
    }
};

struct FOVRightMoveGesture
{
    bool bActive = false;
    bool bValidPoint = false;
    FVector2D Start = FVector2D::ZeroVector;
    FVector Anchor = FVector::ZeroVector;
    double MaxTravelSquared = 0;

    void Begin(FVector2D Position, FVector Point, bool ValidPoint)
    {
        *this = {};
        bActive = true;
        bValidPoint = ValidPoint;
        Start = Position;
        Anchor = Point;
    }

    FVector Sample(FVector2D Position)
    {
        MaxTravelSquared = FMath::Max(MaxTravelSquared, (Position - Start).SizeSquared());
        FVector Destination = Anchor;
        if (MaxTravelSquared >= FMath::Square(8.0))
        {
            const double Offset = (Start.Y - Position.Y) * 0.55 * FOVPlayGame3Profile::UnitsPerReferenceUnit;
            constexpr double Limit = 180 * FOVPlayGame3Profile::UnitsPerReferenceUnit;
            Destination.Z = FMath::Clamp(Anchor.Z + FMath::Clamp(Offset, -Limit, Limit), -1500.0, 4000.0);
        }
        return Destination;
    }

    bool Finish(bool Allowed)
    {
        const bool Commit = bActive && Allowed;
        bActive = false;
        return Commit;
    }
};
