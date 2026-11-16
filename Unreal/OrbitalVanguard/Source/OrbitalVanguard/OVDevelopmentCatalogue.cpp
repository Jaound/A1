#include "OVDevelopmentCatalogue.h"
#include "OVCalibration.h"
#include "OVCarrierSubsystem.h"
#include "OVTransportSubsystem.h"

namespace
{
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
        auto D = Base(Id, EOVCatalogCategory::Hull, OVCalibration::GetHullName(Id), TEXT("D116 시험 함체. 수치와 내부 배치는 후보이며 최종 에셋이 아니다."), CalibrationSource);
        Out.Add(MoveTemp(D));
    }
    for (FName Id : OVCalibration::GetWeaponProfileIds())
    {
        auto D = Base(Id, EOVCatalogCategory::Weapon, WeaponName(Id), TEXT("D116 시험 무장 프로필. 이름의 T는 무장 티어다."), CalibrationSource);
        D.Generation = Evidence(EOVCatalogStatus::NotApplicable, TEXT("무장 프로필은 독립 함체가 아니라 장착 데이터다."));
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
    return Out;
}
