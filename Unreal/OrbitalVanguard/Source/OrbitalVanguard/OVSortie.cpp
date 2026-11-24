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
 for(auto pair:h.RoomSwaps)if(!s.Systems->SwapDesignRooms(pair.X,pair.Y))return false;
 for(const auto& w:h.Weapons)if(!GetWorld()->GetSubsystem<UOVCombatSubsystem>()->SetWeaponProfile(&s,w.Key,w.Value))return false;
 TArray<FString> reasons;return s.Systems->Layout && s.Systems->Layout->ValidateLayout(reasons);
}
AOVShip* UOVPlayScenarioSubsystem::InspectSortie(int32 i){
 if(ActiveId!="SortiePrep"||!Draft.Hulls.IsValidIndex(i)||!Owner.IsValid())return nullptr;
 auto& h=Draft.Hulls[i];if(PreviewKey==h.Key && SortiePreview.IsValid())return SortiePreview.Get();
 ReleaseSortiePreview();auto* ship=GetWorld()->SpawnActor<AOVShip>();if(!ship)return nullptr;
 SortiePreview=ship;
 if(!OVCalibration::ConfigureHull(*Owner->Combat,*ship,h.Entry.Hull,true,FVector(0,0,-900000),OVCalibration::GetHullGeometryScale(h.Entry.Hull),h.Entry.AircraftBays)||!FitSortieHull(*ship,h)){ReleaseSortiePreview();return nullptr;}
 ship->SetActorEnableCollision(false);ship->SetActorHiddenInGame(true);PreviewKey=h.Key;return ship;
}
bool UOVPlayScenarioSubsystem::OpenSortie(AOVController* pc,FName mission,bool edit){
 if(!pc||!BudgetMission(mission))return false;
 auto copy=edit && LastSortie.Mission==mission?LastSortie:FOVSortieDraft{};
 if(!Start(*pc,TEXT("SortiePrep")))return false;
 Draft=MoveTemp(copy);Draft.Mission=mission;SortieStage=0;SortieSelected=SortieHero=SortieRoom=SortieMount=SortieSizeFilter=SortieRoleFilter=0;SortieDeck=1;bSortieExternal=false;bSortieDetails=false;
 if(!edit)RecommendSortie();
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
 int cell=0;for(int i=0;i<Draft.Hulls.Num();++i){
    for(;cell<550;++cell){const int col=(cell/13)%8,row=cell%13;
       FVector p(-58000-col*15500,(row%2?1:-1)*((row+1)/2)*21500,2000+(cell/104)*11500);
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
 {TGuardValue<bool> applying(bApplyingSortie,true);if(Start(*pc,snapshot.Mission)){LastSortie=snapshot;pc->bSimulationPaused=false;return true;}}
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
 if(a=="Recommend"&&SortieStage==1){RecommendSortie();return true;}
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
 if(a=="Defaults"&&SortieStage==2){design.RoomSwaps.Reset();design.Weapons.Reset();ReleaseSortiePreview();return true;}
 if(SortieStage!=2&&SortieStage!=5)return false;
 auto* ship=InspectSortie(SortieSelected);if(!ship)return reject(TEXT("이 함체의 기본 설계를 불러오지 못했습니다."));
 const auto& slots=ship->Systems->Layout->Slots;
 if(a=="Deck"){SortieDeck=n;return true;}
 if(a=="Room"&&slots.IsValidIndex(n)){SortieRoom=n;SortieDeck=slots[n].DeckId;return true;}
 if(a=="Tab"){bSortieExternal=!bSortieExternal;return true;}
 if(a=="Slot"&&ship->Systems->Layout->WeaponMounts.IsValidIndex(n)){SortieMount=n;return true;}
 if(a=="Equip"&&SortieStage==2){
    const auto& mounts=ship->Systems->Layout->WeaponMounts;const auto& profiles=Owner->Combat->Settings->Weapons;
    if(!mounts.IsValidIndex(SortieMount)||!profiles.IsValidIndex(n))return false;
    FName mount=mounts[SortieMount].WeaponId,profile=profiles[n].ProfileId;
    if(!Owner->Combat->SetWeaponProfile(ship,mount,profile))return reject(TEXT("장착할 수 없습니다. 슬롯 티어·전용 장착점·공급 시설을 확인하세요."));
    design.Weapons.Add(mount,profile);return true;
 }
 if(a=="Swap"&&SortieStage==2){
    if(!ship->Systems->SwapDesignRooms(SortieRoom,n))return reject(TEXT("고정 시설이거나 양쪽 슬롯의 허용 시설이 맞지 않습니다."));
    design.RoomSwaps.Add(FIntPoint(SortieRoom,n));
    for(auto& hero:Draft.Heroes)if(hero.HullKey==design.Key){
       const auto* layout=ship->Systems->Layout.Get();const int spawn=layout->GetSlotIndex(hero.StartRoom);
       if(spawn!=INDEX_NONE && layout->Slots[spawn].Facility==EOVFacilityType::None)hero.StartRoom=NAME_None;
       const int duty=layout->GetSlotIndex(hero.DutyRoom);
       if(duty!=INDEX_NONE && !CanDuty(Owner->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definition(hero.Definition),layout->Slots[duty].Facility))hero.DutyRoom=NAME_None;
    }
    return true;
 }
 if((a=="Embark"||a=="Duty"||a=="Board")&&SortieStage==5&&Draft.Heroes.IsValidIndex(SortieHero)&&slots.IsValidIndex(SortieRoom)){
    auto& h=Draft.Heroes[SortieHero];const auto& room=slots[SortieRoom];
    if(room.Facility==EOVFacilityType::None)return reject(TEXT("시작할 시설을 고르세요."));
    if(a!="Embark"&&h.HullKey!=design.Key)return reject(TEXT("먼저 이 함선에 시작 위치를 지정하세요."));
    if(a=="Duty"&&!CanDuty(Owner->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definition(h.Definition),room.Facility))return reject(TEXT("이 영웅의 보직에 맞는 시설을 선택하세요."));
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
