#include "OVBasePresentation.h"
#include "OVCalibration.h"
#include "OVCombatSubsystem.h"
#include "OVController.h"
#include "OVDevelopmentSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

namespace
{
using namespace OVBase;
int Grade(Kind k)
{
    switch(k) {
    case Kind::Yard: case Kind::Dock:return 1;
    case Kind::YardMedium: case Kind::DockMedium:return 2;
    case Kind::YardLarge: case Kind::DockLarge:return 3;
    case Kind::YardSuperheavy: case Kind::DockSuperheavy:return 4;
    default:return 0; }
}
int Size(FName hull) { FOVHullMetadata meta; return OVCalibration::GetHullMetadata(hull,meta)?meta.SizeClass:0; }
Technology Required(FName hull)
{
    if(hull==TEXT("air_defense")||hull==TEXT("radar"))return Technology::SupportHull;
    if(hull==TEXT("carrier"))return Technology::Aviation;
    if(hull==TEXT("missile_ship"))return Technology::Strategic;
    if(hull==TEXT("battlecarrier"))return Technology::CombinedAir;
    switch(Size(hull)) {case 2:return Technology::HullMedium;case 3:return Technology::HullLarge;case 4:return Technology::HullSuperheavy;default:return Technology::None;}
}
Resources Price(FName hull) {const auto v=UOVLogisticsSubsystem::StandardHullPrice(hull);return {int(v.X),int(v.Y),int(v.Z)};}
float BuildSeconds(int size) { constexpr float t[]={0,45,120,240,480};return t[FMath::Clamp(size,0,4)]; }
float TransitSeconds(int size) {constexpr float t[]={0,5,8,12,20};return t[FMath::Clamp(size,0,4)];}
bool Yard(Kind k) {return Grade(k)>0 && Def(k).YardBay;}
int Points(Kind k,int size) {return Yard(k)&&Grade(k)==4 ? (size==4?2:1) : 1<<(size-1);}
int Capacity(Kind k) {return Yard(k)?(Grade(k)==4?2:1<<(Grade(k)-1)) : 4<<(Grade(k)-1);}
FOVBayOrder* Find(TArray<FOVBayOrder>& orders,FGuid id) {return orders.FindByPredicate([id](const auto& o){return o.Id==id;});}
}
using namespace OVBase;

int32 AOVBasePresentation::ModuleIndex(int32 stable) const
{
    for (int i=0;i<ModuleCount();++i) if(Build.Modules()[i].Id==stable)return i;
    return INDEX_NONE;
}
bool AOVBasePresentation::InService(int32 i) const
{
    if(i<0||i>=ModuleCount())return false;
    const auto id=Build.Modules()[i].Id;
    return Orders.ContainsByPredicate([id](const auto& o){return o.Facility==id || o.RefitYard==id;}) ||
           Science.ContainsByPredicate([id](const auto& o){return o.Lab==id;});
}
int32 AOVBasePresentation::MaxPoints(int32 i) const
{
    return i>=0 && i<ModuleCount() && Grade(Build.Modules()[i].Type)>0?Capacity(Build.Modules()[i].Type):0;
}
TArray<FName> AOVBasePresentation::YardHulls(int32 i) const
{
    TArray<FName> hulls;
    if(i<0||i>=ModuleCount())return hulls;
    const auto type=Build.Modules()[i].Type;
    if(!Yard(type))return hulls;
    const int grade=Grade(type);
    for(const auto hull:OVCalibration::GetHullIds()){
        const int size=Size(hull);
        if(size>0&&size<=grade&&(grade!=4||size>=3))hulls.Add(hull);
    }
    return hulls;
}
int32 AOVBasePresentation::StoredEquipmentCount() const
{
    int32 count=0;
    for(const auto& lot:Orders)count+=lot.RemovedGear.Num();
    return count;
}
int32 AOVBasePresentation::UsedPoints(int32 i) const
{
    if(i<0||i>=ModuleCount())return 0;
    const auto& m=Build.Modules()[i]; int used=0;
    for(const auto& o:Orders) if(o.Facility==m.Id || o.RefitYard==m.Id) used+=Points(m.Type,Size(o.Hull));
    return used;
}
FString AOVBasePresentation::ShipOffer(FName hull) const
{
    const auto p=Price(hull);
    return FString::Printf(TEXT("%s · %.0f초\n%d / %d / %d · %s"),*OVCalibration::GetHullName(hull),BuildSeconds(Size(hull)),p.Common,p.Advanced,p.Rare,Technologies[int(Required(hull))].Name);
}
Resources AOVBasePresentation::ResearchPrice(int32 t)
{
    constexpr Resources costs[]={{},{800,100,0},{2500,400,0},{8000,1800,200},{1200,200,0},{4000,700,50},{10000,2200,300},{350,60,0},{800,150,10},{900,150,0},{3000,600,50},{400,80,0},{2500,500,40},{8000,1800,250}};
    return t>0&&t<UE_ARRAY_COUNT(costs)?costs[t]:Resources{};
}
float AOVBasePresentation::ResearchSeconds(int32 t)
{
    constexpr float seconds[]={0,240,360,600,300,480,720,90,180,240,360,120,360,600};
    return t>0&&t<UE_ARRAY_COUNT(seconds)?seconds[t]:0;
}
bool AOVBasePresentation::QueueResearch(int32 t)
{
    if(t<1||t>=int(Technology::Count))return Reject(TEXT("지원하지 않는 기술"));
    if(Build.HasTechnology(Technology(t))||Science.ContainsByPredicate([t](const auto& j){return j.Tech==t;}))return Reject(TEXT("이미 보유 또는 연구 대기 중"));
    const auto& rule=Technologies[t];
    if(Build.CoreTier()<rule.CoreTier||!Build.HasTechnology(rule.Prior))return Reject(TEXT("선행 기술 또는 코어 등급 부족"));
    if ((Technology(t)==Technology::Strategic && !Build.HasTechnology(Technology::Missile)) ||
        (Technology(t)==Technology::CombinedAir && !Build.HasTechnology(Technology::Aviation)))return Reject(TEXT("추가 선행 기술 필요"));
    int lab=INDEX_NONE, shortest=MAX_int32;
    for(int i=1;i<ModuleCount();++i){
        const auto& m=Build.Modules()[i];
        if(m.Type!=Kind::Research||m.Level<rule.CoreTier||m.Remaining>0||!IsModuleConnected(i))continue;
        int count=0;for(const auto& j:Science)if(j.Lab==m.Id)++count;
        if(count<shortest){lab=i;shortest=count;}
    }
    if(lab<0)return Reject(TEXT("연결된 해당 등급 연구소 필요"));
    if(!Build.Pay(ResearchPrice(t)))return Reject(TEXT("연구 자원 부족"));
    Science.Add({t,Build.Modules()[lab].Id,ResearchSeconds(t)});
    OperationMessage=TEXT("연구 접수");return true;
}
bool AOVBasePresentation::OrderHull(int32 i,FName hull)
{
    if(i<0||i>=ModuleCount())return Reject(TEXT("조선소 선택 필요"));
    const auto& m=Build.Modules()[i]; const int size=Size(hull),grade=Grade(m.Type);
    if(!Yard(m.Type)||!size||size>grade||(grade==4&&size<3))return Reject(TEXT("조선소가 지원하지 않는 체급"));
    if(m.Remaining>0||!IsModuleConnected(i))return Reject(TEXT("조선소 공사 중 또는 단절"));
    if(!Build.HasTechnology(Required(hull)))return Reject(TEXT("함체 기술 필요"));
    const int points=Points(m.Type,size);
    if(UsedPoints(i)+points>Capacity(m.Type))return Reject(TEXT("빈 작업칸 부족"));
    float offset=0;
    if(!FitBerth(i,hull,offset))return Reject(TEXT("함체가 들어갈 빈 베이 공간 부족"));
    if(!Build.Pay(Price(hull)))return Reject(TEXT("건조 자원 부족"));
    auto& o=Orders.AddDefaulted_GetRef();o.Id=FGuid::NewGuid();o.Hull=hull;o.Facility=m.Id;o.LateralCm=offset;o.Total=o.Time=BuildSeconds(size);
    OperationMessage=TEXT("건조 시작");return true;
}
FGuid AOVBasePresentation::VesselId(AOVShip* s) const { return IsValid(s)?s->BaseVesselId:FGuid(); }
AOVShip* AOVBasePresentation::FindVessel(FGuid id) const {for(const auto& o:Orders)if(o.Id==id)return o.Ship.Get();return nullptr;}
int32 AOVBasePresentation::VesselPhase(FGuid id) const {for(const auto& o:Orders)if(o.Id==id)return int(o.Phase);return INDEX_NONE;}

void AOVBasePresentation::BayPath(int32 i,FVector& inside,FVector& outside,float offset) const
{
    const auto k=Build.Modules()[i].Type;
    FVector berth,entry;
    switch(k){
    case Kind::Yard: berth={-500,0,1570};entry={-12000,0,1570};break;
    case Kind::Dock: berth={0,0,2240};entry={0,9000,2240};break;
    case Kind::YardMedium: berth={6532,3700,1900};entry={-2800,3700,1900};break;
    case Kind::YardLarge: berth={10423,6200,2200};entry={-2575,6200,2200};break;
    case Kind::YardSuperheavy: berth={14314,10400,3700};entry={-2350,10400,3700};break;
    case Kind::DockMedium: berth={5000,3603,900};entry={5000,9433,900};break;
    case Kind::DockLarge: berth={7500,4840,1100};entry={7500,11010,1100};break;
    default: berth={11300,7265,3400};entry={11300,15374,3400};break;
    }
    const auto& tf=Meshes[i]->GetComponentTransform();
    const FVector lateral=FVector::CrossProduct(FVector::UpVector,(entry-berth).GetSafeNormal());
    inside=tf.TransformPositionNoScale(berth+lateral*offset);
    outside=tf.TransformPositionNoScale(entry+(entry-berth).GetSafeNormal()*Def(k).FlightCells*CellCm);
}
bool AOVBasePresentation::FitBerth(int32 i,FName hull,float& offset) const
{
    const auto& m=Build.Modules()[i];
    const auto half=OVCalibration::GetHullWorldHalfExtent(hull);
    const float spacing=2*half.Y+100;
    for(int candidate=0;candidate<2*Capacity(m.Type)+1;++candidate){
        const float shift=(candidate%2?1:-1)*((candidate+1)/2)*spacing;
        FVector park,mouth;BayPath(i,park,mouth,shift);
        bool overlaps=false;
        for(const auto& other:Orders){
            if(other.Facility!=m.Id)continue;
            const double needed=half.Y+OVCalibration::GetHullWorldHalfExtent(other.Hull).Y+50;
            if(FMath::Abs(other.LateralCm-shift)<needed){overlaps=true;break;}
        }
        if(overlaps)continue;
        const auto heading=(mouth-park).Rotation().Quaternion();
        FHitResult wall;
        if(Meshes[i]->SweepComponent(wall,mouth,park,heading,FCollisionShape::MakeBox(half),true))continue;
        offset=shift;return true;
    }
    return false;
}

bool AOVBasePresentation::DockVessel(int32 i,AOVShip* s)
{
    if(!IsValid(s)||!s->Systems||!s->Systems->CanMove()||s->IsHidden()||!s->bFriendly||s->HangarHost.IsValid()||s->bFTLCharging)return Reject(TEXT("접안 가능한 아군 함선 선택"));
    if(i<0||i>=ModuleCount())return Reject(TEXT("도크 선택 필요"));
    const auto& m=Build.Modules()[i]; const int size=Size(s->HullDefinitionId);
    if(!Grade(m.Type)||Yard(m.Type)||size<1||size>Grade(m.Type)||!IsModuleConnected(i)||m.Remaining>0)return Reject(TEXT("도크 체급·연결·완공 상태 확인"));
    auto* o=Find(Orders,s->BaseVesselId);
    if(o&&o->Phase!=EOVBayPhase::Field)return Reject(TEXT("이미 예약된 함선"));
    const int cost=Points(m.Type,size),cap=Capacity(m.Type);
    int slot=INDEX_NONE;
    for(int start=0;start+cost<=cap;start+=cost){
        if(!Orders.ContainsByPredicate([&](const auto& other){return other.Facility==m.Id&&start<other.Berth+Points(m.Type,Size(other.Hull))&&other.Berth<start+cost;})){slot=start;break;}
    }
    float offset=0;
    if(slot<0||!FitBerth(i,s->HullDefinitionId,offset))return Reject(TEXT("빈 접안 위치 부족"));
    if(!o){o=&Orders.AddDefaulted_GetRef();o->Id=s->BaseVesselId.IsValid()?s->BaseVesselId:FGuid::NewGuid();o->Ship=s;o->Hull=s->HullDefinitionId;}
    s->BaseVesselId=o->Id;s->TrialStation=this;
    o->Facility=m.Id;o->Berth=slot;o->LateralCm=offset;o->Phase=EOVBayPhase::Returning;
    FVector in,out;BayPath(i,in,out,o->LateralCm);
    double clearance=out.Z;
    for(const auto& mesh:Meshes)if(mesh)clearance=FMath::Max(clearance,mesh->Bounds.GetBox().Max.Z);
    clearance+=OVCalibration::GetHullWorldHalfExtent(o->Hull).Size()+CellCm;
    const FVector current=s->GetActorLocation();
    o->ApproachLeg=0;
    s->SetDestination(FVector(current.X,current.Y,FMath::Max(current.Z,clearance)));
    OperationMessage=TEXT("접안 위치 예약 · 입구로 이동");return true;
}
bool AOVBasePresentation::LaunchVessel(FGuid id)
{
    auto* o=Find(Orders,id);
    if(!o||(o->Phase!=EOVBayPhase::Ready&&o->Phase!=EOVBayPhase::Berthed))return Reject(TEXT("출항 가능한 함선이 없습니다"));
    const int i=ModuleIndex(o->Facility);
    if(!IsModuleConnected(i))return Reject(TEXT("연결 복구 후 출항 가능"));
    o->Phase=EOVBayPhase::Departing;o->Total=o->Time=TransitSeconds(Size(o->Hull));OperationMessage=TEXT("출항 요청");return true;
}

FVector AOVBasePresentation::ClipTravel(AOVShip& s,FVector from,FVector to,const FQuat& rotation) const
{
    if(from.Equals(to,.001))return to;
    const FVector half=OVCalibration::GetHullWorldHalfExtent(s.HullDefinitionId).ComponentMax(FVector(1));
    const auto shape=FCollisionShape::MakeBox(half);
    float fraction=1;
    for(int i=0;i<Meshes.Num();++i){
        auto* mesh=Meshes[i].Get(); if(!mesh||!Completed(Build.Modules()[i]))continue;
        FHitResult hit;
        if(mesh->SweepComponent(hit,from,to,rotation,shape,true)){
            if(hit.bStartPenetrating && FVector::DotProduct(to-from,hit.Normal)>0)continue;
            fraction=FMath::Min(fraction,hit.Time);
        } else if(!HasBay(Build.Modules()[i].Type)){
            // Imported render-only hulls may have no physics body. Their reserved solid
            // volume still blocks flight. Bays retain the tested open triangle interior.
            const auto& m=Build.Modules()[i];const auto d=Dimensions(m.Type);
            const FTransform grid(FRotator(0,m.Turns*90,0),CellCenter(m.Origin));
            const FVector lo(-CellCm*.5), hi=FVector(d.X,d.Y,d.Z)*CellCm+lo;
            const auto a=grid.InverseTransformPosition(from),b=grid.InverseTransformPosition(to);
            const auto extent=FBox(-half,half).TransformBy(FTransform(grid.GetRotation().Inverse()*rotation)).GetExtent();
            const FBox box(lo-extent,hi+extent);
            FVector where,normal;float t;
            if(!box.IsInside(a)&&FMath::LineExtentBoxIntersection(box,a,b,FVector::ZeroVector,where,normal,t))fraction=FMath::Min(fraction,t);
        }
    }
    if(fraction>=1)return to;
    s.StopMovement();s.Velocity=FVector::ZeroVector;
    return FMath::Lerp(from,to,FMath::Max(0.f,fraction-float(2/FVector::Distance(from,to))));
}

namespace
{
struct FServiceEstimate { double Work=0, Fraction=0; FString Parts; };
// Independent physical damage pools. Personnel, occupation and consumables never enter this pass.
FServiceEstimate ServiceDamage(UOVShipSystemsComponent& s,double budget=0)
{
    FServiceEstimate q;
    const double h=s.MaxHullHP;
    auto visit=[&](float& hp,float* rebuild,float max,double pool,double weight,double price,const TCHAR* name){
        if(max<=0||pool<=0)return;
        float& current=(rebuild && hp<=0)?*rebuild:hp;
        const double missing=FMath::Max(0.,double(max-current));
        if(missing<=0)return;
        q.Work+=h*weight*missing/pool;q.Fraction+=price*missing/pool;
        if(!q.Parts.Contains(name))q.Parts+=FString(name)+TEXT(" ");
        if(budget<=0)return;
        const double restored=FMath::Min(missing,budget*pool/(weight*h));
        current=FMath::Min(max,float(current+restored));budget-=restored*weight*h/pool;
        if(rebuild && hp<=0 && current>=max-KINDA_SMALL_NUMBER){hp=max;*rebuild=0;}
    };
    if(!s.Layout||h<=0||s.bWrecked)return q;
    visit(s.HullHP,nullptr,h,h,1,.30,TEXT("선체"));
    double armor=0,rooms=0,guns=0;
    for(const auto& a:s.Layout->ArmorSections)armor+=a.MaxArmor;
    for(const auto& f:s.Layout->Slots)rooms+=f.MaxFacilityHP;
    for(const auto& w:s.Layout->WeaponMounts)guns+=w.MaxHP;
    for(int i=0;i<s.ArmorSections.Num();++i)visit(s.ArmorSections[i].HP,nullptr,s.Layout->ArmorSections[i].MaxArmor,armor,.25,.08,TEXT("장갑"));
    for(int i=0;i<s.Slots.Num();++i)visit(s.Slots[i].HP,&s.Slots[i].RebuildHP,s.Layout->Slots[i].MaxFacilityHP,rooms,.35,.12,TEXT("시설"));
    for(int i=0;i<s.WeaponMountStates.Num();++i)visit(s.WeaponMountStates[i].HP,&s.WeaponMountStates[i].RebuildHP,s.Layout->WeaponMounts[i].MaxHP,guns,.15,.05,TEXT("무장"));
    return q;
}
Resources RepairPrice(FName hull,double fraction)
{
    const auto p=Price(hull);
    return {FMath::CeilToInt32(p.Common*fraction),FMath::CeilToInt32(p.Advanced*fraction),FMath::CeilToInt32(p.Rare*fraction)};
}
float RepairRate(const AOVShip& s,Kind dock){return FMath::Max(64.f,s.Systems->MaxHullHP*.001f)*(dock==Kind::DockSuperheavy?1.3f:1.f);}
}

FString AOVBasePresentation::ServiceQuote(const FOVBayOrder& o) const
{
    auto* s=o.Ship.Get();if(!s||!s->Systems)return {};
    const auto q=ServiceDamage(*s->Systems);const auto cost=RepairPrice(o.Hull,q.Fraction);
    return q.Work>0?FString::Printf(TEXT("%s · %d/%d/%d"),*q.Parts,cost.Common,cost.Advanced,cost.Rare):TEXT("손상 없음");
}
int32 AOVBasePresentation::AvailableYard(FName hull) const
{
    const int size=Size(hull);
    for(int i=1;i<ModuleCount();++i){
        const auto& m=Build.Modules()[i];
        if(!Yard(m.Type)||m.Remaining>0||!IsModuleConnected(i)||size>Grade(m.Type)||(Grade(m.Type)==4&&size<3))continue;
        if(UsedPoints(i)+Points(m.Type,size)<=Capacity(m.Type))return i;
    }
    return INDEX_NONE;
}

namespace
{
bool FindRoomSwap(const UOVShipSystemsComponent& sys,int& first,int& second)
{
    const auto& slots=sys.Layout->Slots;
    auto movable=[](EOVFacilityType t){
        return t==EOVFacilityType::LifeSupport||t==EOVFacilityType::WeaponControl||t==EOVFacilityType::Ammo||
            t==EOVFacilityType::Capacitor||t==EOVFacilityType::Radar||t==EOVFacilityType::ShieldControl;
    };
    for(int a=0;a<slots.Num();++a)for(int b=a+1;b<slots.Num();++b){
        const auto& x=slots[a];const auto& y=slots[b];
        if(!movable(x.Facility)||!movable(y.Facility)||x.Facility==y.Facility||
            !x.AllowedFacilities.Contains(y.Facility)||!y.AllowedFacilities.Contains(x.Facility))continue;
        if(sys.Slots[a].OccupyingFaction>=0||sys.Slots[b].OccupyingFaction>=0||
            sys.Slots[a].RepairCrew>0||sys.Slots[b].RepairCrew>0)continue;
        first=a;second=b;return true;
    }
    return false;
}

}

FString AOVBasePresentation::FacilityOffer(FGuid id) const
{
    auto* ship=FindVessel(id);int a,b;
    if(!ship||!ship->Systems||!FindRoomSwap(*ship->Systems,a,b))return TEXT("재배치 가능한 시설 없음");
    const auto& slots=ship->Systems->Layout->Slots;
    return FString::Printf(TEXT("%s ↔ %s"),*slots[a].SlotId.ToString(),*slots[b].SlotId.ToString());
}

bool AOVBasePresentation::RefitFacilities(FGuid id)
{
    auto* o=Find(Orders,id);auto* s=o?o->Ship.Get():nullptr;
    if(!s||o->Phase!=EOVBayPhase::Berthed||!IsModuleConnected(ModuleIndex(o->Facility)))return Reject(TEXT("연결된 도크에 정박 필요"));
    const int yard=AvailableYard(o->Hull);int a,b;
    if(yard<0||!FindRoomSwap(*s->Systems,a,b))return Reject(TEXT("호환 시설·빈 조선소 필요"));
    if(!Build.Pay({40,0,0}))return Reject(TEXT("시설 재배치 비용 부족"));
    const auto& slots=s->Systems->Layout->Slots;
    o->RefitYard=Build.Modules()[yard].Id;o->Mount=slots[a].SlotId;o->Equipment=slots[b].SlotId;
    o->Phase=EOVBayPhase::Refitting;o->Time=o->Total=15;
    OperationMessage=TEXT("시설 재배치 시작");return true;
}
bool AOVBasePresentation::ServiceVessel(FGuid id,bool refit)
{
    auto* o=Find(Orders,id);auto* s=o?o->Ship.Get():nullptr;
    if(!s||!s->Systems||!s->Systems->Layout||s->Systems->bWrecked||o->Phase!=EOVBayPhase::Berthed)return Reject(TEXT("정박 중인 함선 필요"));
    const int i=ModuleIndex(o->Facility);
    if(!IsModuleConnected(i))return Reject(TEXT("도크 연결 필요"));
    if(refit){
        const int yard=AvailableYard(o->Hull);
        if(yard<0)return Reject(TEXT("대응 체급 조선소 작업칸 필요"));
        auto* combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
        const auto state=combat->GetShipState(s);
        if(!combat->Settings||state.Mounts.IsEmpty())return Reject(TEXT("교체 가능한 무장 없음"));
        // Offer a same-family, same-tier alternative. Existing supply rooms stay valid.
        for(const auto& m:state.Mounts){
            const auto* def=s->Systems->Layout->WeaponMounts.FindByPredicate([&](const auto& a){return a.WeaponId==m.MountId;});
            if(!def||!def->RequiredProfileId.IsNone())continue;
            if(m.Profile.Family==EOVWeaponType::Missile&&!Build.HasTechnology(Technology::Missile))continue;
            for(const auto& profile:combat->Settings->Weapons){
                if(profile.ProfileId==m.Profile.ProfileId||profile.Tier!=m.Profile.Tier||profile.Family!=m.Profile.Family)continue;
                TArray<FString> errors;
                if(!s->Systems->Layout->ValidateLayout(errors))return Reject(TEXT("함선 시설 배치 확인 필요"));
                if(!Build.Pay({40,0,0}))return Reject(TEXT("개조 일반 자원 40 필요"));
                o->RefitYard=Build.Modules()[yard].Id;o->Mount=m.MountId;o->Equipment=profile.ProfileId;o->Phase=EOVBayPhase::Refitting;o->Time=o->Total=15;
                OperationMessage=TEXT("장비 교체 시작");return true;
            }
        }
        return Reject(TEXT("동일 계열·티어 교체 장비 없음"));
    }
    const auto q=ServiceDamage(*s->Systems);
    if(q.Work<=0)return Reject(TEXT("수리할 물리 손상 없음"));
    if(!Build.Pay(RepairPrice(o->Hull,q.Fraction)))return Reject(TEXT("수리 자원 부족"));
    o->Phase=EOVBayPhase::Repairing;o->Time=o->Total=q.Work/RepairRate(*s,Build.Modules()[i].Type);
    OperationMessage=TEXT("수리 시작 · ")+q.Parts;return true;
}
bool AOVBasePresentation::PrepareOperationsTrial()
{
    if(ModuleCount()!=1||!Orders.IsEmpty()||!Science.IsEmpty())return Reject(TEXT("빈 시험장에서만 운영 편성 준비 가능"));
    auto* pc=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(!pc||pc->Ships.Num()!=1)return Reject(TEXT("초기 편성 필요"));
    const auto before=Build;
    const auto start=pc->Ships[0]->GetActorLocation();
    pc->Ships[0]->SetActorLocation(GetActorLocation()+FVector(22000,18000,2000));pc->Ships[0]->StopMovement();
    Build.SetTestAccounting(false);
    for(const auto& m:std::initializer_list<Module>{{Kind::Corridor,{0,3,0},1},{Kind::Yard,{-3,4,0}},{Kind::Corridor,{3,0,0}},{Kind::Dock,{4,-1,0}},{Kind::Corridor,{-3,0,0},2},{Kind::Research,{-4,0,0},2}}){
        if(!ReservePlan({m})){Build=before;RefreshMeshes();pc->Ships[0]->SetActorLocation(start);return Reject(TEXT("시험 편성 배치 실패"));}
        Build.Advance(300);
    }
    Build.SetTestAccounting(before.Charges());Build.FillTestStock();RefreshMeshes(); OperationMessage=TEXT("조선소·도크·연구소 준비");return true;
}
void AOVBasePresentation::AdvanceOperations(float seconds)
{
    if(seconds<=0||!FMath::IsFinite(seconds))return;
    TMap<int32,float> remaining;
    for(int n=0;n<Science.Num();){
        auto& j=Science[n];const int i=ModuleIndex(j.Lab);
        if(!IsModuleConnected(i)||Build.Modules()[i].Remaining>0){++n;continue;}
        auto& budget=remaining.FindOrAdd(j.Lab,seconds);const float step=FMath::Min(budget,j.Time);
        budget-=step;j.Time-=step;
        if(j.Time>0){++n;continue;}
        Build.UnlockTestTechnology(Technology(j.Tech));Science.RemoveAt(n);
    }
    auto* pc=Cast<AOVController>(GetWorld()->GetFirstPlayerController());if(!pc)return;
    for(auto& o:Orders){
        const int i=ModuleIndex(o.Facility);if(o.Phase==EOVBayPhase::Field)continue;
        if(!IsModuleConnected(i)||(o.RefitYard && !IsModuleConnected(ModuleIndex(o.RefitYard)))){o.Wait=TEXT("코어 연결 대기");continue;}
        o.Wait.Reset();
        auto* s=o.Ship.Get();FVector in,out;BayPath(i,in,out,o.LateralCm);const auto dir=(out-in).GetSafeNormal();
        if(o.Phase==EOVBayPhase::Building){
            o.Time=FMath::Max(0.f,o.Time-seconds);if(o.Time>0)continue;
            auto result=GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>()->SpawnTrialShips(o.Hull,0,1,in,0,0,0,false,false,true,true);
            if(result.IsEmpty()){o.Wait=TEXT("베이 공간 대기");continue;}
            s=result[0];s->BaseVesselId=o.Id;s->TrialStation=this;s->bUnderBaseControl=true;
            s->SetActorHiddenInGame(true);s->SetActorEnableCollision(false);o.Ship=s;o.Phase=EOVBayPhase::Ready;continue;
        }
        if(!s||s->Systems->bWrecked){o.Wait=TEXT("함선 손실");continue;}
        if(o.Phase==EOVBayPhase::Returning){
            if(o.ApproachLeg<2 && FVector::Dist(s->GetActorLocation(),s->Destination)<180){
                ++o.ApproachLeg;
                s->SetDestination(o.ApproachLeg==1?FVector(out.X,out.Y,s->GetActorLocation().Z):out);
            }
            if(o.ApproachLeg<2 || FVector::Dist(s->GetActorLocation(),out)>180){o.Wait=TEXT("입구 접근 중");continue;}
            s->StopMovement();s->Velocity=FVector::ZeroVector;s->bUnderBaseControl=true;
            o.Phase=EOVBayPhase::Entering;o.Total=o.Time=TransitSeconds(Size(o.Hull));
        }
        if(o.Phase==EOVBayPhase::Departing||o.Phase==EOVBayPhase::Entering){
            const bool depart=o.Phase==EOVBayPhase::Departing;
            const float next=FMath::Max(0.f,o.Time-seconds);
            const FVector pos=FMath::Lerp(depart?in:out,depart?out:in,1.-next/o.Total);
            bool blocked=false;
            for(AOVShip* other:pc->Ships){
                if(!IsValid(other)||other==s||other->IsHidden())continue;
                const FVector closest=FMath::ClosestPointOnSegment(other->GetActorLocation(),s->GetActorLocation(),pos);
                const double radius=OVCalibration::GetHullWorldHalfExtent(other->HullDefinitionId).Size()+OVCalibration::GetHullWorldHalfExtent(o.Hull).Size();
                if(FVector::DistSquared(other->GetActorLocation(),closest)<radius*radius){blocked=true;break;}
            }
            if(!blocked){
                FHitResult wall;
                const auto shape=FCollisionShape::MakeBox(OVCalibration::GetHullWorldHalfExtent(o.Hull));
                blocked=Meshes[i]->SweepComponent(wall,s->GetActorLocation(),pos,dir.Rotation().Quaternion(),shape,true);
            }
            if(blocked){o.Wait=TEXT("출입 공간 차단 · 입구와 앞 함선 확인");continue;}
            s->SetActorHiddenInGame(false);s->SetActorLocationAndRotation(pos,dir.Rotation());
            o.Time=next;if(next>0)continue;
            s->SetActorHiddenInGame(!depart);s->SetActorEnableCollision(depart);s->bUnderBaseControl=!depart;
            if(depart){o.Facility=0;o.Phase=EOVBayPhase::Field;s->StopMovement();pc->SelectShip(s);}
            else o.Phase=EOVBayPhase::Berthed;
        }else if(o.Phase==EOVBayPhase::Repairing){
            const float rate=RepairRate(*s,Build.Modules()[i].Type);ServiceDamage(*s->Systems,seconds*rate);
            s->Systems->RefreshDerivedState();
            o.Time=ServiceDamage(*s->Systems).Work/rate;
            if(o.Time<.0001f)o.Phase=EOVBayPhase::Berthed;
        }else if(o.Phase==EOVBayPhase::Refitting){
            o.Time=FMath::Max(0.f,o.Time-seconds);if(o.Time>0)continue;
            const auto* layout=s->Systems->Layout.Get();
            const int a=layout->GetSlotIndex(o.Mount),b=layout->GetSlotIndex(o.Equipment);
            if(a>=0&&b>=0){
                if(s->Systems->SwapDesignRooms(a,b)){o.RefitYard=0;o.Phase=EOVBayPhase::Berthed;}
                else o.Wait=TEXT("시설 배치 검증 실패");
                continue;
            }
            auto* combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();const auto st=combat->GetShipState(s);
            const auto* old=st.Mounts.FindByPredicate([&](const auto& m){return m.MountId==o.Mount;});
            if(old&&combat->SetWeaponProfile(s,o.Mount,o.Equipment)){
                // Keep the donor order as provenance, but issue recovered parts station-wide.
                for(auto& lot:Orders)if(lot.RemovedGear.RemoveSingle(o.Equipment)>0)break;
                o.RemovedGear.Add(old->Profile.ProfileId);
                o.RefitYard=0;o.Phase=EOVBayPhase::Berthed;
            }
            else o.Wait=TEXT("장착 조건 미충족");
        }
    }
}

TArray<FGuid> AOVBasePresentation::FacilityOrders(int32 i) const
{
    TArray<FGuid> result;if(i<0||i>=ModuleCount())return result;
    for(const auto& o:Orders)if(o.Facility==Build.Modules()[i].Id)result.Add(o.Id);
    return result;
}
FString AOVBasePresentation::RefitOffer(FGuid id) const
{
    auto* s=FindVessel(id);if(!s||!s->Systems||!s->Systems->Layout)return {};
    auto* combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
    if(!combat->Settings)return {};
    const auto state=combat->GetShipState(s);
    for(const auto& m:state.Mounts){
        const auto* def=s->Systems->Layout->WeaponMounts.FindByPredicate([&](const auto& a){return a.WeaponId==m.MountId;});
        if(!def||!def->RequiredProfileId.IsNone())continue;
        if(m.Profile.Family==EOVWeaponType::Missile&&!Build.HasTechnology(Technology::Missile))continue;
        for(const auto& p:combat->Settings->Weapons){
            if(p.ProfileId==m.Profile.ProfileId||p.Tier!=m.Profile.Tier||p.Family!=m.Profile.Family)continue;
            return FString::Printf(TEXT("피해 %.0f → %.0f · 주기 %.1f → %.1f초"),m.Profile.Damage,p.Damage,m.Profile.Interval,p.Interval);
        }
    }
    return TEXT("호환 장비 없음");
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVBaseBayCapacityTest,"OrbitalVanguard.Base.OperationalBerths",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOVBaseBayCapacityTest::RunTest(const FString&)
{
    using namespace OVBase;
    TestEqual(TEXT("Mixed parallel orders fill the large yard"),Points(Kind::YardLarge,2)+2*Points(Kind::YardLarge,1),Capacity(Kind::YardLarge));
    TestEqual(TEXT("Two large hulls fill the super yard"),2*Points(Kind::YardSuperheavy,3),Capacity(Kind::YardSuperheavy));
    TestEqual(TEXT("One superheavy fills the super yard"),Points(Kind::YardSuperheavy,4),Capacity(Kind::YardSuperheavy));
    TestEqual(TEXT("Standard dock holds four small reservations"),4*Points(Kind::Dock,1),Capacity(Kind::Dock));
    TestTrue(TEXT("Larger hulls keep their own build clock"),BuildSeconds(2)>BuildSeconds(1));
    return true;
}
#endif
