#include "OVPlayScenarioSubsystem.h"
#include "OVCalibration.h"
#include "OVController.h"
#include "OVCombatSubsystem.h"
#include "OVDevelopmentSubsystem.h"
#include "OVOrderInput.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

FVector UOVPlayScenarioSubsystem::SandboxPosition(FVector Center, int32 Index) const
{
    const double Spacing = SandboxExtent.Size()*2 + 600;
    return Center + FVector(0, (Index - (SandboxCount-1)*.5) * Spacing, 0);
}

void UOVPlayScenarioSubsystem::EndSandboxPlacement()
{
    for (AStaticMeshActor* Ghost : SandboxGhosts) if (IsValid(Ghost)) Ghost->Destroy();
    SandboxGhosts.Reset();
    bSandboxPlacing = bSandboxPointValid = bSandboxHasPoint = false;
}

bool UOVPlayScenarioSubsystem::BeginSandboxPlacement()
{
    EndSandboxPlacement();
    UStaticMesh* Mesh = OVCalibration::LoadHullMesh(SandboxHull);
    SandboxExtent = OVCalibration::GetHullWorldHalfExtent(SandboxHull);
    if (!Mesh || SandboxExtent.GetMin() <= 0)
    {
        SandboxHint = TEXT("선체 모델을 불러올 수 없습니다");
        return false;
    }
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto* Tint=Base ? UMaterialInstanceDynamic::Create(Base,this) : nullptr;
    if(Tint) Tint->SetVectorParameterValue(TEXT("Color"),SandboxFaction ? FLinearColor(1.f,.35f,.08f) : FLinearColor(.1f,.85f,1.f));
    for (int32 I=0; I<SandboxCount; ++I)
    {
        auto* Ghost = GetWorld()->SpawnActor<AStaticMeshActor>();
        if (!Ghost) { EndSandboxPlacement(); SandboxHint=TEXT("미리보기 생성 실패"); return false; }
        SandboxGhosts.Add(Ghost);
        Ghost->Tags.Add(TEXT("SandboxHullPreview"));
        Ghost->SetActorEnableCollision(false);
        Ghost->SetActorHiddenInGame(true);
        auto* Part = Ghost->GetStaticMeshComponent();
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(false);
        Part->SetStaticMesh(Mesh);
        if(Tint) for(int32 Slot=0;Slot<Part->GetNumMaterials();++Slot) Part->SetMaterial(Slot,Tint);
        const double Scale=(SandboxExtent / Mesh->GetBounds().BoxExtent.ComponentMax(FVector(1))).GetMin();
        Ghost->SetActorScale3D(FVector(Scale));
        Ghost->SetActorRotation(FRotator(0,SandboxFaction==0 ? 180 : 0,0));
    }
    bSandboxPlacing = true;
    SandboxHint = TEXT("전장을 클릭해 배치 · 우클릭 취소");
    return true;
}

bool UOVPlayScenarioSubsystem::CanPlaceSandboxShip(const AOVController& PC, FVector Center) const
{
    if (ActiveId!=TEXT("Sandbox") || !bSandboxPlacing || bSandboxEngaged || !PC.bSimulationPaused || Center.ContainsNaN()) return false;
    for (int32 I=0; I<SandboxCount; ++I)
    {
        const FVector P=SandboxPosition(Center,I);
        if (FMath::Abs(P.X)>OVCalibration::XYLimit || FMath::Abs(P.Y)>OVCalibration::XYLimit ||
            P.Z<OVCalibration::MinZ || P.Z>OVCalibration::MaxZ ||
            !UOVDevelopmentSubsystem::HasPlacementClearance(PC,P,SandboxExtent.Size())) return false;
    }
    return true;
}

bool UOVPlayScenarioSubsystem::UpdateSandboxPointer(AOVController& PC, FVector2D Pixel, bool bWorld)
{
    if (!bSandboxPlacing) return false;
    FVector Eye, Direction;
    bSandboxHasPoint=bWorld && PC.DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,Eye,Direction) &&
        FOVOrderInput::Project(Eye,Direction,SandboxAltitude,SandboxPoint,1,OVCalibration::RayLength);
    bSandboxPointValid=bSandboxHasPoint && CanPlaceSandboxShip(PC,SandboxPoint);
    for (int32 I=0; I<SandboxGhosts.Num(); ++I)
    {
        AStaticMeshActor* Ghost=SandboxGhosts[I];
        Ghost->SetActorHiddenInGame(!bSandboxHasPoint);
        if (bSandboxHasPoint)
        {
            const auto Origin=Ghost->GetStaticMeshComponent()->GetStaticMesh()->GetBounds().Origin;
            Ghost->SetActorLocation(SandboxPosition(SandboxPoint,I) - Ghost->GetActorTransform().TransformVector(Origin));
        }
    }
    return bSandboxPointValid;
}

bool UOVPlayScenarioSubsystem::PlaceSandboxShip(FVector Center)
{
    auto* PC=Owner.Get();
    if (!PC || !CanPlaceSandboxShip(*PC,Center)) { SandboxHint=TEXT("배치 불가 · 함선 간격과 전장 경계를 확인하세요"); return false; }
    auto* Dev=GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>();
    FOVHullMetadata Hull;
    if (!OVCalibration::GetHullMetadata(SandboxHull,Hull)) return false;
    const auto Ships=Dev->SpawnTrialShips(SandboxHull,SandboxFaction,SandboxCount,SandboxPosition(Center,0),
        SandboxExtent.Size()*2+600,Hull.bAllowAircraftOperations ? 10 : 0,0,true,true,true);
    if (Ships.Num()!=SandboxCount)
    {
        const FString Reason=Dev->LastMessage;
        for (auto* Ship : Ships) Dev->RemoveTrialShip(Ship);
        SandboxHint=Reason;
        return false;
    }
    for (auto* Ship : Ships) { Ship->bHold=false; FreshDeployments.Add(Ship); }
    SandboxHint=FString::Printf(TEXT("%s %s %d척 배치 · 다른 위치에 추가 가능"),
        SandboxFaction ? TEXT("적군") : TEXT("아군"), *OVCalibration::GetHullName(SandboxHull),Ships.Num());
    return true;
}

bool UOVPlayScenarioSubsystem::SandboxAction(AOVController& PC,FName Name)
{
    if (Name==TEXT("Cancel") && (bSandboxPlacing || bSandboxHullMenu)) {
        EndSandboxPlacement(); bSandboxHullMenu=false; SandboxHint.Reset(); return true;
    }
    if (Name==TEXT("Pause") && !bSandboxEngaged) Name=TEXT("SBStart");
    if (!Name.ToString().StartsWith(TEXT("SB"))) return false;
    if (Name==TEXT("SBStop")) {
        PC.bSimulationPaused=true; bSandboxEngaged=false; SandboxHint=TEXT("교전 중단 · 현재 전력과 피해 보존"); return true;
    }
    if (bSandboxEngaged) return true;
    if (Name==TEXT("SBStart"))
    {
        bool Active[2]={};
        for (AOVShip* Ship : PC.Ships)
            if (IsValid(Ship) && !Ship->Systems->bWrecked && !Ship->Systems->bNeutral && !PC.Combat->IsDeparted(Ship) &&
                (Ship->Systems->FactionId==0 || Ship->Systems->FactionId==1)) Active[Ship->Systems->FactionId]=true;
        if (!Active[0] || !Active[1]) { SandboxHint=TEXT("아군과 적군을 한 척 이상 배치하세요"); return true; }
        for (auto Ref : FreshDeployments)
        {
            auto* Ship=Ref.Get();
            if (!Ship || !Ship->IsMovementAutomatic() || Ship->bHold || Ship->bHasDestination || Ship->bManualAttitude || Ship->bFTLCharging) continue;
            AOVShip* Nearest=nullptr; double Best=TNumericLimits<double>::Max();
            for (AOVShip* Target : PC.Ships) {
                if (!IsValid(Target) || Target->Systems->bWrecked || Target->Systems->bNeutral ||
                    Target->Systems->FactionId==Ship->Systems->FactionId || PC.Combat->IsDeparted(Target)) continue;
                const double D=Ship->GetSquaredDistanceTo(Target);
                if (D<Best) { Best=D; Nearest=Target; }
            }
            // Each side stops short of the midpoint; opposing capital ships must
            // not be ordered to converge on the same coordinate.
            if (Nearest && Best>FMath::Square(24000.)) {
                const FVector ToEnemy=Nearest->GetActorLocation()-Ship->GetActorLocation();
                const double ClosingDistance=(ToEnemy.Length()-18000.)*.5;
                Ship->SetDestination(Ship->GetActorLocation()+ToEnemy.GetSafeNormal()*ClosingDistance);
            }
        }
        FreshDeployments.Reset();
        EndSandboxPlacement(); bSandboxHullMenu=false;
        bSandboxEngaged=true; PC.bSimulationPaused=false; SandboxHint=TEXT("교전 진행"); return true;
    }
    if (Name==TEXT("SBRemove"))
    {
        auto* Dev=GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>();
        const auto Selection=PC.Selection;
        int32 Removed=0;
        FString Reason;
        for (AOVShip* Ship : Selection) {
            if (Dev->RemoveTrialShip(Ship)) { FreshDeployments.Remove(Ship); ++Removed; }
            else Reason=Dev->LastMessage;
        }
        SandboxHint=Reason.IsEmpty() ? (Removed ? FString::Printf(TEXT("%d척 제거"),Removed) : FString(TEXT("제거할 시험 함선을 선택하세요"))) : Reason;
        return true;
    }
    if (Name==TEXT("SBPlace"))
    {
        bSandboxHullMenu=false;
        if (bSandboxPlacing) { EndSandboxPlacement(); SandboxHint.Reset(); }
        else { PC.Action(TEXT("Cancel")); BeginSandboxPlacement(); }
        return true;
    }
    if (Name==TEXT("SBMenu")) { EndSandboxPlacement(); bSandboxHullMenu=!bSandboxHullMenu; return true; }
    const bool WasPlacing=bSandboxPlacing;
    if (Name==TEXT("SBAlly") || Name==TEXT("SBEnemy")) SandboxFaction=Name==TEXT("SBEnemy") ? 1 : 0;
    else if (Name==TEXT("SBCount")) SandboxCount=SandboxCount==1?3:SandboxCount==3?5:1;
    else if (Name==TEXT("SBUp") || Name==TEXT("SBDown")) SandboxAltitude=FMath::Clamp(SandboxAltitude+(Name==TEXT("SBUp")?5000:-5000),OVCalibration::MinZ,OVCalibration::MaxZ);
    else if (Name.ToString().StartsWith(TEXT("SBHull_"))) {
        const FName Id(*Name.ToString().Mid(7));
        if (!OVCalibration::GetHullIds().Contains(Id)) return true;
        SandboxHull=Id; bSandboxHullMenu=false;
    } else return true;
    if (WasPlacing) BeginSandboxPlacement();
    return true;
}
