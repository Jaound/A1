#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OVAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EOVAudioCue : uint8
{
    None,
    TitleBGM,
    UIClick,
    LaserFire,
    CannonFire,
    ShieldHit,
    ShieldBreak,
    WreckExplosion
};

/**
 * Lightweight, null-safe SFX/BGM hook subsystem.
 * Safely skips playback with zero warnings/crashes when /Game/Audio assets are not yet imported,
 * while tracking hook invocations and persisting Master/BGM/SFX volumes in GameUserSettings.ini.
 */
UCLASS()
class ORBITALVANGUARD_API UOVAudioSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static const TCHAR* GetCueAssetPath(EOVAudioCue Cue);
    static UOVAudioSubsystem* Get(const UObject* WorldContext);

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void PlayTitleBGM();

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void StopTitleBGM();

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void PlayUIClick();

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void PlayCombatSFX(EOVAudioCue Cue, FVector WorldLocation = FVector::ZeroVector);

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void SetVolumes(float InMaster, float InBGM, float InSFX, bool bPersist = true);

    UFUNCTION(BlueprintPure, Category = "Audio")
    float GetMasterVolume() const { return MasterVolume; }

    UFUNCTION(BlueprintPure, Category = "Audio")
    float GetBGMVolume() const { return BGMVolume; }

    UFUNCTION(BlueprintPure, Category = "Audio")
    float GetSFXVolume() const { return SFXVolume; }

    UFUNCTION(BlueprintPure, Category = "Audio")
    EOVAudioCue GetLastTriggeredCue() const { return LastTriggeredCue; }

    UFUNCTION(BlueprintPure, Category = "Audio")
    int32 GetHookTriggerCount() const { return HookTriggerCount; }

    void LoadSavedVolumes();
    void SaveVolumes() const;

private:
    USoundBase* ResolveSoundAsset(EOVAudioCue Cue);
    void UpdateActiveBGMVolume();

    UPROPERTY(Transient)
    TMap<EOVAudioCue, TObjectPtr<USoundBase>> CachedSounds;

    TSet<EOVAudioCue> MissingAssetChecked;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> ActiveBGMComponent;

    float MasterVolume = 0.85f;
    float BGMVolume = 0.70f;
    float SFXVolume = 0.85f;
    EOVAudioCue LastTriggeredCue = EOVAudioCue::None;
    int32 HookTriggerCount = 0;
};
