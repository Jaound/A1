#include "OVBasePresentation.h"

#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

namespace
{
constexpr int32 SaveMagic=0x4f564254;
constexpr int32 SaveVersion=1;

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
    if (length<4 || length>16384 || !FFileHelper::LoadFileToArray(data,*filename))
    {
        message=TEXT("저장 파일을 읽지 못했습니다.");
        return false;
    }
    FMemoryReader reader(data,true);
    int32 magic=0,version=0,count=0,next=0;
    uint32 research=0;
    uint8 charge=0;
    OVBase::Resources stock;
    reader << magic << version << count << next << research << charge;
    Stock(reader,stock);
    if (reader.IsError() || magic!=SaveMagic || version!=SaveVersion || count<1 || count>177 || charge>1)
    {
        message=TEXT("지원하지 않는 저장 형식입니다. 현재 기지를 유지합니다.");
        return false;
    }
    std::vector<OVBase::Module> modules(count);
    for (auto& item:modules) Record(reader,item);
    uint32 checksum=0;
    reader << checksum;
    if (reader.IsError() || reader.Tell()!=reader.TotalSize() ||
        checksum!=FCrc::MemCrc32(data.GetData(),data.Num()-sizeof(checksum)) ||
        !Build.Restore(std::move(modules),stock,research,charge!=0,next))
    {
        message=TEXT("저장된 배치나 공사 기록이 올바르지 않습니다. 현재 기지를 유지합니다.");
        return false;
    }
    RefreshMeshes();
    message=TEXT("저장한 기지를 불러왔습니다.");
    return true;
}
