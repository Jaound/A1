#include "OVMarkedSlots.h"

namespace
{
const FOVMarkedSlotCandidate Candidates[] = {
#include "OVMarkedSlotsData.inl"
};
}

TArray<FOVMarkedSlotCandidate> OVMarkedSlots::GetCandidates(FName HullId)
{
    TArray<FOVMarkedSlotCandidate> Result;
    for (const FOVMarkedSlotCandidate& Candidate : Candidates)
        if (Candidate.HullId == HullId) Result.Add(Candidate);
    return Result;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OVCalibration.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVMarkedSlotsTest, "OrbitalVanguard.Presentation.MarkedSlotCandidates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVMarkedSlotsTest::RunTest(const FString&)
{
    int32 Count = 0;
    for (const FName HullId : OVCalibration::GetHullIds())
    {
        TSet<FName> Ids;
        for (const FOVMarkedSlotCandidate& Candidate : OVMarkedSlots::GetCandidates(HullId))
        {
            ++Count;
            TestFalse(TEXT("Each face retains a unique slot ID"), Ids.Contains(Candidate.SlotId));
            Ids.Add(Candidate.SlotId);
            TestFalse(TEXT("Position is finite"), Candidate.LocalPosition.ContainsNaN());
            TestTrue(TEXT("Surface normal is normalized"), Candidate.LocalNormal.IsNormalized());
        }
    }
    TestEqual(TEXT("All annotated candidates retained"), Count, 129);
    const auto Corvette = OVMarkedSlots::GetCandidates(TEXT("corvette"));
    TestTrue(TEXT("Port and starboard marks remain independent"),
        Corvette.ContainsByPredicate([](const FOVMarkedSlotCandidate& C) { return C.SlotId == TEXT("left_yellow_0"); }) &&
        Corvette.ContainsByPredicate([](const FOVMarkedSlotCandidate& C) { return C.SlotId == TEXT("right_yellow_0"); }));
    TestTrue(TEXT("Unmarked transport does not acquire invented slots"), OVMarkedSlots::GetCandidates(TEXT("transport")).IsEmpty());
    return true;
}

static void CheckMarkedSurface(FAutomationTestBase& Test, FName HullId, UStaticMesh* Mesh)
{
    if (!Test.TestNotNull(TEXT("Imported full hull mesh"), Mesh) ||
        !Test.TestTrue(TEXT("Render triangles are available"), Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0)) return;
    const auto Bounds = Mesh->GetBounds();
    const double Scale = (FVector(175, 75, 30) / Bounds.BoxExtent.ComponentMax(FVector(1))).GetMin();
    const auto& LOD = Mesh->GetRenderData()->LODResources[0];
    const FQuat Rotation = FRotator(0, 180, 0).Quaternion();
    for (const auto& Candidate : OVMarkedSlots::GetCandidates(HullId))
    {
        const FString Id = Candidate.SlotId.ToString();
        const FVector Direction = Id.StartsWith(TEXT("top_")) ? FVector::UpVector :
            Id.StartsWith(TEXT("bottom_")) ? FVector::DownVector :
            Id.StartsWith(TEXT("left_")) ? FVector(0, -1, 0) : FVector(0, 1, 0);
        const FVector Start = Candidate.LocalPosition + Direction * 100;
        const FVector End = Candidate.LocalPosition - Direction * 100;
        double Best = DBL_MAX;
        FVector BestPoint = FVector::ZeroVector;
        for (int32 I = 0; I + 2 < LOD.IndexBuffer.GetNumIndices(); I += 3)
        {
            const auto Position = [&](uint32 Index) {
                return Rotation.RotateVector((FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Index)) - Bounds.Origin) * Scale);
            };
            FVector Hit, Normal;
            if (FMath::SegmentTriangleIntersection(Start, End, Position(LOD.IndexBuffer.GetIndex(I)),
                Position(LOD.IndexBuffer.GetIndex(I + 1)), Position(LOD.IndexBuffer.GetIndex(I + 2)), Hit, Normal))
            {
                const double Distance = FVector::DistSquared(Start, Hit);
                if (Distance < Best) { Best = Distance; BestPoint = Hit; }
            }
        }
        const double Error = FVector::Distance(BestPoint, Candidate.LocalPosition);
        Test.AddInfo(FString::Printf(TEXT("%s %s UE projection error %.4f cm"), *HullId.ToString(), *Id, Error));
        Test.TestTrue(*FString::Printf(TEXT("%s %s matches imported surface within 1 cm"), *HullId.ToString(), *Id), Best < DBL_MAX && Error < 1.0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVMarkedCorvetteSurfaceTest, "OrbitalVanguard.Presentation.MarkedCorvetteUESurface",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVMarkedCorvetteSurfaceTest::RunTest(const FString&)
{
    CheckMarkedSurface(*this, TEXT("corvette"), LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/Ships/SM_PC14_FullMesh.SM_PC14_FullMesh")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVMarkedAllSurfaceTest, "OrbitalVanguard.Presentation.MarkedAllUESurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVMarkedAllSurfaceTest::RunTest(const FString&)
{
    for (FName HullId : OVCalibration::GetHullIds())
    {
        if (HullId == TEXT("corvette") || OVMarkedSlots::GetCandidates(HullId).IsEmpty()) continue;
        FString Name = HullId.ToString();
        Name[0] = FChar::ToUpper(Name[0]);
        for (int32 I = 1; I < Name.Len(); ++I)
            if (Name[I - 1] == TEXT('_')) Name[I] = FChar::ToUpper(Name[I]);
        const FString Path = FString::Printf(TEXT("/Game/Presentation/Ships/%sFull/SM_Hull.SM_Hull"), *Name);
        CheckMarkedSurface(*this, HullId, LoadObject<UStaticMesh>(nullptr, *Path));
    }
    return true;
}
#endif
