#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVBF120ImportedMuzzles,
    "OrbitalVanguard.Presentation.ImportedBF120MuzzleCandidates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVBF120ImportedMuzzles::RunTest(const FString&)
{
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/BF120Trial/SM_BF120_PitchTrial.SM_BF120_PitchTrial"));
    if(!TestNotNull(TEXT("BF120 isolated pitch mesh exists"),Mesh))return false;
    const auto* Data=Mesh->GetRenderData();
    if(!TestTrue(TEXT("Imported mesh has render geometry"),Data&&Data->LODResources.Num()>0))return false;
    const auto& Buffer=Data->LODResources[0].VertexBuffers.PositionVertexBuffer;
    const uint32 Count=Buffer.GetNumVertices();if(!TestTrue(TEXT("Mesh has vertices"),Count>0))return false;
    double MinX=DBL_MAX;for(uint32 I=0;I<Count;++I)MinX=FMath::Min(MinX,double(Buffer.VertexPosition(I).X));
    FVector Lo[2]={FVector(DBL_MAX),FVector(DBL_MAX)},Hi[2]={FVector(-DBL_MAX),FVector(-DBL_MAX)};
    int32 Hits[2]={0,0};
    for(uint32 I=0;I<Count;++I) {
        const FVector P(Buffer.VertexPosition(I));if(P.X>MinX+2.0||FMath::IsNearlyZero(P.Y))continue;
        const int32 Side=P.Y>0?0:1;Lo[Side]=Lo[Side].ComponentMin(P);Hi[Side]=Hi[Side].ComponentMax(P);++Hits[Side];
    }
    // Blender source uses opposite Y and the clearance trial is 1 cm above the imported mesh.
    const FVector Expected[]={FVector(-241.143,41.673,48.709),FVector(-241.167,-42.358,48.477)};
    for(int32 I=0;I<2;++I) {
        if(!TestTrue(FString::Printf(TEXT("Barrel %d front band has vertices"),I),Hits[I]>10))continue;
        const FVector Center=(Lo[I]+Hi[I])*.5;
        AddInfo(FString::Printf(TEXT("UE imported barrel %d candidate center %s; no socket or gameplay approval"),I,*Center.ToString()));
        TestTrue(FString::Printf(TEXT("Barrel %d center matches source after axis conversion"),I),Center.Equals(Expected[I],1.0));
    }
    return true;
}
#endif
