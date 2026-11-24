#include "OVPlayScenarioSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVController.h"
#include "OVCarrierSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"

FString UOVPlayScenarioSubsystem::DifficultyLabel() const
{
    const TCHAR* Names[]={TEXT("쉬움"),TEXT("보통"),TEXT("어려움")};
    return FString::Printf(TEXT("난이도 · %s"),Names[int32(NextDifficulty)]);
}

void UOVPlayScenarioSubsystem::CycleDifficulty()
{
    NextDifficulty=static_cast<EOVCommandDifficulty>((int32(NextDifficulty)+1)%3);
}

void UOVPlayScenarioSubsystem::CommandEnemyHeroes(AOVController& PC)
{
    for (const auto& Unit : PC.Interior->Units)
    {
        AOVShip* Ship=Unit.Ship.Get();
        if (Unit.FactionId!=1 || !ScenarioHeroes.Contains(Unit.HeroId) || Unit.HP<=0 ||
            !IsValid(Ship) || !Ship->Systems || !Ship->Systems->Layout || Ship->Systems->bWrecked ||
            PC.Combat->IsDeparted(Ship) || Unit.State==EOVInteriorState::Fighting ||
            Unit.State==EOVInteriorState::InTransit || Unit.State==EOVInteriorState::AwaitingRecovery ||
            Unit.State==EOVInteriorState::RescueWaiting || Unit.Order==EOVInteriorOrder::Retreat) continue;
        const bool Invading=Unit.bWasInsideTarget && Ship->Systems->FactionId!=Unit.FactionId;
        if (Invading && Unit.HP < Unit.MaxHP*.3f)
        {
            PC.Interior->IssueOrder({Unit.UnitId},EOVInteriorOrder::Retreat);
            continue;
        }
        if (!Invading && Ship->Systems->FactionId!=Unit.FactionId) continue;
        if (!Invading && PC.Interior->Units.ContainsByPredicate([&](const FOVInteriorUnitRecord& Other){
            return Other.FactionId!=Unit.FactionId && Other.HP>0 && Other.Ship==Ship &&
                Other.NodeId==Unit.NodeId && Other.bWasInsideTarget;
        }))
        {
            if (Unit.bDutyRequested || Unit.Order!=EOVInteriorOrder::Defend || Unit.TargetSlot!=Unit.NodeId)
                PC.Interior->IssueOrder({Unit.UnitId},EOVInteriorOrder::Defend,Unit.NodeId);
            continue;
        }
        AOVShip* Target=Ship->AttackTarget;
        if (!Invading && Unit.Role==EOVInteriorRole::Melee && IsValid(Target) && Target->Systems &&
            !Target->Systems->bShieldActive && Ship->Systems->GetEfficiency(EOVFacilityType::Boarding)>0 &&
            FVector::Dist(Ship->GetActorLocation(),Target->GetActorLocation())<=PC.Transport->Settings.PodLaunchRange &&
            PC.Combat->GetDetection(Ship,Target).bDetected && PC.Interior->SetHeroBoardingAssignment(Unit.UnitId,true))
        {
            PC.Transport->LaunchPods(Ship,Target);
            continue;
        }
        const auto Facility = !Invading && Unit.Role==EOVInteriorRole::Ranged ? EOVFacilityType::WeaponControl :
            !Invading && Unit.Role==EOVInteriorRole::Support ? EOVFacilityType::Engine : EOVFacilityType::Bridge;
        for (const auto& Slot : Ship->Systems->Layout->Slots)
        {
            if (Slot.Facility!=Facility || Ship->Systems->GetSlotState(Slot.SlotId).HP<=0) continue;
            if (!Invading && Unit.Role!=EOVInteriorRole::Melee)
            {
                if (!Unit.bDutyRequested || Unit.DutySlot!=Slot.SlotId)
                    PC.Interior->SetHeroDuty(Unit.UnitId,true,Slot.SlotId);
            }
            else
            {
                const auto Order=Invading?EOVInteriorOrder::Capture:EOVInteriorOrder::Defend;
                if (Unit.Order!=Order || Unit.TargetSlot!=Slot.SlotId)
                    PC.Interior->IssueOrder({Unit.UnitId},Order,Slot.SlotId);
            }
            break;
        }
    }
}

void UOVPlayScenarioSubsystem::StepCommandAI(AOVController& PC,float Delta)
{
    for (int32 Side=0; Side<2; ++Side)
    {
        if (ElapsedSeconds < NextCommandDecision[Side]) continue;
        const auto Skill=Side==0?EOVCommandDifficulty::Hard:ActiveDifficulty;
        const double Interval=Skill==EOVCommandDifficulty::Easy?4.:Skill==EOVCommandDifficulty::Normal?.95:.45;
        NextCommandDecision[Side]=ElapsedSeconds+Interval;
        if (Side==1) CommandEnemyHeroes(PC);
        const auto Kind=GetActiveDefinition()->Kind;
        if ((Kind!=EOVPlaySceneKind::Fleet && Kind!=EOVPlaySceneKind::Sandbox) || Skill==EOVCommandDifficulty::Easy) continue;
        for (AOVShip* Ship : PC.Ships)
        {
            if (!IsValid(Ship) || !Ship->Systems || Ship->Systems->FactionId!=Side) continue;
            if (Ship->HangarHost.IsValid() || PC.Carrier->Hangars.ContainsByPredicate([Ship](const auto& Bay){return Bay.InboundShip==Ship;}) || !Ship->IsMovementAutomatic() || Ship->bPlayerHold || Ship->bManualAttitude || Ship->bFTLCharging ||
                !Ship->Systems->CanMove() || PC.Combat->IsDeparted(Ship))
            { Evasions.Remove(Ship); continue; }
            if (auto* Evading=Evasions.Find(Ship))
            {
                if (ElapsedSeconds < Evading->Until) continue;
                if (Evading->HadGoal && Ship->Destination.Equals(Evading->Sidestep,1))
                    Ship->SetDestination(Evading->PreviousGoal);
                Evasions.Remove(Ship);
            }
            for (const auto& Shot : PC.Combat->Attacks)
            {
                if (Shot.FactionId==Side || Shot.Weapon.Family!=EOVWeaponType::Missile ||
                    Shot.Result!=EOVAttackResult::Flying) continue;
                const double Horizon=Skill==EOVCommandDifficulty::Hard?5.:3.1;
                const FVector Position=Ship->GetActorLocation();
                const double Radius=(Ship->GetLocalShieldRadii()*Ship->GetActorScale3D().GetAbs()).GetMax();
                if (FVector::Dist(Shot.Position,Position)>Shot.Weapon.Speed*Horizon+Radius) continue;
                UOVCombatSubsystem::FMissilePath Path;
                UOVCombatSubsystem::PredictMissilePath(Shot,Path);
                bool Threat=false; double Travel=0; FVector Escape=FVector::ZeroVector;
                for (int32 I=1; I<Path.Num() && Travel<Shot.Weapon.Speed*Horizon; ++I)
                {
                    const FVector Closest=FMath::ClosestPointOnSegment(Position,Path[I-1],Path[I]);
                    const FVector Offset=Position-Closest;
                    Travel+=FVector::Dist(Path[I-1],Path[I]);
                    if (Offset.SizeSquared()>FMath::Square(Radius*1.1)) continue;
                    // Crossing the attack plane clears a vertical-launch salvo even as its tangent turns.
                    const FVector Cross=FVector::CrossProduct((Shot.FrozenAimPoint-Shot.Origin).GetSafeNormal(),
                        (Shot.VLSControlPoint-Shot.Origin).GetSafeNormal());
                    const FVector Axis=(Path[I]-Path[I-1]).GetSafeNormal();
                    const FVector Lateral=FVector::VectorPlaneProject(Offset,Axis);
                    Escape=Cross.GetSafeNormal(1.e-4,Lateral.GetSafeNormal(1.,FVector::RightVector));
                    if (FVector::DotProduct(Escape,Ship->Velocity+Lateral*.25)<0) Escape=-Escape;
                    Threat=true;break;
                }
                if (!Threat) continue;
                const FVector Goal=Position+Escape*FMath::Max(Radius*1.8,Ship->MoveSpeed*1.3);
                Evasions.Add(Ship,FEvasionOrder{Goal,Ship->Destination,Ship->bHasDestination,ElapsedSeconds+2.5});
                Ship->SetDestination(Goal);
                if (Side==0) ++AlliedEvasions; else ++EnemyEvasions;
                break;
            }
        }
    }
}
