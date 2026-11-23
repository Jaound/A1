#include "OVDevelopmentSubsystem.h"
#include "OVController.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCalibration.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVHeroSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

bool UOVDevelopmentSubsystem::OpenRosterRoom()
{
    auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(!PC)return false;
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    auto* Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();
    for(const auto& U:Interior->Units)if(U.HeroId.IsValid() && U.HP>0 && U.State!=EOVInteriorState::Dead)
    {
        if((Mission->bEnabled && Mission->Result==EOVMissionResult::Ongoing && Mission->GameTime>0) ||
            !U.Ship.IsValid() || PC->Combat->IsDeparted(U.Ship.Get()) || U.Ship->bFTLCharging ||
            U.Ship->Systems->FactionId!=U.FactionId || U.State==EOVInteriorState::InTransit || U.State==EOVInteriorState::AwaitingRecovery)
        {LastMessage=TEXT("운용 중 영웅은 먼저 실제 철수·회수해야 합니다. 전투를 종료하지 않은 채 편성실로 무료 귀환할 수 없습니다.");return false;}
    }
    // Detach before Mission::Disable so unfinished operations cannot be hidden from persistence.
    Interior->Disable(); PC->ResetPhase2C(); PC->Combat->DisableCombat();
    PC->bSimulationPaused=true;PC->ClearSelection();
    LastMessage=TEXT("비전투 편성실 · 영웅 HP/성장/손실/소모품 유지 · 임무 시작 시 선택 편제만 배치");
    PC->LastAction=LastMessage;return true;
}

TArray<AOVShip*> UOVDevelopmentSubsystem::SpawnShips(FName Id, int32 Faction, int32 Count,
    FVector Location, float Spacing, int32 AircraftPerHangar, int32 Troops, bool MoveAuto, bool FireAuto)
{
    auto* PC = Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->bDeveloperMode) { LastMessage=TEXT("개발 모드를 켜야 시험 함선을 생성할 수 있습니다."); return {}; }
    return SpawnTrialShips(Id,Faction,Count,Location,Spacing,AircraftPerHangar,Troops,MoveAuto,FireAuto,false);
}

bool UOVDevelopmentSubsystem::HasPlacementClearance(const AOVController& PC, FVector Position, double Radius)
{
    for (AOVShip* Other : PC.Ships)
        if (IsValid(Other) && !Other->IsHidden() && Other->Systems->Layout)
        {
            const double OtherRadius = (Other->Systems->Layout->LocalHalfExtent * Other->GetActorScale3D()).Size();
            if (FVector::Dist(Position,Other->GetActorLocation()) < Radius + OtherRadius + 200) return false;
        }
    return true;
}

TArray<AOVShip*> UOVDevelopmentSubsystem::SpawnTrialShips(FName Id, int32 Faction, int32 Count,
    FVector Location, float Spacing, int32 AircraftPerHangar, int32 Troops, bool MoveAuto, bool FireAuto, bool bPlayerRange)
{
    TArray<AOVShip*> Result;
    auto* PC = Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    auto* Combat = GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    if (!PC || !Combat->bEnabled || PC->CalibrationScenario < 0)
    { LastMessage=TEXT("후보 프로필 시험장(F6/F7/통합 임무)을 먼저 여세요. 기존 장면은 자동 초기화하지 않습니다."); return Result; }
    const double Scale = OVCalibration::GetHullGeometryScale(Id);
    if (Scale<=0 || Faction<0 || Faction>1 || Count<1 || Count>16 || AircraftPerHangar<0 || AircraftPerHangar>20 || Troops<0 || Troops>12 ||
        Location.ContainsNaN() || !FMath::IsFinite(Spacing) || Spacing<0)
    { LastMessage=TEXT("정의/소속/수량 오류: 함선 1~16척, 격납고당 0~20기, 병력 0~12명만 시험 공급합니다."); return Result; }
    SpawnedShips.RemoveAll([](const auto& S){return !IsValid(S);});
    auto* Transit=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    for(int32 I=0; I<Count; ++I)
    {
        const FVector Position=Location+FVector(0,Spacing*I,0);
        if(FMath::Abs(Position.X)>OVCalibration::XYLimit || FMath::Abs(Position.Y)>OVCalibration::XYLimit ||
            Position.Z<OVCalibration::MinZ || Position.Z>OVCalibration::MaxZ)
        { LastMessage=TEXT("이동 가능한 시험장 범위 밖입니다. 좌표 또는 배치 간격을 줄이세요."); break; }
        if(!HasPlacementClearance(*PC,Position,OVCalibration::GetHullWorldHalfExtent(Id).Size()))
        { LastMessage=TEXT("다른 함선과 너무 가깝습니다."); break; }
        AOVShip* Ship=GetWorld()->SpawnActor<AOVShip>();
        if(!Ship || !OVCalibration::ConfigureHull(*Combat,*Ship,Id,Faction==0,Position,Scale))
        { if(Ship)Ship->Destroy(); LastMessage=TEXT("함체/시설/무장 프로필 생성 실패"); break; }
        const FString HullName=OVCalibration::GetHullName(Id);
        if(bPlayerRange) {
            int32 Ordinal=PC->Ships.Num();
            do { Ship->ShipName=FString::Printf(TEXT("%s %02d"),*HullName,++Ordinal); }
            while(PC->Ships.ContainsByPredicate([Ship](const auto& Other){ return IsValid(Other.Get()) && Other->ShipName==Ship->ShipName; }));
        } else Ship->ShipName=FString::Printf(TEXT("시험 %s · %s"),*HullName,*Ship->GetName());
        Ship->SetAutomation(MoveAuto,FireAuto); PC->Ships.Add(Ship); SpawnedShips.Add(Ship); Result.Add(Ship);
        int32 PDCount=0;for(const auto& M:Ship->Systems->Layout->WeaponMounts)if(M.Type==EOVWeaponType::Interception)++PDCount;
        auto* PD=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
        if(!PD->bEnabled)PD->Reset(260112);
        PD->RegisterShip(Ship,PDCount,Id==TEXT("air_defense")?1.5f:1.f);
        GetWorld()->GetSubsystem<UOVCarrierSubsystem>()->RegisterCarrier(Ship,AircraftPerHangar);
        Transit->AddShip(Ship); Interior->AddShip(Ship);
        if(Id==TEXT("assault"))Transit->RegisterShuttle(Ship);
        if(Troops>0)Interior->RegisterBoardingTeam(Ship,Faction,0,Troops);
        if(Id==TEXT("repair")||Id==TEXT("mining")||Id==TEXT("transport"))
            GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->RegisterDevShip(Ship,Id==TEXT("repair")?EOVDevLogisticsRole::Repair:
                Id==TEXT("mining")?EOVDevLogisticsRole::Mining:EOVDevLogisticsRole::Transport,1000,Id==TEXT("repair")?500:0);
    }
    if(Result.Num()==Count)LastMessage=FString::Printf(TEXT("시험 함선 %d척 생성 · 최초 재고/병력만 공급 · 캠페인 획득 아님"),Count);
    else if(Result.Num()>0)LastMessage=FString::Printf(TEXT("%d/%d척 생성. "),Result.Num(),Count)+LastMessage;
    if(Result.Num()>0 && !bPlayerRange)PC->SelectShip(Result.Last());
    return Result;
}

bool UOVDevelopmentSubsystem::DeleteShip(AOVShip* Ship)
{
    auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(!PC||!PC->bDeveloperMode||!IsValid(Ship)||!SpawnedShips.Contains(Ship))
    {LastMessage=TEXT("이 창에서 만든 시험 함선만 삭제할 수 있습니다. 원래 시험 편제는 장면 재시작을 사용하세요.");return false;}
    return RemoveTrialShip(Ship);
}

bool UOVDevelopmentSubsystem::RemoveTrialShip(AOVShip* Ship)
{
    auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(!PC || !IsValid(Ship) || !SpawnedShips.Contains(Ship)) return false;
    auto* Wing=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
    auto* Transit=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
    if(Wing->HasOutstandingAircraft(Ship)) {LastMessage=TEXT("미귀환 함재기가 있습니다. 실제 회수 후 삭제하세요.");return false;}
    if(!Transit->CanRemoveShip(Ship)) {LastMessage=TEXT("생존 승객/운송 연결이 남아 있습니다. 먼저 회수·배치 해제하세요.");return false;}
    auto* Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
    if(!Interior->RemoveShip(Ship)){LastMessage=Interior->LastMessage;return false;}
    auto* Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    for(AOVShip* Other:PC->Ships)if(IsValid(Other)&&Other->AttackTarget==Ship)Combat->CancelAttackOrder(Other);
    Combat->CancelAttackOrder(Ship);
    // Emitted attacks retain their immutable attacker/faction/profile snapshots.
    Combat->Ships.RemoveAll([Ship](const auto& S){return S.Ship==Ship;});
    Wing->Aircraft.RemoveAll([Ship](const auto& C){return C.Carrier==Ship;});
    Wing->Hangars.RemoveAll([Ship](const auto& H){return H.Carrier==Ship;});
    Wing->Carriers.RemoveAll([Ship](const auto& C){return C.Carrier==Ship;});
    GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>()->Ships.RemoveAll([Ship](const auto& S){return S.Ship==Ship;});
    Transit->RemoveShip(Ship); GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->RemoveDevShip(Ship);
    if(PC->Selected==Ship)PC->ClearSelection(); else PC->Selection.Remove(Ship);
    PC->Ships.Remove(Ship); PC->TrialFTLRemaining.Remove(Ship); SpawnedShips.Remove(Ship); Ship->Destroy();
    LastMessage=TEXT("시험 함선과 해당 격납 시험 재고를 명시적으로 폐기 · 기발사 공격 기록은 유지");return true;
}

bool UOVDevelopmentSubsystem::EquipWeapon(AOVShip* Ship,FName MountId,FName ProfileId)
{
    const auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(!PC||!PC->bDeveloperMode){LastMessage=TEXT("개발 모드에서만 시험 장착할 수 있습니다.");return false;}
    auto* Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    const bool Result=IsValid(Ship)&&Combat->SetWeaponProfile(Ship,MountId,ProfileId);
    LastMessage=Result?TEXT("무장 교체 · 슬롯과 티어 제약 유지 · HP 회복 없음"):
        TEXT("교체 불가: 슬롯·티어·함체 제한 또는 무장 정보를 확인하세요.");return Result;
}
bool UOVDevelopmentSubsystem::SetBF120Trial(AOVShip* Ship,FName MountId,bool bEnable)
{
    const auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(!PC||!PC->bDeveloperMode){LastMessage=TEXT("개발 모드를 먼저 켜세요.");return false;}
    if(!IsValid(Ship)||!Ship->Systems||!Ship->Systems->Layout||Ship->Systems->FactionId!=0||Ship->Systems->bWrecked)
    {LastMessage=TEXT("사용 가능한 아군 함선의 함포를 선택하세요.");return false;}
    const auto* Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    const auto State=Combat?Combat->GetShipState(Ship):FOVCombatShipState();
    const auto* Mount=State.Mounts.FindByPredicate([MountId](const auto& M){return M.MountId==MountId;});
    const int32 Index=Ship->Systems->Layout->WeaponMounts.IndexOfByPredicate([MountId](const auto& M){return M.WeaponId==MountId;});
    if(!Mount||Mount->Profile.Kind!=EOVAttackKind::Cannon||!Ship->Systems->Layout->WeaponMounts.IsValidIndex(Index)
       ||Ship->Systems->Layout->WeaponMounts[Index].Type!=EOVWeaponType::Cannon)
    {LastMessage=TEXT("현재 장착된 함포에서만 시험할 수 있습니다.");return false;}
    const FName Existing=Ship->Systems->Layout->WeaponMounts[Index].PresentationProfile;
    if((bEnable&&!Existing.IsNone()&&Existing!=TEXT("BF120Trial"))||(!bEnable&&Existing!=TEXT("BF120Trial")))
    {LastMessage=TEXT("해당 장착점은 BF120 시험 상태가 아닙니다.");return false;}
    FOVWeaponMountDefinition Candidate=Ship->Systems->Layout->WeaponMounts[Index];Candidate.PresentationProfile=TEXT("BF120Trial");
    if(bEnable&&!OVWeaponGeometry::HasBF120Trial(Candidate))
    {LastMessage=TEXT("BF120 시험 자산을 찾을 수 없습니다.");return false;}
    if((bEnable&&Existing==TEXT("BF120Trial"))||(!bEnable&&Existing.IsNone()))
    {LastMessage=TEXT("이미 요청한 시험 상태입니다.");return true;}
    auto* Copy=DuplicateObject<UShipLayoutDataAsset>(Ship->Systems->Layout,Ship);
    if(!Copy){LastMessage=TEXT("시험 레이아웃을 만들 수 없습니다.");return false;}
    Copy->WeaponMounts[Index].PresentationProfile=bEnable?FName(TEXT("BF120Trial")):NAME_None;
    Ship->Systems->Layout=Copy;Ship->RefreshWeaponVisuals();
    LastMessage=bEnable?TEXT("BF120 원본 기반 시험 · 포구 위치 변경 · 최종 모델 아님"):TEXT("시험 해제 · 기존 포탑과 발사 위치로 복귀");
    return true;
}
bool UOVDevelopmentSubsystem::SetShipTarget(AOVShip* Ship,AOVShip* Target)
{
    const auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(!PC||!PC->bDeveloperMode){LastMessage=TEXT("개발 모드를 먼저 켜세요.");return false;}
    const bool Result=IsValid(Ship)&&GetWorld()->GetSubsystem<UOVCombatSubsystem>()->SetTarget(Ship,Target);
    LastMessage=Result?TEXT("표적 지정 수락 · 발사 명령과 별개"):TEXT("표적 지정 불가: 소속·지휘·이탈·완파 상태 확인");return Result;
}
bool UOVDevelopmentSubsystem::FireWeapon(AOVShip* Ship,FName MountId)
{
    const auto* PC=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(!PC||!PC->bDeveloperMode){LastMessage=TEXT("개발 모드를 먼저 켜세요.");return false;}
    auto* Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    const bool Result=IsValid(Ship)&&Combat->RequestFire(Ship,MountId);
    LastMessage=Result?TEXT("발사 요청 수락 · 실제 발사 가능 조건은 별도 검사"):
        TEXT("발사 요청 거부: 표적·무장·지휘 상태 확인");
    if(IsValid(Ship))LastMessage+=TEXT(" / ")+Combat->GetReadiness(Ship,MountId).Reason;
    return Result;
}
FString UOVDevelopmentSubsystem::GetShipDetails(AOVShip* Ship) const
{
    if(!IsValid(Ship)||!Ship->Systems->Layout)return TEXT("함선을 선택하세요.");
    return FString::Printf(TEXT("%s\n실행 ID %s\n시설 %d · 장착점 %d\n시험 생성 %s"),*Ship->ShipName,*Ship->GetName(),
        Ship->Systems->Layout->Slots.Num(),Ship->Systems->Layout->WeaponMounts.Num(),SpawnedShips.Contains(Ship)?TEXT("예"):TEXT("아니오"));
}
