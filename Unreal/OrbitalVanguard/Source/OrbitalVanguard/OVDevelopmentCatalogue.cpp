#include "OVDevelopmentCatalogue.h"
#include "OVCalibration.h"
#include "OVCarrierSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
bool IsHexDigest(const FString& Value, int32 Length)
{
    if (Value.Len() != Length) return false;
    for (TCHAR C : Value)
        if (!((C >= TEXT('0') && C <= TEXT('9')) || (C >= TEXT('a') && C <= TEXT('f')) || (C >= TEXT('A') && C <= TEXT('F')))) return false;
    return true;
}

void ApplyGenerationEvidence(const TSharedPtr<FJsonObject>& Root, TArray<FOVCatalogDefinition>& Definitions)
{
    double Version = 0;
    FString CodeSHA, TreeSHA, BinarySHA, ReportPath, ReportSHA;
    const TArray<TSharedPtr<FJsonValue>>* Checks = nullptr;
    if (!Root->TryGetNumberField(TEXT("schema_version"), Version) || Version != 1 ||
        !Root->TryGetStringField(TEXT("code_sha"), CodeSHA) || !IsHexDigest(CodeSHA, 40) ||
        !Root->TryGetStringField(TEXT("source_archive_sha256"), TreeSHA) || !IsHexDigest(TreeSHA, 64) ||
        !Root->TryGetStringField(TEXT("binary_sha256"), BinarySHA) || !IsHexDigest(BinarySHA, 64) ||
        !Root->TryGetStringField(TEXT("report"), ReportPath) || ReportPath.IsEmpty() ||
        !Root->TryGetStringField(TEXT("report_sha256"), ReportSHA) || !IsHexDigest(ReportSHA, 64) ||
        !Root->TryGetArrayField(TEXT("checks"), Checks) || !Checks) return;
    // Only individual, unambiguous observed spawn checks update historical status.
    for (FOVCatalogDefinition& Definition : Definitions)
    {
        if (Definition.Category != EOVCatalogCategory::Hull) continue;
        const FString Expected = TEXT("생성 ") + Definition.DefinitionId.ToString();
        int32 Matches = 0; bool bPassed = false;
        for (const auto& Value : *Checks)
        {
            const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
            FString CheckId; bool Result = false;
            if (Row.IsValid() && Row->TryGetStringField(TEXT("id"), CheckId) && CheckId == Expected && Row->TryGetBoolField(TEXT("passed"), Result))
            { ++Matches; bPassed = Result; }
        }
        if (Matches != 1) continue;
        FOVCatalogEvidence Historic;
        Historic.Status = EOVCatalogStatus::Partial;
        Historic.VerificationSHA = CodeSHA;
        Historic.Reason = FString::Printf(TEXT("기록된 빌드의 개별 생성 검사 %s. 현재 실행 바이너리와 일치 검증은 미실시. 보고서: %s / 보고서 SHA-256: %s / 소스 아카이브 SHA-256: %s / 바이너리 SHA-256: %s"),
            bPassed ? TEXT("성공") : TEXT("실패"), *ReportPath, *ReportSHA, *TreeSHA, *BinarySHA);
        Definition.Generation = Historic;
        Definition.AutomatedVerification = Historic;
    }
}

void ApplyRoleEvidence(const TSharedPtr<FJsonObject>& Root, TArray<FOVCatalogDefinition>& Definitions)
{
    double Version = 0;
    FString CodeSHA, SourceSHA, BinarySHA, ReportPath, ReportSHA;
    const TArray<TSharedPtr<FJsonValue>>* Checks = nullptr;
    if (!Root->TryGetNumberField(TEXT("schema_version"), Version) || Version != 1 ||
        !Root->TryGetStringField(TEXT("role_code_sha"), CodeSHA) || !IsHexDigest(CodeSHA, 40) ||
        !Root->TryGetStringField(TEXT("role_source_archive_sha256"), SourceSHA) || !IsHexDigest(SourceSHA, 64) ||
        !Root->TryGetStringField(TEXT("role_binary_sha256"), BinarySHA) || !IsHexDigest(BinarySHA, 64) ||
        !Root->TryGetStringField(TEXT("role_report"), ReportPath) || ReportPath.IsEmpty() ||
        !Root->TryGetStringField(TEXT("role_report_sha256"), ReportSHA) || !IsHexDigest(ReportSHA, 64) ||
        !Root->TryGetArrayField(TEXT("role_checks"), Checks) || !Checks) return;
    TMap<FString, int32> Counts;
    TMap<FString, bool> Passed;
    for (const auto& Value : *Checks)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
        FString Id; bool Result = false;
        if (Row.IsValid() && Row->TryGetStringField(TEXT("id"), Id) && Row->TryGetBoolField(TEXT("passed"), Result))
        { ++Counts.FindOrAdd(Id); Passed.Add(Id, Result); }
    }
    for (FOVCatalogDefinition& D : Definitions)
    {
        if (D.Category != EOVCatalogCategory::Hull) continue;
        const FString Key = D.DefinitionId.ToString();
        const TCHAR* Common[] = {TEXT("실제 생성"), TEXT("선택"), TEXT("실제 이동"), TEXT("실제 시설 피해"),
            TEXT("엔진 기능 정지"), TEXT("코어 완파"), TEXT("DEV 정리"), TEXT("삭제 등록 정리")};
        TArray<FString> Required;
        for (const TCHAR* Name : Common) Required.Add(Key + TEXT(" / ") + Name);
        const bool bRepair = Key == TEXT("repair"), bMining = Key == TEXT("mining"),
            bTransport = Key == TEXT("transport"), bAssault = Key == TEXT("assault");
        const bool bSupport = bRepair || bMining || bTransport || bAssault;
        if (bSupport) Required.Add(Key + TEXT(" / 공격 무장 없음"));
        else
        {
            Required.Add(Key + TEXT(" / 발사 요청"));
            Required.Add(Key + TEXT(" / 실제 발사"));
            Required.Add(Key + TEXT(" / 동일 표적 실제 피격"));
        }
        if (bRepair) { Required.Add(TEXT("수리 / 유한 자원 링크 시작")); Required.Add(TEXT("수리 / 실제 복구·자원 차감")); }
        if (bMining) Required.Add(TEXT("채굴 / 유한 자원 차감"));
        if (bTransport) { Required.Add(TEXT("수송 / 채굴 재고 수거")); Required.Add(TEXT("수송 / 수리함 재고 이전")); }
        if (bAssault) { Required.Add(TEXT("assault / PD 1기")); Required.Add(TEXT("assault / 수송선 등록")); }
        if (Key == TEXT("carrier") || Key == TEXT("battlecarrier"))
        { Required.Add(Key + TEXT(" / 실제 출격")); Required.Add(Key + TEXT(" / 실제 복귀와 동일 기체 재고")); }
        bool bComplete = true;
        for (const FString& Id : Required)
        {
            const int32* Count = Counts.Find(Id); const bool* Result = Passed.Find(Id);
            if (!Count || *Count != 1 || !Result || !*Result) { bComplete = false; break; }
        }
        if (!bComplete) continue;
        D.Role.Status = EOVCatalogStatus::Partial;
        D.Role.VerificationSHA = CodeSHA;
        D.Role.Reason = FString::Printf(TEXT("과거 빌드의 해당 함체 개별 시험 %d건 통과. 생성·선택·이동과 DEV 시설 파괴/완파·정리를 확인. 공격 함체는 대표 무장 1개의 발사·피격, 지원함은 시험 자원 작업 일부만 확인. PD 비활성 조건으로 실제 방공/레이더 역할 및 모든 장착점은 미검증. 현재 실행 바이너리와 일치 여부 미확인. 검증 코드 %s · 보고서 %s · 보고서 SHA-256 %s · 소스 SHA-256 %s · 바이너리 SHA-256 %s"),
            Required.Num(), *CodeSHA, *ReportPath, *ReportSHA, *SourceSHA, *BinarySHA);
    }
}

void ApplyHistoricalEvidence(TArray<FOVCatalogDefinition>& Definitions)
{
    FString Data;
    const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DevelopmentEvidence.json"));
    if (!FFileHelper::LoadFileToString(Data, *Path) || Data.Len() > 2000000) return;
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;
    ApplyGenerationEvidence(Root, Definitions);
    ApplyRoleEvidence(Root, Definitions);
}

FOVCatalogEvidence Evidence(EOVCatalogStatus Status, const TCHAR* Reason)
{
    FOVCatalogEvidence E; E.Status = Status; E.Reason = Reason; return E;
}
FString WeaponName(FName Id)
{
    const FString Value = Id.ToString();
    struct FLabel { const TCHAR* Prefix; const TCHAR* Name; };
    const FLabel Labels[] = {{TEXT("D116_heavy_beam_"),TEXT("중형 빔")}, {TEXT("D116_heavy_cannon_"),TEXT("중포")},
        {TEXT("D116_guided_"),TEXT("유도 미사일")}, {TEXT("D116_missile_"),TEXT("미사일")},
        {TEXT("D116_spinal_"),TEXT("초대형 전용 주포")}, {TEXT("D116_cannon_"),TEXT("함포")}, {TEXT("D116_beam_"),TEXT("빔")}};
    for (const auto& Label : Labels)
        if (Value.StartsWith(Label.Prefix)) return FString::Printf(TEXT("%s %s"), Label.Name, *Value.Right(2));
    return Value;
}
FOVCatalogDefinition Base(FName Id, EOVCatalogCategory Category, const FString& Name, const FString& Description, const FString& Source)
{
    FOVCatalogDefinition D; D.DefinitionId = Id; D.Category = Category; D.DisplayName = Name; D.Description = Description; D.SourceEvidence = Source;
    D.Planning = Evidence(EOVCatalogStatus::Available, TEXT("관련 기획 문서에 정의됨. 일부 수치는 후보임."));
    D.Data = Evidence(EOVCatalogStatus::Available, TEXT("현재 소스의 후보 데이터에 정의됨."));
    D.Generation = Evidence(EOVCatalogStatus::Unknown, TEXT("개별 생성 경로의 통합 실행 결과를 아직 확인하지 않음."));
    D.Role = Evidence(EOVCatalogStatus::Unknown, TEXT("해당 정의의 역할별 실제 동작을 아직 확인하지 않음."));
    D.AutomatedVerification = Evidence(EOVCatalogStatus::Unknown, TEXT("현재 리비전의 개별 자동 검증 결과 없음."));
    D.DirectInput = Evidence(EOVCatalogStatus::Unknown, TEXT("직접 입력 경로의 통합 실행 결과를 아직 확인하지 않음."));
    return D;
}
}
TArray<FOVCatalogDefinition> UOVDevelopmentCatalogue::GetDefinitions() const
{
    TArray<FOVCatalogDefinition> Out;
    const FString CalibrationSource = FString::Printf(TEXT("OVCalibrationData.inl; source SHA-256 %s"), *OVCalibration::GetSourceHash());
    for (FName Id : OVCalibration::GetHullIds())
    {
        FOVHullMetadata Metadata;
        OVCalibration::GetHullMetadata(Id, Metadata);
        auto D = Base(Id, EOVCatalogCategory::Hull, OVCalibration::GetHullName(Id), Metadata.Description, CalibrationSource);
        D.SizeClass = Metadata.SizeClass; D.FleetCost = Metadata.FleetCost; D.RoleTags = Metadata.RoleTags;
        Out.Add(MoveTemp(D));
    }
    for (FName Id : OVCalibration::GetWeaponProfileIds())
    {
        auto D = Base(Id, EOVCatalogCategory::Weapon, WeaponName(Id), OVCalibration::GetWeaponProfileDescription(Id), CalibrationSource);
        D.Generation = Evidence(EOVCatalogStatus::NotApplicable, TEXT("무장 프로필은 독립 함체가 아니라 장착 데이터다."));
        Out.Add(MoveTemp(D));
    }
    {
        auto D = Base(TEXT("planned_special_weapon"), EOVCatalogCategory::Weapon, TEXT("특수 무장 · 내용 미정"),
            TEXT("기획상 특수 무장 계열을 나타내는 단일 표시 항목. 하위 종류·티어·효과·공급 시설은 미정이며, 이 항목을 미정 무장의 전체 개수로 해석할 수 없다. 현재 후보 데이터 49개와 별도다."),
            TEXT("docs/01_design/fleet/FLEET_COMBAT.md: 공격 무장 표"));
        D.Data = Evidence(EOVCatalogStatus::Unavailable, TEXT("구체적인 무장 종류와 수치 데이터가 아직 없다."));
        D.Generation = Evidence(EOVCatalogStatus::NotApplicable, TEXT("무장 계열의 기획 표시이며 생성 가능한 개체가 아니다."));
        D.Role = Evidence(EOVCatalogStatus::Unknown, TEXT("특수 무장의 효과와 역할은 미정이다."));
        D.AutomatedVerification = Evidence(EOVCatalogStatus::Unknown, TEXT("구현되지 않아 동작 검증 결과가 없다."));
        D.DirectInput = Evidence(EOVCatalogStatus::Unavailable, TEXT("종류와 장착 데이터가 없어 직접 장착할 수 없다."));
        Out.Add(MoveTemp(D));
    }
    for (EOVPDWeaponKind Kind : {EOVPDWeaponKind::Beam, EOVPDWeaponKind::Kinetic})
    {
        const FOVPDProfile Profile = UOVPointDefenseSubsystem::DefaultProfile(Kind);
        const FString Description = FString::Printf(TEXT("2C 시험 방공 · 사거리 %.0fcm · 명중 계수 %.2f · 주기 %.2f초 · 피해 %.1f · 장착점 HP %.1f"),
            Profile.Range, Profile.Accuracy, Profile.Interval, Profile.Damage, Profile.MountHP);
        auto D = Base(Profile.ProfileId, EOVCatalogCategory::PointDefense,
            Kind == EOVPDWeaponKind::Beam ? TEXT("방공 빔") : TEXT("방공 실탄"), Description,
            TEXT("OVPointDefenseSubsystem.cpp: DefaultProfile; OVPointDefenseSubsystem.h: FOVPDProfile"));
        D.Generation = Evidence(EOVCatalogStatus::NotApplicable, TEXT("독립 함체가 아니라 방공 장착점 시험 프로필이다."));
        D.DirectInput = Evidence(EOVCatalogStatus::Unavailable, TEXT("방공 종류를 직접 교체하는 DEV 명령은 아직 제공되지 않는다."));
        Out.Add(MoveTemp(D));
    }
    const FOVAircraftProfile Aircraft;
    auto Fighter = Base(Aircraft.ProfileId, EOVCatalogCategory::Fighter, TEXT("기본 함재기"), TEXT("격납고가 보유하는 개별 기체 후보. 함체 카탈로그와 별개다."), TEXT("OVCarrierSubsystem.h: FOVAircraftProfile; docs/01_design/fleet/CARRIER_OPERATIONS.md"));
    Out.Add(MoveTemp(Fighter));
    const FOVTransportProfile Transport;
    auto Pod = Base(Transport.ProfileId, EOVCatalogCategory::Pod, TEXT("개인 강습 포드"), TEXT("개인 수송 단위의 DEV 시험 프로필. 함체나 함재기가 아니다."), TEXT("OVTransportSubsystem.h: FOVTransportProfile; docs/01_design/boarding/BOARDING_COMBAT.md"));
    Out.Add(MoveTemp(Pod));
    struct FHeroDefinition { const TCHAR* Id; const TCHAR* Name; };
    const FHeroDefinition Heroes[] = {{TEXT("hero_melee"),TEXT("근거리형 영웅")}, {TEXT("hero_ranged"),TEXT("원거리형 영웅")}, {TEXT("hero_support"),TEXT("지원형 영웅")}, {TEXT("hero_commander"),TEXT("지휘형 영웅")}};
    for (const auto& Hero : Heroes)
    {
        auto D = Base(Hero.Id, EOVCatalogCategory::Hero, Hero.Name, TEXT("영웅 전투 유형 정의. 개별 영웅 ID와 분리되며 성장·스킬 수치는 후보이다."), TEXT("OVInteriorSubsystem.h: EOVInteriorRole; docs/01_design/boarding/HERO_SYSTEMS.md"));
        D.Data = Evidence(EOVCatalogStatus::Partial, TEXT("유형 열거형과 시험 프로필이 있으나 전체 기획 능력의 검증은 별도다."));
        Out.Add(MoveTemp(D));
    }
    ApplyHistoricalEvidence(Out);
    return Out;
}

FString UOVDevelopmentCatalogue::GetOverviewText() const
{
    const TArray<FOVCatalogDefinition> Definitions = GetDefinitions();
    FString Text = TEXT("전체 등록 항목 ") + FString::FromInt(Definitions.Num())
        + TEXT("개 · 기획 항목과 후보 데이터는 별개로 집계\n");
    Text += TEXT("기획 기준: 035a4fba81d26f1d67daa1f44bded0f7e8d3a434\n");
    FString BuildText = TEXT("빌드 코드/소스 식별 정보: 미기록\n");
    const FString BuildPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("BuildIdentity.generated.json"));
    FString Raw;
    if (FFileHelper::LoadFileToString(Raw, *BuildPath) && Raw.Len() <= 4096)
    {
        TSharedPtr<FJsonObject> Object;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
        FString CodeSHA, SourceSHA; bool bDirty = false;
        if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid()
            && Object->TryGetStringField(TEXT("code_sha"), CodeSHA) && IsHexDigest(CodeSHA, 40)
            && Object->TryGetStringField(TEXT("source_sha256"), SourceSHA) && IsHexDigest(SourceSHA, 64)
            && Object->TryGetBoolField(TEXT("dirty"), bDirty))
        {
            BuildText = FString::Printf(TEXT("빌드 기록 코드: %s · 미커밋 변경: %s\n빌드 소스 SHA-256: %s\n"),
                *CodeSHA, bDirty ? TEXT("있음") : TEXT("없음"), *SourceSHA);
        }
    }
    Text += BuildText;
    FString EvidenceSHA;
    for (const FOVCatalogDefinition& D : Definitions)
        if (IsHexDigest(D.AutomatedVerification.VerificationSHA, 40)) { EvidenceSHA = D.AutomatedVerification.VerificationSHA; break; }
    Text += EvidenceSHA.IsEmpty() ? TEXT("개별 검증 기록 리비전: 미기록\n")
        : FString::Printf(TEXT("개별 검증 기록 리비전: %s · 현재 빌드와 일치 여부 미확인\n"), *EvidenceSHA);
    struct FCategory { EOVCatalogCategory Category; const TCHAR* Name; };
    const FCategory Categories[] = {{EOVCatalogCategory::Hull,TEXT("함체")}, {EOVCatalogCategory::Weapon,TEXT("공격 무장")},
        {EOVCatalogCategory::Fighter,TEXT("함재기")}, {EOVCatalogCategory::Pod,TEXT("포드")},
        {EOVCatalogCategory::Hero,TEXT("영웅 유형")}, {EOVCatalogCategory::PointDefense,TEXT("방공 무장")}};
    for (const FCategory& Category : Categories)
    {
        int32 Total=0, Planned=0, Data=0, DataPartial=0, Generated=0, GeneratedHistorical=0, GenerationUnknown=0;
        int32 Role=0, RolePartial=0, RoleUnknown=0;
        for (const FOVCatalogDefinition& D : Definitions)
        {
            if (D.Category != Category.Category) continue;
            ++Total;
            Planned += D.Planning.Status == EOVCatalogStatus::Available ? 1 : 0;
            Data += D.Data.Status == EOVCatalogStatus::Available ? 1 : 0;
            DataPartial += D.Data.Status == EOVCatalogStatus::Partial ? 1 : 0;
            Generated += D.Generation.Status == EOVCatalogStatus::Available ? 1 : 0;
            GeneratedHistorical += D.Generation.Status == EOVCatalogStatus::Partial ? 1 : 0;
            GenerationUnknown += D.Generation.Status == EOVCatalogStatus::Unknown ? 1 : 0;
            Role += D.Role.Status == EOVCatalogStatus::Available ? 1 : 0;
            RolePartial += D.Role.Status == EOVCatalogStatus::Partial ? 1 : 0;
            RoleUnknown += D.Role.Status == EOVCatalogStatus::Unknown ? 1 : 0;
        }
        Text += FString::Printf(TEXT("\n%s %d · 기획 %d · 데이터 있음 %d/부분 %d\n생성 확인 %d/과거 기록 %d/미확인 %d · 역할 기능 확인 %d/부분 확인 %d/미확인 %d\n"),
            Category.Name, Total, Planned, Data, DataPartial, Generated, GeneratedHistorical, GenerationUnknown, Role, RolePartial, RoleUnknown);
    }
    Text += TEXT("\n과거 기록은 성공·실패를 포함할 수 있으며 현재 빌드의 기능 완료를 뜻하지 않습니다. 해당 없음·미지원 항목은 미확인 집계와 구분합니다.");
    return Text;
}
