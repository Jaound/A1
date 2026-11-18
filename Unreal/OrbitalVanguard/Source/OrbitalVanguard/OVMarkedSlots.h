#pragma once

#include "CoreMinimal.h"

// Candidate geometry derived from the user's annotated orthographic views.
// These are not equipped weapons and are not yet verified against UE mesh data.
enum class EOVMarkedSlotKind : uint8 { General, Interception, Dedicated };
struct FOVMarkedSlotCandidate
{
    FName HullId;
    FName SlotId;
    EOVMarkedSlotKind Kind;
    FVector LocalPosition;
    FVector LocalNormal;
};

namespace OVMarkedSlots
{
    ORBITALVANGUARD_API TArray<FOVMarkedSlotCandidate> GetCandidates(FName HullId);
}
