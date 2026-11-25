#include "OVController.h"
#include "OVCombatSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVTransportSubsystem.h"

namespace
{
bool Support(FName Id)
{
    return Id==TEXT("carrier") || Id==TEXT("battlecarrier") || Id==TEXT("repair") || Id==TEXT("missile_ship");
}
double Radius(const AOVShip& Ship)
{
    return (Ship.Systems->Layout->LocalHalfExtent*Ship.GetActorScale3D()).Size();
}
}
int32 AOVController::FormationRow(FName Preset,FName Hull)
{
    if(Preset==TEXT("FormationScreen")){
        if(Hull==TEXT("corvette"))return 2;
        if(Hull==TEXT("battleship") || Hull==TEXT("dreadnought") || Hull==TEXT("cruiser"))return 1;
        return Support(Hull)?-1:0;
    }
    if(Preset==TEXT("FormationCarrier")){
        if(Hull==TEXT("carrier") || Hull==TEXT("battlecarrier"))return 0;
        if(Hull==TEXT("air_defense") || Hull==TEXT("frigate"))return 1;
        return Support(Hull)?-1:2;
    }
    if(Hull==TEXT("transport") || Hull==TEXT("mining"))return 0;
    return Support(Hull)?-1:1;
}
FVector2D AOVController::FormationOffset(FName Preset,int32 Row,int32 Index,int32 Count)
{
    if (Preset==TEXT("FormationCarrier") && Row==1 && Count>0)
    {
        const double Angle=HALF_PI+2.*PI*Index/Count;
        const double Radius=FMath::Max(1.,Count/(2.*PI));
        return FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius;
    }
    if (Preset==TEXT("FormationConvoy") && Row==1)
        return FVector2D((Index%2==0?1.:-1.)*(1.+Index/4),Index%4<2?-1.4:1.4);
    return FVector2D(Row,Index-(Count-1)*.5);
}
void AOVController::CloseFormation()
{
    bFormationMenu=false;
    FormationKind=NAME_None;
    FormationPreview.Reset();
}
bool AOVController::CanForm(AOVShip* Ship) const
{
    return IsValid(Ship) && Ship->bFriendly && Ship->Systems && Ship->Systems->CanCommand() && Ship->Systems->CanMove()
        && !Ship->bPlayerHold && !Ship->bFTLCharging && !Ship->IsHidden() && (!Combat || !Combat->IsDeparted(Ship));
}
bool AOVController::FormationAction(FName ActionName)
{
    if(ActionName==TEXT("FormationClose")){ CloseFormation();return true; }
    if(ActionName==TEXT("FormationMenu")){
        const bool Open=!bFormationMenu && Selection.Num()>1;
        CloseFormation(); bFormationMenu=Open; return true;
    }
    if(ActionName==TEXT("FormationApply")){
        int32 Moved=0;
        for(const auto& Entry:FormationPreview){
            auto* S=Entry.Key.Get();
            if(!CanForm(S) || !Selection.Contains(S))continue;
            GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->CancelFieldOrder(S);
            if(Transport) Transport->SuspendShuttle(S);
            S->SetAutomation(false,S->IsAttackAutomatic());
            S->SetDestination(Entry.Value); ++Moved;
        }
        LastAction=FString::Printf(TEXT("진형 이동 %d척"),Moved);
        CloseFormation(); return true;
    }
    if(ActionName!=TEXT("FormationScreen") && ActionName!=TEXT("FormationCarrier") && ActionName!=TEXT("FormationConvoy"))return false;
    if(!bFormationMenu || !Selected)return true;
    FormationKind=ActionName;
    FormationPreview.Reset();
    TArray<AOVShip*> Group;
    FVector Center=FVector::ZeroVector;
    double Spacing=2200;
    for(AOVShip* S:Selection) if(CanForm(S)){
        Group.Add(S); Center+=S->GetActorLocation(); Spacing=FMath::Max(Spacing,2.15*Radius(*S)+600.);
    }
    if(Group.IsEmpty()) {LastAction=TEXT("정지·충전 중인 함선은 위치를 유지합니다"); return true;}
    Center/=Group.Num();
    FVector Forward=Selected->GetActorForwardVector().GetSafeNormal2D();
    if(Forward.IsNearlyZero()) Forward=FVector::ForwardVector;
    const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
    int32 Totals[4]={}, Used[4]={};
    for(auto* S:Group)++Totals[FormationRow(ActionName,S->HullDefinitionId)+1];
    for(auto* S:Group){
        const int32 R=FormationRow(ActionName,S->HullDefinitionId),Idx=Used[R+1]++;
        const FVector2D Offset=FormationOffset(ActionName,R,Idx,Totals[R+1]);
        const FVector Point=Center+Spacing*(Offset.X*Forward+Offset.Y*Right);
        bool Placed=false;
        for(int32 Attempt=0;Attempt<=Group.Num()+Ships.Num();++Attempt){
            const FVector Candidate=Point+Right*(Spacing*Attempt);
            if(FMath::Abs(Candidate.X)>CommandXY() || FMath::Abs(Candidate.Y)>CommandXY() || Candidate.Z<CommandMinZ() || Candidate.Z>CommandMaxZ())continue;
            bool Clear=true;
            for(AOVShip* Other:Ships) if(IsValid(Other) && Other!=S && Other->Systems && Other->Systems->Layout && !Other->IsHidden()){
                const FVector* Reserved=FormationPreview.Find(Other);
                if(!Reserved && Group.Contains(Other))continue;
                if(FVector::Dist(Candidate,Reserved?*Reserved:Other->GetActorLocation())<Radius(*S)+Radius(*Other)+300){Clear=false;break;}
            }
            if(Clear){ FormationPreview.Add(S,Candidate); Placed=true; break; }
        }
        if(!Placed){ FormationPreview.Reset();LastAction=TEXT("진형 공간 부족");return true; }
    }
    LastAction=FString::Printf(TEXT("진형 미리보기 %d척 · 적용을 눌러 이동"),FormationPreview.Num());
    return true;
}
