#include "OVAudioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"

namespace
{
constexpr TCHAR AudioConfigSection[] = TEXT("/Script/OrbitalVanguard.OVUserPreferences");
}

void UOVAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadSavedVolumes();
}

void UOVAudioSubsystem::Deinitialize()
{
    StopTitleBGM();
    CachedSounds.Reset();
    MissingAssetChecked.Reset();
    Super::Deinitialize();
}

const TCHAR* UOVAudioSubsystem::GetCueAssetPath(EOVAudioCue Cue)
{
    switch (Cue)
    {
    case EOVAudioCue::TitleBGM:       return TEXT("/Game/Audio/BGM/BGM_TitleTheme.BGM_TitleTheme");
    case EOVAudioCue::UIClick:        return TEXT("/Game/Audio/UI/SFX_UI_Click.SFX_UI_Click");
    case EOVAudioCue::LaserFire:      return TEXT("/Game/Audio/Combat/SFX_Laser_Fire.SFX_Laser_Fire");
    case EOVAudioCue::CannonFire:     return TEXT("/Game/Audio/Combat/SFX_Cannon_Fire.SFX_Cannon_Fire");
    case EOVAudioCue::ShieldHit:      return TEXT("/Game/Audio/Combat/SFX_Shield_Hit.SFX_Shield_Hit");
    case EOVAudioCue::ShieldBreak:    return TEXT("/Game/Audio/Combat/SFX_Shield_Break.SFX_Shield_Break");
    case EOVAudioCue::WreckExplosion: return TEXT("/Game/Audio/Combat/SFX_Wreck_Explosion.SFX_Wreck_Explosion");
    default:                          return TEXT("");
    }
}

UOVAudioSubsystem* UOVAudioSubsystem::Get(const UObject* WorldContext)
{
    if (!WorldContext) return nullptr;
    const UWorld* World = WorldContext->GetWorld();
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<UOVAudioSubsystem>() : nullptr;
}

USoundBase* UOVAudioSubsystem::ResolveSoundAsset(EOVAudioCue Cue)
{
    if (Cue == EOVAudioCue::None) return nullptr;
    if (TObjectPtr<USoundBase>* Found = CachedSounds.Find(Cue))
    {
        return Found->Get();
    }
    if (MissingAssetChecked.Contains(Cue))
    {
        return nullptr;
    }

    const TCHAR* Path = GetCueAssetPath(Cue);
    if (!Path || Path[0] == TEXT('\0'))
    {
        MissingAssetChecked.Add(Cue);
        return nullptr;
    }

    // Safe silent probe: never emits asset-not-found warnings or crashes when audio assets are absent.
    USoundBase* Loaded = LoadObject<USoundBase>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (Loaded)
    {
        CachedSounds.Add(Cue, Loaded);
        return Loaded;
    }

    MissingAssetChecked.Add(Cue);
    return nullptr;
}

void UOVAudioSubsystem::PlayTitleBGM()
{
    LastTriggeredCue = EOVAudioCue::TitleBGM;
    ++HookTriggerCount;

    if (!FApp::CanEverRender() || IsRunningDedicatedServer()) return;
    const float EffectiveVol = FMath::Clamp(MasterVolume * BGMVolume, 0.0f, 1.0f);
    if (EffectiveVol <= KINDA_SMALL_NUMBER) return;

    if (IsValid(ActiveBGMComponent) && ActiveBGMComponent->IsPlaying())
    {
        ActiveBGMComponent->SetVolumeMultiplier(EffectiveVol);
        return;
    }

    USoundBase* Sound = ResolveSoundAsset(EOVAudioCue::TitleBGM);
    UWorld* World = GetWorld();
    if (!Sound || !World) return;

    ActiveBGMComponent = UGameplayStatics::SpawnSound2D(World, Sound, EffectiveVol, 1.0f, 0.0f, nullptr, false, false);
}

void UOVAudioSubsystem::StopTitleBGM()
{
    if (IsValid(ActiveBGMComponent))
    {
        ActiveBGMComponent->Stop();
        ActiveBGMComponent = nullptr;
    }
}

void UOVAudioSubsystem::PlayUIClick()
{
    LastTriggeredCue = EOVAudioCue::UIClick;
    ++HookTriggerCount;

    if (!FApp::CanEverRender() || IsRunningDedicatedServer()) return;
    const float EffectiveVol = FMath::Clamp(MasterVolume * SFXVolume, 0.0f, 1.0f);
    if (EffectiveVol <= KINDA_SMALL_NUMBER) return;

    USoundBase* Sound = ResolveSoundAsset(EOVAudioCue::UIClick);
    UWorld* World = GetWorld();
    if (!Sound || !World) return;

    UGameplayStatics::PlaySound2D(World, Sound, EffectiveVol);
}

void UOVAudioSubsystem::PlayCombatSFX(EOVAudioCue Cue, FVector WorldLocation)
{
    if (Cue == EOVAudioCue::None) return;
    LastTriggeredCue = Cue;
    ++HookTriggerCount;

    if (!FApp::CanEverRender() || IsRunningDedicatedServer()) return;
    const float EffectiveVol = FMath::Clamp(MasterVolume * SFXVolume, 0.0f, 1.0f);
    if (EffectiveVol <= KINDA_SMALL_NUMBER) return;

    USoundBase* Sound = ResolveSoundAsset(Cue);
    UWorld* World = GetWorld();
    if (!Sound || !World) return;

    if (WorldLocation.IsNearlyZero())
    {
        UGameplayStatics::PlaySound2D(World, Sound, EffectiveVol);
    }
    else
    {
        UGameplayStatics::PlaySoundAtLocation(World, Sound, WorldLocation, EffectiveVol);
    }
}

void UOVAudioSubsystem::SetVolumes(float InMaster, float InBGM, float InSFX, bool bPersist)
{
    MasterVolume = FMath::Clamp(InMaster, 0.0f, 1.0f);
    BGMVolume = FMath::Clamp(InBGM, 0.0f, 1.0f);
    SFXVolume = FMath::Clamp(InSFX, 0.0f, 1.0f);
    UpdateActiveBGMVolume();
    if (bPersist)
    {
        SaveVolumes();
    }
}

void UOVAudioSubsystem::UpdateActiveBGMVolume()
{
    if (IsValid(ActiveBGMComponent))
    {
        ActiveBGMComponent->SetVolumeMultiplier(FMath::Clamp(MasterVolume * BGMVolume, 0.0f, 1.0f));
    }
}

void UOVAudioSubsystem::LoadSavedVolumes()
{
    if (!GConfig) return;
    float SavedMaster = MasterVolume;
    float SavedBGM = BGMVolume;
    float SavedSFX = SFXVolume;
    if (GConfig->GetFloat(AudioConfigSection, TEXT("MasterVolume"), SavedMaster, GGameUserSettingsIni))
    {
        MasterVolume = FMath::Clamp(SavedMaster, 0.0f, 1.0f);
    }
    if (GConfig->GetFloat(AudioConfigSection, TEXT("BGMVolume"), SavedBGM, GGameUserSettingsIni))
    {
        BGMVolume = FMath::Clamp(SavedBGM, 0.0f, 1.0f);
    }
    if (GConfig->GetFloat(AudioConfigSection, TEXT("SFXVolume"), SavedSFX, GGameUserSettingsIni))
    {
        SFXVolume = FMath::Clamp(SavedSFX, 0.0f, 1.0f);
    }
}

void UOVAudioSubsystem::SaveVolumes() const
{
    if (!GConfig) return;
    GConfig->SetFloat(AudioConfigSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
    GConfig->SetFloat(AudioConfigSection, TEXT("BGMVolume"), BGMVolume, GGameUserSettingsIni);
    GConfig->SetFloat(AudioConfigSection, TEXT("SFXVolume"), SFXVolume, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}
