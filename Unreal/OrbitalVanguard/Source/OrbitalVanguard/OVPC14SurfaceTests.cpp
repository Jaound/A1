#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Misc/AutomationTest.h"
#include "OVCalibration.h"
#include "OVCombatSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPC14SurfaceGrid,
    "OrbitalVanguard.Presentation.ImportedPC14SurfaceGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVPC14SurfaceGrid::RunTest(const FString&)
{
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Ships/SM_PC14.SM_PC14"));
    if(!TestNotNull(TEXT("PC14 trial mesh exists"),Mesh)||!TestTrue(TEXT("Mesh render data available"),Mesh->GetRenderData()&&Mesh->GetRenderData()->LODResources.Num()>0))return false;
    const auto& LOD=Mesh->GetRenderData()->LODResources[0];const auto Bounds=Mesh->GetBounds();
    const FVector Ratio=FVector(175,75,30)/Bounds.BoxExtent.ComponentMax(FVector(1));const double Scale=Ratio.GetMin();
    TArray<double> Xs,Ys;
    for(int32 X=-170;X<=170;X+=10)Xs.Add(X);
    Xs.Append({-46.6666667,46.6666667});Xs.Sort();
    for(int32 Y=-70;Y<=70;Y+=5)Ys.Add(Y);
    FString Rows;int32 TopHits=0,BottomHits=0;
    for(double X:Xs)for(double Y:Ys)
    {
        double Zs[2]={0,0};bool Hits[2]={false,false};
        for(int32 Side=0;Side<2;++Side)
        {
            const double Sign=Side==0?1.0:-1.0;const FVector Start(X,Y,Sign*100),End(X,Y,-Sign*100);double Nearest=DBL_MAX;
            for(int32 I=0;I+2<LOD.IndexBuffer.GetNumIndices();I+=3)
            {
                const auto Position=[&](uint32 Index){return (FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Index))-Bounds.Origin)*Scale;};
                FVector Hit,Normal;
                if(FMath::SegmentTriangleIntersection(Start,End,Position(LOD.IndexBuffer.GetIndex(I)),Position(LOD.IndexBuffer.GetIndex(I+1)),Position(LOD.IndexBuffer.GetIndex(I+2)),Hit,Normal))
                {
                    const double Distance=FVector::DistSquared(Start,Hit);
                    if(Distance<Nearest){Nearest=Distance;Zs[Side]=Hit.Z;Hits[Side]=true;}
                }
            }
        }
        TopHits+=Hits[0]?1:0;BottomHits+=Hits[1]?1:0;
        if(!Rows.IsEmpty())Rows+=TEXT(",\n");
        Rows+=FString::Printf(TEXT("{\"x_cm\":%.4f,\"y_cm\":%.4f,\"top_z_cm\":%s,\"bottom_z_cm\":%s}"),X,Y,
            Hits[0]?*FString::SanitizeFloat(Zs[0]):TEXT("null"),Hits[1]?*FString::SanitizeFloat(Zs[1]):TEXT("null"));
    }
    TestTrue(TEXT("Grid finds upper and lower imported geometry"),TopHits>0&&BottomHits>0);
    const FString Json=FString::Printf(TEXT("{\"scope\":\"UE imported PC14 trial mesh; vertical rays, not final collision\",\"selection_half_extent_cm\":[175,75,30],\"top_hits\":%d,\"bottom_hits\":%d,\"samples\":[\n%s\n]}"),TopHits,BottomHits,*Rows);
    const FString Output=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation"),TEXT("PC14SurfaceGrid.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true);
    TestTrue(TEXT("Surface report is written to generated validation folder"),FFileHelper::SaveStringToFile(Json,*Output));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPC14ImportedMountSurface,
    "OrbitalVanguard.Presentation.ImportedPC14MountSurface",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVPC14ImportedMountSurface::RunTest(const FString&)
{
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Ships/SM_PC14.SM_PC14"));
    if(!TestNotNull(TEXT("Prepared PC14 static mesh exists"),Mesh))return false;
    const auto* Data=Mesh->GetRenderData();
    if(!TestTrue(TEXT("Imported mesh has render geometry"),Data&&Data->LODResources.Num()>0))return false;
    const auto& LOD=Data->LODResources[0];
    const uint32 Vertices=LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
    const int32 Indices=LOD.IndexBuffer.GetNumIndices();
    if(!TestTrue(TEXT("Imported mesh has triangle geometry"),Vertices>0&&Indices>=3&&Indices%3==0))return false;
    const auto Bounds=Mesh->GetBounds();
    const FVector Ratio=FVector(175,75,30)/Bounds.BoxExtent.ComponentMax(FVector(1));
    const double Scale=Ratio.GetMin();
    TestTrue(TEXT("Imported fit is finite and positive"),FMath::IsFinite(Scale)&&Scale>0);
    const double Xs[]={-46.6666667,46.6666667};
    const double Sides[]={1,-1};
    for(int32 M=0;M<2;++M)
    {
        const FVector Start(Xs[M],0,Sides[M]*100),End(Xs[M],0,-Sides[M]*100);
        double Nearest=DBL_MAX;FVector Best=FVector::ZeroVector;
        for(int32 I=0;I+2<Indices;I+=3)
        {
            const uint32 IA=LOD.IndexBuffer.GetIndex(I),IB=LOD.IndexBuffer.GetIndex(I+1),IC=LOD.IndexBuffer.GetIndex(I+2);
            if(IA>=Vertices||IB>=Vertices||IC>=Vertices)continue;
            const auto Position=[&](uint32 Index){return (FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Index))-Bounds.Origin)*Scale;};
            FVector Hit,Normal;
            if(FMath::SegmentTriangleIntersection(Start,End,Position(IA),Position(IB),Position(IC),Hit,Normal))
            {
                const double Distance=FVector::DistSquared(Start,Hit);
                if(Distance<Nearest){Nearest=Distance;Best=Hit;}
            }
        }
        TestTrue(FString::Printf(TEXT("Mount %d ray meets imported geometry"),M),Nearest<DBL_MAX);
        if(Nearest<DBL_MAX)
            AddInfo(FString::Printf(TEXT("PC14 mount %d UE local x=%.3f surface z=%.3f mount origin z=%.3f gap=%.3f cm; pivot and fit still unapproved"),M,Xs[M],Best.Z,Sides[M]*42,FMath::Abs(Sides[M]*42-Best.Z)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVPC14MountAdapter,
    "OrbitalVanguard.Presentation.ImportedPC14MountAdapter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVPC14MountAdapter::RunTest(const FString&)
{
    auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("ov.Presentation.ImportedHull"));
    if(!TestNotNull(TEXT("Source inspection flag exists"),CVar))return false;
    auto* MaterialCVar=IConsoleManager::Get().FindConsoleVariable(TEXT("ov.Presentation.PC14SourceMaterial"));
    if(!TestNotNull(TEXT("Source material inspection flag exists"),MaterialCVar))return false;
    const int32 PreviousMaterial=MaterialCVar->GetInt();MaterialCVar->Set(1,ECVF_SetByConsole);
    const int32 Previous=CVar->GetInt();CVar->Set(0,ECVF_SetByConsole);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    TArray<AOVShip*> Ships={World->SpawnActor<AOVShip>(),World->SpawnActor<AOVShip>(),World->SpawnActor<AOVShip>()};
    auto* Combat=World->GetSubsystem<UOVCombatSubsystem>();
    const bool Started=OVCalibration::Start(*Combat,Ships,1);
    TestTrue(TEXT("Actual calibration range starts"),Started);
    if(Started)
    {
        AOVShip* Ship=Ships[1];Ship->RefreshWeaponVisuals();
        const FName Gun=Ship->Systems->Layout->WeaponMounts[0].WeaponId;
        const FVector MuzzleBefore=Ship->GetWeaponMuzzleWorld(Gun);
        const float HPBefore=Ship->Systems->HullHP;
        auto* OriginalMaterial=Ship->Hull->GetMaterial(0);
        CVar->Set(1,ECVF_SetByConsole);Ship->ApplyPresentationHull(TEXT("corvette"));
        auto* TrialMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/PC14Trial/M_PC14_TrialReadable.M_PC14_TrialReadable"));
        TestTrue(TEXT("Source-derived trial material applied"),TrialMaterial&&Ship->Hull->GetMaterial(0)&&Ship->Hull->GetMaterial(0)->GetMaterial()==TrialMaterial->GetMaterial());
        TArray<UStaticMeshComponent*> Components;Ship->GetComponents<UStaticMeshComponent>(Components);
        int32 Supports=0;
        for(const auto* Component:Components)if(Component->GetName().StartsWith(TEXT("PC14MountSupport_")))
        {
            ++Supports;TestEqual(TEXT("Adapter never changes collision"),Component->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
            const FVector P=Component->GetRelativeLocation(),Scale=Component->GetRelativeScale3D();
            const double HalfHeight=Scale.Z*50;
            if(P.X<0)TestTrue(TEXT("Upper adapter reaches measured hull surface and gun base"),FMath::IsNearlyEqual(P.Z-HalfHeight,-2.550614,.01)&&FMath::IsNearlyEqual(P.Z+HalfHeight,33.68,.01));
            else TestTrue(TEXT("Lower adapter reaches measured hull surface and gun base"),FMath::IsNearlyEqual(P.Z+HalfHeight,-29.390047,.01)&&FMath::IsNearlyEqual(P.Z-HalfHeight,-33.68,.01));
        }
        TestEqual(TEXT("Upper and lower mount adapters exist"),Supports,2);
        TestTrue(TEXT("Presentation leaves actual muzzle unchanged"),Ship->GetWeaponMuzzleWorld(Gun).Equals(MuzzleBefore,.001));
        TestEqual(TEXT("Presentation leaves hull HP unchanged"),Ship->Systems->HullHP,HPBefore);
        Ship->ApplyPresentationHull(TEXT("corvette"));
        Components.Reset();Ship->GetComponents<UStaticMeshComponent>(Components);
        int32 Rebuilt=0;for(const auto* Component:Components)if(Component->GetName().StartsWith(TEXT("PC14MountSupport_")))++Rebuilt;
        TestEqual(TEXT("Repeated application does not duplicate adapters"),Rebuilt,2);
        Ship->SetCalibrationHull(&Ship->Systems->Layout->LocalHalfExtent);
        TestTrue(TEXT("Leaving trial hull restores previous material"),Ship->Hull->GetMaterial(0)==OriginalMaterial);
        Components.Reset();Ship->GetComponents<UStaticMeshComponent>(Components);
        TestFalse(TEXT("Changing hull removes old adapters"),Components.ContainsByPredicate([](const UStaticMeshComponent* C){return C->GetName().StartsWith(TEXT("PC14MountSupport_"));}));
        MaterialCVar->Set(0,ECVF_SetByConsole);Ship->ApplyPresentationHull(TEXT("corvette"));
        TestTrue(TEXT("Source material switch off retains original faction material"),Ship->Hull->GetMaterial(0)==OriginalMaterial);
    }
    CVar->Set(Previous,ECVF_SetByConsole);MaterialCVar->Set(PreviousMaterial,ECVF_SetByConsole);World->DestroyWorld(false);
    return true;
}
#endif
