#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVPerformanceProbe.generated.h"

class AOVController;

// -OVPerf가 있을 때만 생성되는 진단기. 일반 플레이에는 추가 Tick이 없다.
UCLASS()
class ORBITALVANGUARD_API UOVPerformanceProbe : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual TStatId GetStatId() const override;
    virtual void Tick(float DeltaTime) override;
    virtual void Deinitialize() override;
private:
    void BeginRun(AOVController& PC, double Now);
    void BeginCase(AOVController& PC, double Now);
    void BeginStage(AOVController& PC, int32 NextStage, double Now);
    void EndStage(AOVController& PC, double Now);
    void RecordCounts(AOVController& PC, double Now);
    void RecordMissionCounts(AOVController& PC, double Now);
    void SaveReport(bool bComplete);
    void Finish(AOVController& PC);

    bool bStarted = false;
    bool bFinished = false;
    bool bWarmup = true;
    bool bMissionRun = false;
    double RunStarted = 0;
    double CaseStarted = 0;
    double StageStarted = 0;
    double StageGameStarted = 0;
    double LastFrameAt = 0;
    double LastSampleAt = 0;
    double FinishAt = 0;
    double RequestedSeconds = 600;
    double WarmupSeconds = 3;
    int32 CaseIndex = 0;
    int32 Stage = 0;
    int32 BeginObjects = 0;
    uint64 BeginMemory = 0;
    int32 StartShots = 0;
    int32 StartHits = 0;
    int32 StartPDShots = 0;
    TArray<FIntPoint> Cases;
    TArray<double> FrameMilliseconds;
    TArray<FString> Results;
    TSet<int32> InitialUnitIds;
    TSet<int32> InitialPersonIds;
    TSet<int32> SeenUnitIds;
    TSet<int32> SeenPersonIds;
    TSet<int32> SeenPodIds;
    FString OutputDirectory;
    FString Label;
    FString Failure;
    FString UIMode=TEXT("play");
    int32 LastUIStage=-1;
};
