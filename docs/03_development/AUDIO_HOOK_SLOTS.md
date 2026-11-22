# 전투 · UI 오디오(SFX / BGM) 훅 슬롯 규약 (Orbital Vanguard · UE 5.8)

본 문서는 `UOVAudioSubsystem`(`OVAudioSubsystem.h/.cpp`)에 연결된 오디오 에셋 슬롯 경로 및 볼륨 채널 규약을 정의합니다.

## 안전 폴백 원칙
- 아래 경로에 `USoundBase`(`SoundWave`, `SoundCue`, `MetaSoundSource`) 에셋이 아직 임포트되지 않은 클린 환경에서도 `LOAD_NoWarn | LOAD_Quiet`로 탐색하며, 에셋이 없으면 **경고·크래시 없이 안전하게 스킵**합니다.
- `-NullRHI` 및 자동화 테스트 환경에서도 안전하게 동작하며, `GetLastTriggeredCue()` 및 `GetHookTriggerCount()`로 훅 호출 여부를 검증할 수 있습니다.

## 에셋 슬롯 경로 규약

| Enum (`EOVAudioCue`) | 에셋 경로 (`/Game/Audio/...`) | 채널 | 연결 호출 지점 |
| --- | --- | --- | --- |
| `TitleBGM` | `/Game/Audio/BGM/BGM_TitleTheme.BGM_TitleTheme` | `Master × BGM` | `UOVTitleScreenWidget::ActivateTitleScreen()` / `DeactivateTitleScreen()` |
| `UIClick` | `/Game/Audio/UI/SFX_UI_Click.SFX_UI_Click` | `Master × SFX` | `UOVTitleScreenWidget` 버튼 클릭 핸들러 |
| `LaserFire` | `/Game/Audio/Combat/SFX_Laser_Fire.SFX_Laser_Fire` | `Master × SFX` | `UOVCombatVFXSubsystem::SynchronizeVFX()` (신규 빔 발사 생성 시) |
| `CannonFire` | `/Game/Audio/Combat/SFX_Cannon_Fire.SFX_Cannon_Fire` | `Master × SFX` | `UOVCombatVFXSubsystem::SynchronizeVFX()` (신규 함포탄 생성 시) |
| `ShieldHit` | `/Game/Audio/Combat/SFX_Shield_Hit.SFX_Shield_Hit` | `Master × SFX` | `AOVShip::TriggerShieldImpact()` |
| `ShieldBreak` | `/Game/Audio/Combat/SFX_Shield_Break.SFX_Shield_Break` | `Master × SFX` | `UOVCombatVFXSubsystem::SynchronizeVFX()` (`bShieldDepleted == true`) |
| `WreckExplosion` | `/Game/Audio/Combat/SFX_Wreck_Explosion.SFX_Wreck_Explosion` | `Master × SFX` | `AOVShip::TriggerWreckDissolve()` 및 `UOVCombatVFXSubsystem` 폭발 이벤트 |

## 사용자 설정 저장 위치
- 파일: `GameUserSettings.ini` (`GGameUserSettingsIni`)
- 섹션: `[/Script/OrbitalVanguard.OVUserPreferences]`
- 키: `MasterVolume`, `BGMVolume`, `SFXVolume` (범위 `0.0 ~ 1.0`, 타이틀 환경 설정 모달 슬라이더와 양방향 동기화)
