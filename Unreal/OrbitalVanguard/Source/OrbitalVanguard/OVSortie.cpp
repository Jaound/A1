#include "OVPlayScenarioSubsystem.h"
#include "OVCalibration.h"
#include "OVController.h"
#include "OVHUD.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVInteriorSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"

namespace {
const FOVPlaySceneDefinition* BudgetMission(FName id){
 return UOVPlayScenarioSubsystem::Definitions().FindByPredicate([&](const auto& d){return d.Id==id && d.Kind==EOVPlaySceneKind::Fleet && d.Id.ToString().StartsWith(TEXT("Fleet"));});
}
int Cost(FName id){FOVHullMetadata m;return OVCalibration::GetHullMetadata(id,m)?m.FleetCost:0;}
int CrewLimit(FName id){FOVHullMetadata m;OVCalibration::GetHullMetadata(id,m);return id=="assault"?4:m.SizeClass==4?6:m.SizeClass==3?4:m.SizeClass==2?2:1;}
bool CanDuty(const FOVHeroDefinition* hero,EOVFacilityType room){
 return hero && room!=EOVFacilityType::None && (hero->Role!=EOVInteriorRole::Ranged||room==EOVFacilityType::WeaponControl) && (hero->Role!=EOVInteriorRole::Commander||room==EOVFacilityType::Bridge);
}
using ESortieFacility = EOVFacilityType;
const FName PDBeam(TEXT("PD_Beam"));
const FName PDKinetic(TEXT("PD_Kinetic"));

FString RoomProblem(const UShipLayoutDataAsset& layout,const FOVSlotDefinition& slot,ESortieFacility choice)
{
    if (choice==ESortieFacility::None) return {};
    if (slot.SlotId==layout.CoreSlotId && choice!=ESortieFacility::Core) return TEXT("중앙 고정 슬롯에는 코어를 배치하세요.");
    if (choice==ESortieFacility::Core && slot.SlotId!=layout.CoreSlotId) return TEXT("코어를 중앙 고정 슬롯에 배치하세요.");
    if (choice==ESortieFacility::Bridge && !layout.BridgeAllowedSlots.Contains(slot.SlotId)) return TEXT("함교를 함교 허용 슬롯에 배치하세요.");
    if (choice==ESortieFacility::Hangar && layout.MaxHangarInstallations==0) return TEXT("이 함체에는 격납고를 설치할 수 없습니다.");
    if (choice==ESortieFacility::FighterControl && !layout.bAllowAircraftOperations) return TEXT("함재기 통제실은 항공모함 계열에서 사용합니다.");
    if (!slot.AllowedFacilities.Contains(choice)) return TEXT("이 슬롯에는 해당 시설을 설치할 수 없습니다. 엔진은 함미의 고정 슬롯을 사용합니다.");
    return {};
}

const FOVWeaponProfile* AttackProfile(const UOVCombatSettings& settings,FName id)
{
    return settings.Weapons.FindByPredicate([id](const FOVWeaponProfile& profile){return profile.ProfileId==id;});
}
FString WeaponProblem(const FOVWeaponMountDefinition& mount,const UOVCombatSettings& settings,FName id)
{
    if (mount.Type==EOVWeaponType::Interception)
        return id==PDBeam||id==PDKinetic ? FString() : TEXT("요격 장착점에는 빔 또는 실탄 요격포를 배치하세요.");
    const auto* profile=AttackProfile(settings,id);
    if (!profile) return TEXT("이 장착점에는 공격 무장을 배치하세요.");
    if (!mount.RequiredProfileId.IsNone() && mount.RequiredProfileId!=id) return TEXT("전용 장착점에 지정된 무장을 배치하세요.");
    if (id==TEXT("D116_spinal_T4") && mount.RequiredProfileId!=id)
        return TEXT("거대 함포는 전용 장착점에 배치하세요.");
    if (profile->Tier!=FMath::RoundToInt(mount.Tier)) return FString::Printf(TEXT("이 장착점은 T%d 무장을 사용합니다."),FMath::RoundToInt(mount.Tier));
    if (profile->Family!=mount.Type && (mount.AuthoredSlotPurpose!=TEXT("General") || profile->MountMaxHP<=0))
        return TEXT("이 장착점에는 해당 계열의 무장이 맞지 않습니다.");
    return {};
}

// Build a separate layout: the physical sockets on the editing template stay available even after removing a weapon.
UShipLayoutDataAsset* DesignLayout(AOVShip& ship,const FOVSortieHull& hull,const UOVCombatSettings& settings,FString& reason)
{
    reason.Reset();
    const UShipLayoutDataAsset& source=*ship.Systems->Layout;
    UShipLayoutDataAsset* result=DuplicateObject<UShipLayoutDataAsset>(&source,&ship);
    TMap<ESortieFacility,int32> counts;
    for (auto& room:result->Slots)
    {
        const auto choice=hull.Rooms.FindRef(room.SlotId);
        reason=RoomProblem(source,room,choice);
        if (!reason.IsEmpty()) return nullptr;
        if (room.Facility!=choice)
        {
            const auto* exemplar=source.Slots.FindByPredicate([choice](const auto& s){return s.Facility==choice;});
            room.Facility=choice;
            room.MaxFacilityHP=exemplar?exemplar->MaxFacilityHP:choice==ESortieFacility::None?0.f:100.f;
            room.RequiredCrew=exemplar?exemplar->RequiredCrew:choice==ESortieFacility::None||choice==ESortieFacility::Core||choice==ESortieFacility::Security?0.f:10.f;
        }
        ++counts.FindOrAdd(choice);
    }
    if (counts.FindRef(ESortieFacility::Core)!=1) reason=TEXT("중앙 고정 슬롯에 코어 1개를 배치하세요.");
    else if (counts.FindRef(ESortieFacility::Bridge)!=1) reason=TEXT("함교 허용 슬롯에 함교 1개를 배치하세요.");
    else if (!counts.Contains(ESortieFacility::Engine)) reason=TEXT("함미 고정 슬롯에 엔진을 배치하세요.");
    else if (!counts.Contains(ESortieFacility::LifeSupport)) reason=TEXT("생명 유지 시설을 배치하세요.");
    else if (source.MaxHangarInstallations>=0 && counts.FindRef(ESortieFacility::Hangar)>source.MaxHangarInstallations) reason=TEXT("함체의 격납고 설치 한도를 초과했습니다.");
    else if (source.bAllowAircraftOperations && (!counts.Contains(ESortieFacility::Hangar)||!counts.Contains(ESortieFacility::FighterControl)))
        reason=TEXT("함재기 운용에 필요한 격납고와 함재기 통제실을 배치하세요.");
    else if ((source.bAllowAircraftOperations||hull.Entry.Hull=="repair"||hull.Entry.Hull=="mining"||hull.Entry.Hull=="transport") && !counts.Contains(ESortieFacility::ResourceStorage))
        reason=TEXT("이 함종의 보급·운송에 필요한 자원 적재실을 배치하세요.");
    else if (hull.Entry.Hull=="radar" && !counts.Contains(ESortieFacility::Radar)) reason=TEXT("레이더함에 레이더 시설을 배치하세요.");
    else if (hull.Entry.Hull=="air_defense" && !counts.Contains(ESortieFacility::Interception)) reason=TEXT("방공함에 요격 제어 시설을 배치하세요.");
    else if (hull.Entry.Hull=="assault" && !counts.Contains(ESortieFacility::Boarding)) reason=TEXT("강습선에 강습 시설을 배치하세요.");
    if (!reason.IsEmpty()) return nullptr;
    result->WeaponMounts.Reset();
    result->BeamSlots=result->MissileSlots=result->CannonSlots=0;
    for (const auto& socket:source.WeaponMounts)
    {
        const FName id=hull.Weapons.FindRef(socket.WeaponId);
        if (id.IsNone()) continue;
        reason=WeaponProblem(socket,settings,id);
        if (!reason.IsEmpty()) return nullptr;
        auto& mount=result->WeaponMounts.Add_GetRef(socket);
        if (socket.Type==EOVWeaponType::Interception) {mount.PresentationProfile=id;continue;}
        const auto& profile=*AttackProfile(settings,id);
        mount.Type=profile.Family;
        if (profile.MountMaxHP>0) mount.MaxHP=profile.MountMaxHP;
        if (mount.Type!=socket.Type) mount.PresentationProfile=NAME_None;
        if (mount.Type==EOVWeaponType::Beam) ++result->BeamSlots;
        if (mount.Type==EOVWeaponType::Cannon) ++result->CannonSlots;
        if (mount.Type==EOVWeaponType::Missile) ++result->MissileSlots;
    }
    if (result->BeamSlots+result->CannonSlots+result->MissileSlots>0 && !counts.Contains(ESortieFacility::WeaponControl)) reason=TEXT("공격 무장을 쓰려면 무기 제어실을 배치하세요.");
    else if (result->BeamSlots && !counts.Contains(ESortieFacility::Capacitor)) reason=TEXT("빔 무장을 쓰려면 축전실을 배치하세요.");
    else if (result->CannonSlots && !counts.Contains(ESortieFacility::Ammo)) reason=TEXT("함포를 쓰려면 탄약고를 배치하세요.");
    else if (result->MissileSlots && !counts.Contains(ESortieFacility::MissileStorage)) reason=TEXT("미사일을 쓰려면 미사일 저장소를 배치하세요.");
    if (!reason.IsEmpty()) return nullptr;
    TArray<FString> errors;
    if (!result->ValidateLayout(errors))
    {
        UE_LOG(LogTemp,Warning,TEXT("Sortie layout %s: %s"),*hull.Entry.Hull.ToString(),*FString::Join(errors,TEXT(", ")));
        reason=TEXT("배치한 시설과 함체의 슬롯 조건을 확인하세요.");return nullptr;
    }
    return result;
}
}
bool UOVPlayScenarioSubsystem::IsBudgetMission(FName id){return BudgetMission(id)!=nullptr;}
int32 UOVPlayScenarioSubsystem::SortieBudget()const{auto* def=BudgetMission(Draft.Mission);return def?def->FleetBudget:0;}
int32 UOVPlayScenarioSubsystem::SortieHeroLimit()const{return FMath::Min(15,FMath::DivideAndRoundUp(SortieBudget(),20));}
int32 UOVPlayScenarioSubsystem::SortieCost()const{int total=0;for(const auto& h:Draft.Hulls)total+=Cost(h.Entry.Hull);return total;}
TArray<FName> UOVPlayScenarioSubsystem::SortieRoster()const{
 TArray<FName> ids;if(auto* gi=GetWorld()->GetGameInstance())for(const auto& h:gi->GetSubsystem<UOVHeroSubsystem>()->Definitions)ids.Add(h.Id);return ids;
}
void UOVPlayScenarioSubsystem::ReleaseSortiePreview(){
 if(auto* actor=SortiePreview.Get()){
    if(auto* c=GetWorld()->GetSubsystem<UOVCombatSubsystem>())c->Ships.RemoveAll([&](const auto& state){return state.Ship==actor;});
    actor->Destroy();
 }
 SortiePreview.Reset();PreviewKey=0;
}
bool UOVPlayScenarioSubsystem::FitSortieHull(AOVShip& s,const FOVSortieHull& h)const{
 auto* combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
 if(!s.Systems->Layout || !combat || !combat->Settings)return false;
 FString reason;
 auto* layout=DesignLayout(s,h,*combat->Settings,reason);
 if(!layout)return false;
 const auto state=combat->GetShipState(&s);
 combat->Ships.RemoveAll([&](const auto& record){return record.Ship==&s;});
 s.Systems->Initialize(layout);
 combat->RegisterShip(&s,state.RadarTier,state.bRadarSupport,state.WeaponTierCap);
 for(const auto& mount:layout->WeaponMounts)
    if(mount.Type!=EOVWeaponType::Interception && !combat->SetWeaponProfile(&s,mount.WeaponId,h.Weapons.FindRef(mount.WeaponId)))return false;
 return true;
}
EOVFacilityType UOVPlayScenarioSubsystem::SortieRoomAt(int32 i)const
{
 const auto* s=SortiePreview.Get();
 if(!s||!Draft.Hulls.IsValidIndex(SortieSelected)||PreviewKey!=Draft.Hulls[SortieSelected].Key||!s->Systems->Layout->Slots.IsValidIndex(i))return ESortieFacility::None;
 return Draft.Hulls[SortieSelected].Rooms.FindRef(s->Systems->Layout->Slots[i].SlotId);
}
FName UOVPlayScenarioSubsystem::SortieWeaponAt(int32 i)const
{
 const auto* s=SortiePreview.Get();
 if(!s||!Draft.Hulls.IsValidIndex(SortieSelected)||PreviewKey!=Draft.Hulls[SortieSelected].Key||!s->Systems->Layout->WeaponMounts.IsValidIndex(i))return NAME_None;
 return Draft.Hulls[SortieSelected].Weapons.FindRef(s->Systems->Layout->WeaponMounts[i].WeaponId);
}
AOVShip* UOVPlayScenarioSubsystem::InspectSortie(int32 i){
 if(ActiveId!="SortiePrep"||!Draft.Hulls.IsValidIndex(i)||!Owner.IsValid())return nullptr;
 auto& h=Draft.Hulls[i];if(PreviewKey==h.Key && SortiePreview.IsValid())return SortiePreview.Get();
 ReleaseSortiePreview();auto* ship=GetWorld()->SpawnActor<AOVShip>();if(!ship)return nullptr;
 SortiePreview=ship;
 if(!OVCalibration::ConfigureHull(*Owner->Combat,*ship,h.Entry.Hull,true,FVector(0,0,-900000),OVCalibration::GetHullGeometryScale(h.Entry.Hull),h.Entry.AircraftBays)){ReleaseSortiePreview();return nullptr;}
 ship->SetActorEnableCollision(false);ship->SetActorHiddenInGame(true);PreviewKey=h.Key;return ship;
}
bool UOVPlayScenarioSubsystem::OpenSortie(AOVController* pc,FName mission,bool edit){
 if(!pc||!BudgetMission(mission))return false;
 auto copy=edit && LastSortie.Mission==mission?LastSortie:FOVSortieDraft{};
 if(!Start(*pc,TEXT("SortiePrep")))return false;
 Draft=MoveTemp(copy);Draft.Mission=mission;SortieStage=0;SortieSelected=SortieHero=SortieRoom=SortieMount=SortieSizeFilter=SortieRoleFilter=0;SortieDeck=1;bSortieExternal=false;bSortieDetails=false;
 SyncEnvironmentHazard(*pc,Draft.Map,FVector(0,0,2000));pc->bSimulationPaused=true;SortieFeedback.Reset();return true;
}
void UOVPlayScenarioSubsystem::RecommendSortie(){
 const auto* def=BudgetMission(Draft.Mission);if(!def)return;
 ReleaseSortiePreview();Draft.Hulls.Reset();
 for(auto& hero:Draft.Heroes){hero.HullKey=0;hero.StartRoom=hero.DutyRoom=NAME_None;hero.bBoard=false;}
 for(const auto& entry:def->Fleet)for(int n=0;n<entry.Count;++n){FOVSortieHull h;h.Key=Draft.NextKey++;h.Entry=entry;h.Entry.Count=1;Draft.Hulls.Add(h);}
 SortieSelected=0;AutoPlaceSortie();
}
FString UOVPlayScenarioSubsystem::PlacementProblem(int32 i,FVector p)const{
 if(!Draft.Hulls.IsValidIndex(i)||p.ContainsNaN())return TEXT("배치할 함선을 선택하세요.");
 const double r=OVCalibration::GetHullWorldHalfExtent(Draft.Hulls[i].Entry.Hull).Size();
 if(Draft.Map==EOVSpaceHazardKind::BlackHole && FVector::Dist(p,FVector(0,0,2000))<=4500+r)return TEXT("블랙홀 중심부와 겹칩니다.");
 if(p.X-r< -OVCalibration::XYLimit||p.X+r>0 || FMath::Abs(p.Y)+r>OVCalibration::XYLimit || p.Z<OVCalibration::MinZ+r || p.Z>OVCalibration::MaxZ-r)return TEXT("아군 시작 구역 안에 배치하세요.");
 if(Owner.IsValid()&&Draft.Map==EOVSpaceHazardKind::AsteroidField)for(auto a:Owner->ActiveAsteroids)if(FVector::Dist(p,FVector(a.X,a.Y,a.Z))<=a.W+r)return TEXT("소행성에 함체가 닿습니다.");
 for(int j=0;j<Draft.Hulls.Num();++j)if(j!=i&&!Draft.Hulls[j].Position.IsZero() && FVector::Dist(p,Draft.Hulls[j].Position)<r+OVCalibration::GetHullWorldHalfExtent(Draft.Hulls[j].Entry.Hull).Size()+300)return TEXT("다른 함선과 겹칩니다.");
 return {};
}
void UOVPlayScenarioSubsystem::AutoPlaceSortie(){
 for(auto& h:Draft.Hulls)h.Position=FVector::ZeroVector;
 int totals[4]={},used[4]={};double spacing=5500;
 for(const auto& h:Draft.Hulls){
    ++totals[AOVController::FormationRow(Draft.Formation,h.Entry.Hull)+1];
    spacing=FMath::Max(spacing,2.15*OVCalibration::GetHullWorldHalfExtent(h.Entry.Hull).Size()+600);
 }
 int cell=0;for(int i=0;i<Draft.Hulls.Num();++i){
    const int row=AOVController::FormationRow(Draft.Formation,Draft.Hulls[i].Entry.Hull);
    const FVector2D offset=AOVController::FormationOffset(Draft.Formation,row,used[row+1]++,totals[row+1]);
    FVector balanced(-100000+offset.X*spacing,offset.Y*spacing,2000);
    if(PlacementProblem(i,balanced).IsEmpty()){Draft.Hulls[i].Position=balanced;continue;}
    for(;cell<550;++cell){const int col=(cell/13)%8,gridRow=cell%13;
       FVector p(-58000-col*15500,(gridRow%2?1:-1)*((gridRow+1)/2)*21500,2000+(cell/104)*11500);
       if(PlacementProblem(i,p).IsEmpty()){Draft.Hulls[i].Position=p;++cell;break;}
    }
 }
}
bool UOVPlayScenarioSubsystem::SetSortiePosition(FVector point,float yaw){
 if(ActiveId!="SortiePrep"||SortieStage!=3||!FMath::IsFinite(yaw))return false;
 SortieFeedback=PlacementProblem(SortieSelected,point);if(!SortieFeedback.IsEmpty())return false;
 auto& h=Draft.Hulls[SortieSelected];h.Position=point;h.Yaw=FRotator::NormalizeAxis(yaw);return true;
}
FString UOVPlayScenarioSubsystem::SortieProblem(int32 step)const{
 if(!BudgetMission(Draft.Mission))return TEXT("함대전 임무를 선택하세요.");
 if(step>=1 && (Draft.Hulls.IsEmpty()||SortieCost()>SortieBudget()))return Draft.Hulls.IsEmpty()?TEXT("함선을 한 척 이상 추가하세요."):TEXT("함선 코스트가 예산을 넘었습니다.");
 if(step>=2)for(int i=0;i<Draft.Hulls.Num();++i)if(!Draft.Hulls[i].DesignIssue.IsEmpty())
    return FString::Printf(TEXT("%02d %s · %s"),i+1,*OVCalibration::GetHullName(Draft.Hulls[i].Entry.Hull),*Draft.Hulls[i].DesignIssue);
 if(step>=3)for(int i=0;i<Draft.Hulls.Num();++i){auto reason=PlacementProblem(i,Draft.Hulls[i].Position);if(!reason.IsEmpty())return OVCalibration::GetHullName(Draft.Hulls[i].Entry.Hull)+TEXT(" · ")+reason;}
 if(step>=4&&Draft.Heroes.Num()>SortieHeroLimit())return TEXT("영웅 배치 한도를 넘었습니다.");
 if(step>=5){
    TSet<FName> unique;TMap<int,int> counts,board;
    for(const auto& h:Draft.Heroes){
       auto* hull=Draft.Hulls.FindByPredicate([&](const auto& s){return s.Key==h.HullKey;});
       if(!hull||h.StartRoom.IsNone())return TEXT("영웅의 탑승 함선과 시작 방을 지정하세요.");
       if(unique.Contains(h.Definition))return TEXT("같은 영웅을 중복 배치할 수 없습니다.");
       unique.Add(h.Definition);
       if(++counts.FindOrAdd(h.HullKey)>CrewLimit(hull->Entry.Hull))return TEXT("함선의 영웅 정원을 넘었습니다.");
       if(h.bBoard&&++board.FindOrAdd(h.HullKey)>4)return TEXT("강습팀의 영웅 정원은 4명입니다.");
    }
 }
 return {};
}
bool UOVPlayScenarioSubsystem::ApplySortieHeroes(AOVController& pc){
 auto* hero=pc.GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();auto* interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
 for(auto& h:Draft.Heroes){int i=Draft.Hulls.IndexOfByPredicate([&](const auto& s){return s.Key==h.HullKey;});
    if(!pc.Ships.IsValidIndex(i))return false;
    const auto* definition=hero->Definition(h.Definition);if(!definition)return false;
    const FGuid id=hero->CreateRosterEntry(0,definition->Role,definition->Name,true);
    auto* record=hero->FindHero(id);if(!record)return false;
    record->DefinitionId=h.Definition;
    const int u=interior->SpawnRosterHero(pc.Ships[i],id,h.StartRoom);if(!u)return false;
    if(!h.DutyRoom.IsNone()&&!interior->SetHeroDuty(u,true,h.DutyRoom))return false;
    if(h.bBoard&&!interior->SetHeroBoardingAssignment(u,true))return false;
 }
 return true;
}
bool UOVPlayScenarioSubsystem::CommitSortie(){
 if(ActiveId!="SortiePrep"||SortieStage!=6||!Owner.IsValid())return false;
 SortieFeedback=SortieProblem(6);if(!SortieFeedback.IsEmpty())return false;
 auto* pc=Owner.Get();FOVSortieDraft snapshot=Draft;
 {TGuardValue<bool> applying(bApplyingSortie,true);if(Start(*pc,snapshot.Mission)){LastSortie=snapshot;pc->bSimulationPaused=true;return true;}}
 OpenSortie(pc,snapshot.Mission,true);Draft=MoveTemp(snapshot);SortieStage=6;SortieFeedback=TEXT("편성 적용에 실패했습니다. 장비와 영웅 배치를 확인하세요.");return false;
}
bool UOVPlayScenarioSubsystem::SortieCommand(FName a,int32 n){
 if(ActiveId!="SortiePrep")return false;
 SortieFeedback.Reset();
 auto reject=[&](const TCHAR* msg){SortieFeedback=msg;return false;};
 if(a=="Next"){SortieFeedback=SortieProblem(SortieStage);if(!SortieFeedback.IsEmpty()||SortieStage>=6)return false;++SortieStage;return true;}
 if(a=="Prev"){SortieStage=FMath::Max(0,SortieStage-1);return true;}
 if(a=="Review"&&SortieStage==6&&n>=0&&n<6){SortieStage=n;return true;}
 if(a=="Map"&&SortieStage==0&&n>=0&&n<3){const EOVSpaceHazardKind maps[]={EOVSpaceHazardKind::None,EOVSpaceHazardKind::BlackHole,EOVSpaceHazardKind::AsteroidField};Draft.Map=maps[n];SyncEnvironmentHazard(*Owner,Draft.Map,FVector(0,0,2000));return true;}
 if(a=="RecommendFleetDesign"&&SortieStage==2){
    const TArray<FOVSortieHull> previousHulls=Draft.Hulls;
    const TArray<FOVSortieHero> previousHeroes=Draft.Heroes;
    TGuardValue<int32> selection(SortieSelected,SortieSelected);
    for(int32 index=0;index<Draft.Hulls.Num();++index){
        SortieSelected=index;
        if(!SortieCommand(TEXT("Defaults"),0)){Draft.Hulls=previousHulls;Draft.Heroes=previousHeroes;return false;}
    }
    return true;
 }
 if(a=="Recommend"&&SortieStage==1){RecommendSortie();return true;}
 if(a=="Formation"&&SortieStage==3){
    const FName presets[]={TEXT("FormationScreen"),TEXT("FormationCarrier"),TEXT("FormationConvoy")};
    if(n<0||n>=UE_ARRAY_COUNT(presets))return false;
    Draft.Formation=presets[n];AutoPlaceSortie();return true;
 }
 if(a=="Auto"&&SortieStage==3){AutoPlaceSortie();return true;}
 if(a=="Ship"&&Draft.Hulls.IsValidIndex(n)){SortieSelected=n;SortieRoom=SortieMount=0;SortieDeck=1;return true;}
 if(a=="Add"&&SortieStage==1){
    auto ids=OVCalibration::GetHullIds();if(!ids.IsValidIndex(n))return false;FName id=ids[n];
    if(SortieCost()+Cost(id)>SortieBudget())return reject(TEXT("남은 함선 코스트가 부족합니다."));
    FOVSortieHull h;h.Entry={id,1,20};h.Key=Draft.NextKey++;SortieSelected=Draft.Hulls.Add(h);
    for(int i=0;i<550;++i){FVector p(-58000-((i/13)%8)*15500,(i%13-6)*21500,2000+i/104*11500);if(PlacementProblem(SortieSelected,p).IsEmpty()){Draft.Hulls.Last().Position=p;break;}}
    return true;
 }
 if(a=="Remove"&&SortieStage==1&&Draft.Hulls.IsValidIndex(n)){
    int key=Draft.Hulls[n].Key;for(auto& h:Draft.Heroes)if(h.HullKey==key){h.HullKey=0;h.StartRoom=h.DutyRoom=NAME_None;h.bBoard=false;}
    ReleaseSortiePreview();Draft.Hulls.RemoveAt(n);SortieSelected=FMath::Clamp(SortieSelected,0,FMath::Max(0,Draft.Hulls.Num()-1));return true;
 }
 if(a=="Filter"&&SortieStage==1){SortieSizeFilter=(SortieSizeFilter+1)%5;return true;}
 if(a=="Role"&&SortieStage==1){SortieRoleFilter=(SortieRoleFilter+1)%4;return true;}
 if(a=="Hero"&&SortieStage==4){
    auto ids=SortieRoster();if(!ids.IsValidIndex(n))return false;FName id=ids[n];int i=Draft.Heroes.IndexOfByPredicate([id](const auto& h){return h.Definition==id;});
    if(i!=INDEX_NONE)Draft.Heroes.RemoveAt(i);
    else{if(Draft.Heroes.Num()>=SortieHeroLimit())return reject(TEXT("이 임무의 영웅 한도에 도달했습니다."));FOVSortieHero h;h.Definition=id;Draft.Heroes.Add(h);}
    SortieHero=0;return true;
 }
 if(a=="Person"&&Draft.Heroes.IsValidIndex(n)){SortieHero=n;return true;}
 if(a=="Detail"){bSortieDetails=!bSortieDetails;return true;}
 if(!Draft.Hulls.IsValidIndex(SortieSelected))return false;
 auto& design=Draft.Hulls[SortieSelected];
 if(SortieStage!=2&&SortieStage!=5)return false;
 auto* ship=InspectSortie(SortieSelected);if(!ship)return reject(TEXT("이 함체의 기본 설계를 불러오지 못했습니다."));
 const auto& slots=ship->Systems->Layout->Slots;
 if(a=="Deck"){
    const int32 first=slots.IndexOfByPredicate([n](const auto& room){return room.DeckId==n;});
    if(first==INDEX_NONE)return false;
    if(!slots.IsValidIndex(SortieRoom)||slots[SortieRoom].DeckId!=n)SortieRoom=first;
    SortieDeck=n;return true;
 }
 if(a=="Room"&&slots.IsValidIndex(n)){SortieRoom=n;SortieDeck=slots[n].DeckId;return true;}
 if(a=="Tab"){bSortieExternal=!bSortieExternal;return true;}
 if(a=="Slot"&&ship->Systems->Layout->WeaponMounts.IsValidIndex(n)){SortieMount=n;return true;}
 if(SortieStage==2)
 {
    const auto& layout=*ship->Systems->Layout;
    const auto& mounts=layout.WeaponMounts;
    const auto& settings=*Owner->Combat->Settings;
    const bool recommendRooms=a=="RecommendRooms"||a=="Defaults";
    const bool recommendWeapons=a=="RecommendWeapons"||a=="Defaults";
    if(recommendRooms){
        design.Rooms.Reset();
        for(const auto& room:slots)if(room.Facility!=ESortieFacility::None)design.Rooms.Add(room.SlotId,room.Facility);
    }
    if(recommendWeapons){
        design.Weapons.Reset();
        for(const auto& mount:Owner->Combat->GetShipState(ship).Mounts)
            if(!mount.Profile.ProfileId.IsNone())design.Weapons.Add(mount.MountId,mount.Profile.ProfileId);
        int32 pdIndex=0;
        for(const auto& mount:mounts)if(mount.Type==EOVWeaponType::Interception)
            design.Weapons.Add(mount.WeaponId,pdIndex++%2==0?PDBeam:PDKinetic);
    }
    if(a=="SetRoom"||a=="ClearRoom"){
        if(!slots.IsValidIndex(SortieRoom)||n<0||n>int(ESortieFacility::Security))return false;
        const auto& slot=slots[SortieRoom];
        const auto choice=a=="ClearRoom"?ESortieFacility::None:static_cast<ESortieFacility>(n);
        SortieFeedback=RoomProblem(layout,slot,choice);
        if(!SortieFeedback.IsEmpty())return false;
        if(choice==ESortieFacility::None)design.Rooms.Remove(slot.SlotId);else design.Rooms.Add(slot.SlotId,choice);
    }
    else if(a=="Swap"){
        if(!slots.IsValidIndex(n)||!slots.IsValidIndex(SortieRoom)||SortieRoom==n)return false;
        const auto& source=slots[SortieRoom];const auto& target=slots[n];
        const auto left=design.Rooms.FindRef(source.SlotId),right=design.Rooms.FindRef(target.SlotId);
        if(left==right||!RoomProblem(layout,source,right).IsEmpty()||!RoomProblem(layout,target,left).IsEmpty())
            return reject(TEXT("서로 다른 시설을 양쪽에서 허용하는 슬롯끼리 바꿀 수 있습니다."));
        design.Rooms.Add(source.SlotId,right);design.Rooms.Add(target.SlotId,left);
    }
    else if(a=="Equip"||a=="EquipPD"||a=="ClearMount"){
        if(!mounts.IsValidIndex(SortieMount))return false;
        const auto& socket=mounts[SortieMount];
        if(a=="ClearMount")design.Weapons.Remove(socket.WeaponId);
        else {
            const bool pd=a=="EquipPD";
            if(pd?(n!=0&&n!=1):!settings.Weapons.IsValidIndex(n))return false;
            const FName id=pd?(n==0?PDBeam:PDKinetic):settings.Weapons[n].ProfileId;
            SortieFeedback=WeaponProblem(socket,settings,id);
            if(!SortieFeedback.IsEmpty())return false;
            design.Weapons.Add(socket.WeaponId,id);
        }
    }
    else if(!recommendRooms&&!recommendWeapons)return false;
    DesignLayout(*ship,design,settings,design.DesignIssue);
    for(auto& hero:Draft.Heroes)if(hero.HullKey==design.Key){
        if(design.Rooms.FindRef(hero.StartRoom)==ESortieFacility::None){hero.StartRoom=hero.DutyRoom=NAME_None;hero.bBoard=false;}
        else if(!hero.DutyRoom.IsNone()&&!CanDuty(Owner->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definition(hero.Definition),design.Rooms.FindRef(hero.DutyRoom)))hero.DutyRoom=NAME_None;
    }
    return true;
 }
 if((a=="Embark"||a=="Duty"||a=="Board")&&SortieStage==5&&Draft.Heroes.IsValidIndex(SortieHero)&&slots.IsValidIndex(SortieRoom)){
    auto& h=Draft.Heroes[SortieHero];const auto& room=slots[SortieRoom];
    if(SortieRoomAt(SortieRoom)==EOVFacilityType::None)return reject(TEXT("시작할 시설을 고르세요."));
    if(a!="Embark"&&h.HullKey!=design.Key)return reject(TEXT("먼저 이 함선에 시작 위치를 지정하세요."));
    if(a=="Duty"&&!CanDuty(Owner->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definition(h.Definition),SortieRoomAt(SortieRoom)))return reject(TEXT("이 영웅의 보직에 맞는 시설을 선택하세요."));
    if(a=="Embark"){
       int other=0;for(const auto& v:Draft.Heroes)if(&v!=&h && v.HullKey==design.Key)++other;
       if(other>=CrewLimit(design.Entry.Hull))return reject(TEXT("이 함선의 영웅 자리가 가득 찼습니다."));
       h.HullKey=design.Key;h.StartRoom=room.SlotId;h.DutyRoom=NAME_None;h.bBoard=false;
    }else if(a=="Duty"){
       int posts=0;for(const auto& other:Draft.Heroes)if(&other!=&h && other.HullKey==design.Key && !other.DutyRoom.IsNone())++posts;
       if(design.Entry.Hull=="assault"||posts>=int(ship->Systems->Layout->HullSizeClass))return reject(TEXT("이 함선의 지원 보직이 가득 찼습니다."));
       h.DutyRoom=room.SlotId;h.bBoard=false;
    }
    else{int count=0;for(const auto& v:Draft.Heroes)if(v.HullKey==design.Key&&v.bBoard)++count;if(!h.bBoard&&count>=4)return reject(TEXT("강습팀 정원이 가득 찼습니다."));h.bBoard=!h.bBoard;h.DutyRoom=NAME_None;}
    return true;
 }
 return false;
}

TArray<FName> UOVPlayScenarioSubsystem::SortieCatalog() const{return OVCalibration::GetHullIds();}
