#include "OVBasePresentation.h"
#include "OVController.h"
#include "OVCalibration.h"
#include "OVCombatSubsystem.h"
#include "OVDevelopmentSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

namespace
{
constexpr int32 SaveMagic=0x4f564254;
constexpr int32 SaveVersion=2;

struct VesselSnapshot
{
    bool Present=false;
    FVector Position{},Velocity{},Destination{},Cargo{};
    FRotator Rotation{};
    bool Move=false,Hold=true,ShieldOn=false,ShieldOff=false;
    float Hull=0,Shield=0;
    TArray<FOVFacilityState> Rooms;
    TArray<FOVSlotDefinition> Design;
    TArray<FOVArmorSectionState> Armor;
    TArray<FOVWeaponMountState> Weapons;
    TArray<FName> Profiles;
};
template<class T> void Parts(FArchive& ar,TArray<T>& list)
{
    int32 count=list.Num();ar<<count;
    if(count<0||count>512){ar.SetError();return;}
    if(ar.IsLoading())list.SetNum(count);
    for(auto& part:list)T::StaticStruct()->SerializeItem(ar,&part,nullptr);
}
void Operations(FArchive& raw,TArray<FOVBayOrder>& orders,TArray<FOVResearchOrder>& science,TArray<VesselSnapshot>& vessels)
{
    FObjectAndNameAsStringProxyArchive ar(raw,false);
    int32 count=science.Num();ar<<count;
    if(count<0||count>=int(OVBase::Technology::Count)){raw.SetError();return;}
    if(ar.IsLoading())science.SetNum(count);
    for(auto& j:science)ar<<j.Tech<<j.Lab<<j.Time;
    count=orders.Num();ar<<count;
    if(count<0||count>512){raw.SetError();return;}
    if(ar.IsLoading()){orders.SetNum(count);vessels.SetNum(count);}
    for(int i=0;i<count;++i){
        auto& o=orders[i];auto& v=vessels[i];uint8 phase=uint8(o.Phase);
        ar<<o.Id<<o.Hull<<o.Mount<<o.Equipment<<o.Facility<<o.Berth<<o.ApproachLeg<<o.RefitYard<<o.LateralCm<<phase<<o.Time<<o.Total<<o.RemovedGear<<v.Present;
        if(ar.IsLoading())o.Phase=EOVBayPhase(phase);
        if(!v.Present)continue;
        ar<<v.Position<<v.Rotation<<v.Velocity<<v.Destination<<v.Move<<v.Hold<<v.Hull<<v.Shield<<v.ShieldOn<<v.ShieldOff<<v.Cargo<<v.Profiles;
        Parts(ar,v.Design);Parts(ar,v.Rooms);Parts(ar,v.Armor);Parts(ar,v.Weapons);
        if(ar.IsError()){raw.SetError();return;}
    }
}
bool HasManifest(AOVShip* s)
{
    if(!s)return false;
    auto* w=s->GetWorld();
    const auto* carrier=w->GetSubsystem<UOVCarrierSubsystem>();
    const auto* interior=w->GetSubsystem<UOVInteriorSubsystem>();
    return interior->Units.ContainsByPredicate([s](const auto& unit){return unit.Ship==s;}) ||
        carrier->Aircraft.ContainsByPredicate([s](const auto& a){return a.Carrier==s;}) ||
        carrier->Hangars.ContainsByPredicate([s](const auto& a){return a.Carrier==s && a.StoredSmallShip.IsValid();});
}
FString SlotPath()
{
    return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("BaseTrial"),TEXT("BaseTrial.ovbase"));
}

FArchive& Stock(FArchive& archive,OVBase::Resources& value)
{
    return archive << value.Common << value.Advanced << value.Rare;
}

FArchive& Record(FArchive& archive,OVBase::Module& module)
{
    int32 type=static_cast<int32>(module.Type);
    archive << type << module.Origin.X << module.Origin.Y << module.Origin.Z << module.Turns;
    archive << module.Remaining << module.Id << module.Level << module.UpgradeLevel;
    Stock(Stock(archive,module.Paid),module.Reserved);
    if (archive.IsLoading()) module.Type=static_cast<OVBase::Kind>(type);
    return archive;
}
}

bool AOVBasePresentation::SaveTrial(FString& message) const
{
    FBufferArchive data;
    int32 magic=SaveMagic,version=SaveVersion,count=ModuleCount(),next=Build.NextModuleId();
    uint32 research=Build.ResearchMask();
    uint8 charge=Build.Charges()?1:0;
    auto stock=Build.Available();
    data << magic << version << count << next << research << charge;
    Stock(data,stock);
    for (auto item:Build.Modules()) Record(data,item);
    auto orders=Orders;auto science=Science;TArray<VesselSnapshot> vessels;
    auto* controller=Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if(controller){
        for(AOVShip* field:controller->Ships){
            if(!IsValid(field)||field->TrialStation!=this||!field->BaseVesselId.IsValid())continue;
            if(orders.ContainsByPredicate([field](const auto& order){return order.Id==field->BaseVesselId;}))continue;
            auto& order=orders.AddDefaulted_GetRef();order.Id=field->BaseVesselId;
            order.Ship=field;order.Hull=field->HullDefinitionId;order.Phase=EOVBayPhase::Field;
        }
    }
    for(const auto& o:orders){
        auto& v=vessels.AddDefaulted_GetRef();auto* s=o.Ship.Get();v.Present=s!=nullptr;
        if(!s)continue;
        if(HasManifest(s)||s->Systems->bWrecked||s->Systems->bNeutral){message=TEXT("탑승자·함재기가 있거나 전투 손실 상태인 함선은 시험 저장을 지원하지 않습니다.");return false;}
        v.Position=s->GetActorLocation();v.Rotation=s->GetActorRotation();v.Velocity=s->Velocity;
        v.Destination=s->Destination;v.Move=s->bHasDestination;v.Hold=s->bHold;
        auto& sys=*s->Systems;v.Hull=sys.HullHP;v.Shield=sys.Shield;v.ShieldOn=sys.bShieldActive;v.ShieldOff=sys.bShieldManualOff;
        v.Design=sys.Layout->Slots;v.Rooms=sys.Slots;v.Armor=sys.ArmorSections;v.Weapons=sys.WeaponMountStates;
        if(const auto* c=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->FindCargo(s))v.Cargo={c->Amount,c->Advanced,c->Rare};
        for(const auto& m:GetWorld()->GetSubsystem<UOVCombatSubsystem>()->GetShipState(s).Mounts)v.Profiles.Add(m.Profile.ProfileId);
    }
    Operations(data,orders,science,vessels);
    if(data.IsError()){message=TEXT("운영 상태 저장 실패");return false;}
    uint32 checksum=FCrc::MemCrc32(data.GetData(),data.Num());
    data << checksum;

    const FString filename=SlotPath();
    const FString temporary=filename+TEXT(".tmp");
    auto& files=IFileManager::Get();
    files.MakeDirectory(*FPaths::GetPath(filename),true);
    const FString backup=filename+TEXT(".bak");
    if (!FFileHelper::SaveArrayToFile(data,*temporary) ||
        (files.FileExists(*filename) && files.Copy(*backup,*filename,true,true)!=COPY_OK) ||
        !files.Move(*filename,*temporary,true,true,false,true))
    {
        message=TEXT("저장 파일을 쓰지 못했습니다.");
        return false;
    }
    message=TEXT("기지 시험장을 저장했습니다.");
    return true;
}

bool AOVBasePresentation::LoadTrial(FString& message)
{
    const FString filename=SlotPath();
    const auto length=IFileManager::Get().FileSize(*filename);
    TArray<uint8> data;
    if (length<0)
    {
        message=TEXT("이 시험장에 저장한 기지가 없습니다.");
        return false;
    }
    if (length<4 || length>8*1024*1024 || !FFileHelper::LoadFileToArray(data,*filename))
    {
        message=TEXT("저장 파일을 읽지 못했습니다.");
        return false;
    }
    uint32 savedCrc;FMemory::Memcpy(&savedCrc,data.GetData()+length-4,4);
    if(savedCrc!=FCrc::MemCrc32(data.GetData(),length-4)){message=TEXT("저장 검증 실패 · 현재 기지 유지");return false;}
    struct FBoundedReader final : FMemoryReader { explicit FBoundedReader(TArray<uint8>& bytes) : FMemoryReader(bytes,true) {ArMaxSerializeSize=bytes.Num();} };
    FBoundedReader reader(data);
    int32 magic=0,version=0,count=0,next=0;
    uint32 research=0;
    uint8 charge=0;
    OVBase::Resources stock;
    reader << magic << version << count << next << research << charge;
    Stock(reader,stock);
    if (reader.IsError() || magic!=SaveMagic || (version!=1 && version!=SaveVersion) || count<1 || count>177 || charge>1)
    {
        message=TEXT("지원하지 않는 저장 형식입니다. 현재 기지를 유지합니다.");
        return false;
    }
    std::vector<OVBase::Module> modules(count);
    for (auto& item:modules) Record(reader,item);
    TArray<FOVBayOrder> orders;TArray<FOVResearchOrder> science;TArray<VesselSnapshot> vessels;
    if(version>=2)Operations(reader,orders,science,vessels);
    OVBase::BuildState restored=Build;
    uint32 checksum=0;
    reader << checksum;
    if (reader.IsError() || reader.Tell()!=reader.TotalSize() ||
        checksum!=FCrc::MemCrc32(data.GetData(),data.Num()-sizeof(checksum)) ||
        !restored.Restore(std::move(modules),stock,research,charge!=0,next))
    {
        message=TEXT("저장된 배치나 공사 기록이 올바르지 않습니다. 현재 기지를 유지합니다.");
        return false;
    }
    auto validModule=[&](int id){return std::any_of(restored.Modules().begin(),restored.Modules().end(),[id](const auto& m){return m.Id==id;});};
    TSet<int> techs;TSet<FGuid> ids;
    message=TEXT("운영 기록이 올바르지 않습니다. 현재 기지를 유지합니다.");
    for(const auto& j:science){
        if(!validModule(j.Lab)||j.Tech<=0||j.Tech>=int(OVBase::Technology::Count)||techs.Contains(j.Tech)||!FMath::IsFinite(j.Time)||j.Time<=0||j.Time>ResearchSeconds(j.Tech))return false;
        techs.Add(j.Tech);
    }
    for(int i=0;i<orders.Num();++i){
        const auto& o=orders[i];const auto& v=vessels[i];FOVHullMetadata meta;
        if(!o.Id.IsValid()||ids.Contains(o.Id)||!OVCalibration::GetHullMetadata(o.Hull,meta)||
            (o.RefitYard&&!validModule(o.RefitYard))||!FMath::IsFinite(o.LateralCm)||o.ApproachLeg<0||o.ApproachLeg>2||uint8(o.Phase)>uint8(EOVBayPhase::Refitting)||!FMath::IsFinite(o.Time)||o.Time<0||
            (o.Phase!=EOVBayPhase::Field&&!validModule(o.Facility))||v.Present==(o.Phase==EOVBayPhase::Building)||
            (v.Present&&(v.Position.ContainsNaN()||v.Rotation.ContainsNaN()||!FMath::IsFinite(v.Hull)||v.Hull<=0)))return false;
        ids.Add(o.Id);
    }
    auto* pc=Cast<AOVController>(GetWorld()->GetFirstPlayerController());if(!pc)return false;
    auto* factory=GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>();
    TArray<AOVShip*> created;
    auto rollback=[&](){for(auto* s:created)factory->RemoveTrialShip(s);return false;};
    for(const auto& old:Orders)if(HasManifest(old.Ship.Get())){message=TEXT("탑승자·함재기를 먼저 분리하세요.");return false;}
    for(int i=0;i<orders.Num();++i){
        auto& o=orders[i];const auto& v=vessels[i];if(!v.Present)continue;
        AOVShip* s=nullptr;
        for(AOVShip* existing:pc->Ships)if(IsValid(existing)&&existing->BaseVesselId==o.Id){s=existing;break;}
        if(!s){
            for(int pos=0;pos<100&&!s;++pos){
                const FVector staging(-150000.+(pos%10)*18000.,130000.-(pos/10)*18000.,48000.);
                auto result=factory->SpawnTrialShips(o.Hull,0,1,staging,0,0,0,false,false,true,true);
                if(!result.IsEmpty()){s=result[0];created.Add(s);}
            }
        }
        if(!s||!s->Systems||s->HullDefinitionId!=o.Hull||s->Systems->Slots.Num()!=v.Design.Num()||s->Systems->Slots.Num()!=v.Rooms.Num()||
            s->Systems->WeaponMountStates.Num()!=v.Weapons.Num()||s->Systems->ArmorSections.Num()!=v.Armor.Num())return rollback();
        auto* verified=DuplicateObject<UShipLayoutDataAsset>(s->Systems->Layout,s);
        verified->Slots=v.Design;TArray<FString> errors;
        if(!verified->ValidateLayout(errors))return rollback();
        for(int n=0;n<v.Design.Num();++n)
            if(v.Design[n].SlotId!=s->Systems->Layout->Slots[n].SlotId||v.Rooms[n].SlotId!=v.Design[n].SlotId)return rollback();
        o.Ship=s;
    }
    for(const auto& old:Orders){
        if(auto* s=old.Ship.Get();s&&!orders.ContainsByPredicate([s](const auto& o){return o.Ship==s;})){
            if(s->BaseVesselId==FGuid(0,0,0,1)){s->bUnderBaseControl=false;s->SetActorHiddenInGame(false);s->SetActorEnableCollision(true);s->StopMovement();}
            else factory->RemoveTrialShip(s);
        }
    }
    Build=MoveTemp(restored);Science=MoveTemp(science);Orders=MoveTemp(orders);
    RefreshMeshes();
    for(int i=0;i<Orders.Num();++i){
        auto& o=Orders[i];auto* s=o.Ship.Get();if(!s)continue;const auto& v=vessels[i];
        auto* combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();const auto old=combat->GetShipState(s);
        for(int m=0;m<old.Mounts.Num()&&m<v.Profiles.Num();++m)combat->SetWeaponProfile(s,old.Mounts[m].MountId,v.Profiles[m]);
        s->BaseVesselId=o.Id;s->TrialStation=this;s->SetActorLocationAndRotation(v.Position,v.Rotation);
        s->Velocity=v.Velocity;s->Destination=v.Destination;s->bHasDestination=v.Move;s->bHold=v.Hold;
        auto& sys=*s->Systems;auto* design=DuplicateObject<UShipLayoutDataAsset>(sys.Layout,s);design->Slots=v.Design;sys.Layout=design;sys.HullHP=v.Hull;sys.Shield=v.Shield;sys.bShieldActive=v.ShieldOn;sys.bShieldManualOff=v.ShieldOff;
        sys.Slots=v.Rooms;sys.ArmorSections=v.Armor;sys.WeaponMountStates=v.Weapons;sys.RefreshDerivedState();
        for(auto& c:GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->Cargo)if(c.Ship==s){c.Amount=v.Cargo.X;c.Advanced=v.Cargo.Y;c.Rare=v.Cargo.Z;}
        const bool field=o.Phase==EOVBayPhase::Field||o.Phase==EOVBayPhase::Returning;
        s->bUnderBaseControl=!field;s->SetActorHiddenInGame(!field&&o.Phase!=EOVBayPhase::Departing&&o.Phase!=EOVBayPhase::Entering);s->SetActorEnableCollision(field);
    }
    message=TEXT("저장한 기지를 불러왔습니다.");
    return true;
}
