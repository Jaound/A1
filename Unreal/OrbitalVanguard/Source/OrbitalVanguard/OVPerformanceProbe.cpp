#include "OVPerformanceProbe.h"
#include "OVPerformance.h"
#include "OVController.h"
#include "OVHUD.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVTransportSubsystem.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectArray.h"

CSV_DEFINE_CATEGORY(OrbitalVanguard, true);

namespace
{
const TCHAR* StageName(int32 Stage)
{
    return Stage == 0 ? TEXT("출격") : Stage == 1 ? TEXT("교전") : TEXT("귀환");
}
double Fraction(int32 Stage) { return Stage == 0 ? .25 : Stage == 1 ? .45 : .30; }
int32 Objects() { return GUObjectArray.GetObjectArrayNumMinusAvailable(); }
double MemoryMB() { return FPlatformMemory::GetStats().UsedPhysical / (1024. * 1024.); }
}

bool UOVPerformanceProbe::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return FParse::Param(FCommandLine::Get(), TEXT("OVPerf")) && World
        && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
        && Super::ShouldCreateSubsystem(Outer);
}

TStatId UOVPerformanceProbe::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UOVPerformanceProbe, STATGROUP_Tickables);
}

void UOVPerformanceProbe::BeginRun(AOVController& PC, double Now)
{
    bStarted = true;
    RunStarted = LastFrameAt = LastSampleAt = Now;
    FParse::Value(FCommandLine::Get(), TEXT("OVPerfSeconds="), RequestedSeconds);
    RequestedSeconds = FMath::Clamp(RequestedSeconds, 18., 900.);
    Label = TEXT("현재코드");
    FParse::Value(FCommandLine::Get(), TEXT("OVPerfLabel="), Label);
    Label = FPaths::MakeValidFileName(Label);
    OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/IntegratedPerformance"), Label);
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    int32 PairFilter = -1, SpeedFilter = 0;
    FParse::Value(FCommandLine::Get(), TEXT("OVPerfPairs="), PairFilter);
    FParse::Value(FCommandLine::Get(), TEXT("OVPerfSpeed="), SpeedFilter);
    for (int32 Pairs : {0, 1, 3}) for (int32 Speed : {1, 3})
        if ((PairFilter < 0 || PairFilter == Pairs) && (SpeedFilter == 0 || SpeedFilter == Speed))
            Cases.Add(FIntPoint(Pairs, Speed));
    if (Cases.IsEmpty()) { Failure = TEXT("지원하지 않는 편성/배속 필터"); Finish(PC); return; }
    FFileHelper::SaveStringToFile(TEXT("벽시계초,사례,단계,게임초,함선,생존함선,기체레코드,비행기체,완파기체,기체탄,대함탄,포드,승객,진입기록,기체피격기록,요격기록,대함피격기록,UObject추정개수,프로세스물리MB\n"),
        *FPaths::Combine(OutputDirectory, TEXT("상태시계열.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FCsvProfiler::Get()->BeginCapture(-1, OutputDirectory, TEXT("프로파일.csv"));
    BeginCase(PC, Now);
    UE_LOG(LogTemp, Display, TEXT("OV PERF 시작: %s, 측정 %.0f초 + 사례별 준비 %.0f초"), *Label, RequestedSeconds, WarmupSeconds);
}

void UOVPerformanceProbe::BeginCase(AOVController& PC, double Now)
{
    if (!PC.StartPhase2CStress(Cases[CaseIndex].X))
    {
        Failure = TEXT("부하 측정 장면 생성 실패"); Finish(PC); return;
    }
    PC.bDeveloperMode = false;
    if (auto* HUD = Cast<AOVHUD>(PC.GetHUD())) HUD->SetDetailsOpen(false);
    PC.SimulationSpeed = Cases[CaseIndex].Y;
    PC.bSimulationPaused = false;
    for (AOVShip* Ship : PC.Ships) Ship->SetAutomation(false, true);
    for (const auto& Carrier : PC.Carrier->Carriers) PC.Carrier->Launch(Carrier.Carrier.Get());
    bWarmup = true;
    CaseStarted = LastFrameAt = Now;
    Stage = 0;
    FrameMilliseconds.Reset();
    TRACE_BOOKMARK(TEXT("OV 성능 사례 %d: 함선 %d, 배속 %d"), CaseIndex, PC.Ships.Num(), Cases[CaseIndex].Y);
}

void UOVPerformanceProbe::BeginStage(AOVController& PC, int32 NextStage, double Now)
{
    Stage = NextStage;
    StageStarted = LastFrameAt = Now;
    StageGameStarted = PC.Combat->GameTime;
    BeginObjects = Objects();
    BeginMemory = FPlatformMemory::GetStats().UsedPhysical;
    StartShots = PC.Carrier->ShotCount;
    StartHits = PC.Carrier->HitCount;
    StartPDShots = PC.PointDefense->ShotCount;
    FrameMilliseconds.Reset();
    // 귀환은 명시 명령이다. 무료 수리·재고 보충·함선 HP 변경은 하지 않는다.
    if (Stage == 2)
        for (const auto& Carrier : PC.Carrier->Carriers) PC.Carrier->Recall(Carrier.Carrier.Get());
    TRACE_BOOKMARK(TEXT("OV 성능 %d %s"), CaseIndex, StageName(Stage));
}

void UOVPerformanceProbe::RecordCounts(AOVController& PC, double Now)
{
    int32 Airborne = 0, Wrecked = 0, LivingShips = 0;
    for (const auto& Craft : PC.Carrier->Aircraft)
    {
        Airborne += UOVCarrierSubsystem::IsAirborneState(Craft.State) ? 1 : 0;
        Wrecked += Craft.State == EOVAircraftState::Wrecked ? 1 : 0;
    }
    for (const AOVShip* Ship : PC.Ships) LivingShips += !Ship->Systems->bWrecked ? 1 : 0;
    const FString Row = FString::Printf(TEXT("%.3f,%d,%s,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f\n"),
        Now - RunStarted, CaseIndex, bWarmup ? TEXT("준비") : StageName(Stage), PC.Combat->GameTime,
        PC.Ships.Num(), LivingShips, PC.Carrier->Aircraft.Num(), Airborne, Wrecked,
        PC.Carrier->Bolts.Num(), PC.Combat->Attacks.Num(), PC.Transport->Pods.Num(), PC.Transport->Persons.Num(),
        PC.Transport->EntryEvents.Num(), PC.Carrier->RecentEvents.Num(), PC.PointDefense->RecentEvents.Num(), PC.Combat->RecentEvents.Num(), Objects(), MemoryMB());
    FFileHelper::SaveStringToFile(Row, *FPaths::Combine(OutputDirectory, TEXT("상태시계열.csv")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    CSV_CUSTOM_STAT(OrbitalVanguard, AircraftRecords, PC.Carrier->Aircraft.Num(), ECsvCustomStatOp::Set);
    CSV_CUSTOM_STAT(OrbitalVanguard, FighterBolts, PC.Carrier->Bolts.Num(), ECsvCustomStatOp::Set);
    CSV_CUSTOM_STAT(OrbitalVanguard, AttackRecords, PC.Combat->Attacks.Num(), ECsvCustomStatOp::Set);
    CSV_CUSTOM_STAT(OrbitalVanguard, UObjectEstimate, Objects(), ECsvCustomStatOp::Set);
    CSV_CUSTOM_STAT(OrbitalVanguard, PhysicalMemoryMB, float(MemoryMB()), ECsvCustomStatOp::Set);
}

void UOVPerformanceProbe::EndStage(AOVController& PC, double Now)
{
    if (FrameMilliseconds.IsEmpty()) return;
    FrameMilliseconds.Sort();
    double Sum = 0;
    for (double Value : FrameMilliseconds) Sum += Value;
    int32 Width = 0, Height = 0;
    PC.GetViewportSize(Width, Height);
    int32 Airborne = 0, Wrecked = 0;
    int32 FriendlyCost = 0, EnemyCost = 0, LivingFriendlyCost = 0, LivingEnemyCost = 0;
    const int32 CostByHullSize[] = {0, 1, 5, 25, 125};
    // 이 교정 장면은 ConfigureHull이 WeaponTierCap에 함체 size를 명시 등록한다.
    // 장갑/덱 수/실제 혼합 무장 티어에서 함급을 추정하지 않는다.
    for (const auto& ShipState : PC.Combat->Ships)
    {
        const int32 Cost = CostByHullSize[FMath::Clamp(ShipState.WeaponTierCap, 1, 4)];
        const bool Friendly = ShipState.Ship->Systems->FactionId == 0;
        (Friendly ? FriendlyCost : EnemyCost) += Cost;
        if (!ShipState.Ship->Systems->bWrecked && !ShipState.bDeparted)
            (Friendly ? LivingFriendlyCost : LivingEnemyCost) += Cost;
    }
    for (const auto& Craft : PC.Carrier->Aircraft)
    {
        Airborne += UOVCarrierSubsystem::IsAirborneState(Craft.State) ? 1 : 0;
        Wrecked += Craft.State == EOVAircraftState::Wrecked ? 1 : 0;
    }
    Results.Add(FString::Printf(TEXT("{\"사례\":%d,\"추가항모쌍\":%d,\"배속\":%d,\"단계\":\"%s\",\"벽시계초\":%.3f,\"게임초\":%.3f,\"화면너비\":%d,\"화면높이\":%d,\"함선\":%d,\"기체레코드\":%d,\"비행기체\":%d,\"완파기체\":%d,\"프레임수\":%d,\"평균ms\":%.6f,\"p95ms\":%.6f,\"최대ms\":%.6f,\"시작UObject\":%d,\"종료UObject\":%d,\"시작물리MB\":%.3f,\"종료물리MB\":%.3f,\"기체발사\":%d,\"기체명중\":%d,\"PD발사\":%d}"),
        CaseIndex, Cases[CaseIndex].X, Cases[CaseIndex].Y, StageName(Stage), Now-StageStarted, PC.Combat->GameTime-StageGameStarted,
        Width, Height, PC.Ships.Num(), PC.Carrier->Aircraft.Num(), Airborne, Wrecked, FrameMilliseconds.Num(),
        Sum/FrameMilliseconds.Num(), FrameMilliseconds[FMath::Min(FrameMilliseconds.Num()-1, FMath::CeilToInt(FrameMilliseconds.Num()*.95)-1)],
        FrameMilliseconds.Last(), BeginObjects, Objects(), BeginMemory/(1024.*1024.), MemoryMB(),
        PC.Carrier->ShotCount-StartShots, PC.Carrier->HitCount-StartHits, PC.PointDefense->ShotCount-StartPDShots));
    Results.Last().RemoveFromEnd(TEXT("}"));
    Results.Last() += FString::Printf(TEXT(",\"등록아군코스트\":%d,\"등록적군코스트\":%d,\"생존아군코스트\":%d,\"생존적군코스트\":%d}"),
        FriendlyCost, EnemyCost, LivingFriendlyCost, LivingEnemyCost);
    SaveReport(false);
}

void UOVPerformanceProbe::SaveReport(bool bComplete)
{
    FString Report = FString::Printf(TEXT("{\"종류\":\"별도 게임 창 실제 렌더/벽시계 프레임; 자동 API 시간 진행 아님\",\"완료\":%s,\"전체벽시계초\":%.3f,\"요청측정초\":%.0f,\"사례별준비초\":%.0f,\"설명\":\"같은 프로세스에서 명시 장면 재시작; 사망/손상 보존하며 무료 보충 없음. CSV는 준비/전환 포함, 구간 JSON은 준비 제외. UObject는 GUObjectArray 점유 인덱스 추정이며 GC 대기 포함. 중첩 프로파일 구간 합산 금지.\",\"오류\":\"%s\",\"구간\":[%s]}"),
        bComplete ? TEXT("true") : TEXT("false"), FPlatformTime::Seconds()-RunStarted, RequestedSeconds, WarmupSeconds,
        *Failure, *FString::Join(Results, TEXT(",\n")));
    FFileHelper::SaveStringToFile(Report, *FPaths::Combine(OutputDirectory, TEXT("성능요약.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UOVPerformanceProbe::Finish(AOVController& PC)
{
    PC.bSimulationPaused = true;
    bFinished = true;
    SaveReport(Failure.IsEmpty());
    FCsvProfiler::Get()->EndCapture();
    FinishAt = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Display, TEXT("OV PERF 완료: %s / 오류: %s"), *OutputDirectory, *Failure);
}

void UOVPerformanceProbe::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    const double Now = FPlatformTime::Seconds();
    if (bFinished)
    {
        // CSV 비동기 저장이 끝난 뒤 종료한다. UI 조작 검증과 구분되는 자동 진단 실행이다.
        if (Now-FinishAt > 3 && !FCsvProfiler::Get()->IsEndCapturePending()) FPlatformMisc::RequestExit(false);
        return;
    }
    AOVController* PC = Cast<AOVController>(GetWorld()->GetFirstPlayerController());
    if (!PC || PC->Ships.Num() < 3 || !PC->Carrier || !PC->Combat || !PC->Transport || !PC->PointDefense) return;
    if (!bStarted) { BeginRun(*PC, Now); return; }
    CSV_CUSTOM_STAT(OrbitalVanguard, CaseIndex, CaseIndex, ECsvCustomStatOp::Set);
    CSV_CUSTOM_STAT(OrbitalVanguard, StageIndex, bWarmup ? -1 : Stage, ECsvCustomStatOp::Set);
    CSV_CUSTOM_STAT(OrbitalVanguard, GameSpeed, Cases[CaseIndex].Y, ECsvCustomStatOp::Set);
    if (Now-LastSampleAt >= 1) { RecordCounts(*PC, Now); LastSampleAt=Now; }
    if (bWarmup)
    {
        if (Now-CaseStarted >= WarmupSeconds) { bWarmup=false; BeginStage(*PC, 0, Now); }
        return;
    }
    FrameMilliseconds.Add((Now-LastFrameAt)*1000.);
    LastFrameAt=Now;
    if (Now-StageStarted < RequestedSeconds/Cases.Num()*Fraction(Stage)) return;
    EndStage(*PC, Now);
    if (Stage < 2) { BeginStage(*PC, Stage+1, Now); return; }
    if (++CaseIndex < Cases.Num()) { BeginCase(*PC, Now); return; }
    Finish(*PC);
}

void UOVPerformanceProbe::Deinitialize()
{
    if (bStarted && !bFinished)
    {
        Failure = TEXT("요청 시간이 끝나기 전에 월드 종료");
        SaveReport(false);
        FCsvProfiler::Get()->EndCapture();
    }
    Super::Deinitialize();
}
