#include "OVController.h"
#include "OVShip.h"
#include "OVHUD.h"
#include "OVCalibration.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVDevelopmentSubsystem.h"
#include "OVHeroSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/InputSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AOVController::AOVController() { bShowMouseCursor=true; bEnableClickEvents=true; bEnableMouseOverEvents=true; }
void AOVController::BeginPlay()
{
 Super::BeginPlay();
 LegacyTraceDistance=HitResultTraceDistance;
 Combat=GetWorld()->GetSubsystem<UOVCombatSubsystem>();
 Carrier=GetWorld()->GetSubsystem<UOVCarrierSubsystem>();
 auto* AircraftMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Ships/FighterFull/SM_Hull.SM_Hull"),nullptr,LOAD_NoWarn);
 auto* AircraftMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Ships/FighterFull/M_Readable.M_Readable"),nullptr,LOAD_NoWarn);
 AircraftPresentationActor=GetWorld()->SpawnActor<AActor>();
 if(AircraftPresentationActor) {
  auto* ActorRoot=NewObject<USceneComponent>(AircraftPresentationActor,TEXT("PresentationRoot"));
  AircraftPresentationActor->AddInstanceComponent(ActorRoot);
  ActorRoot->SetMobility(EComponentMobility::Movable);
  ActorRoot->RegisterComponent();
  AircraftPresentationActor->SetRootComponent(ActorRoot);
  AircraftPresentationActor->SetActorHiddenInGame(false);
  AircraftPresentationActor->SetActorEnableCollision(false);
  auto CreateInstanced=[&](const TCHAR* Name,UStaticMesh* StaticMesh,UMaterialInterface* BaseMat,FLinearColor ParamColor,const TCHAR* ParamName,float Intensity=2.8f,float Opacity=0.9f) -> UInstancedStaticMeshComponent* {
   if(!StaticMesh) return nullptr;
   auto* Mesh=NewObject<UInstancedStaticMeshComponent>(AircraftPresentationActor,Name);
   AircraftPresentationActor->AddInstanceComponent(Mesh);
   Mesh->SetupAttachment(ActorRoot);
   Mesh->SetMobility(EComponentMobility::Movable);
   Mesh->SetStaticMesh(StaticMesh);
   Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Mesh->SetGenerateOverlapEvents(false);
   Mesh->SetCanEverAffectNavigation(false);
   Mesh->SetCastShadow(false);
   if(BaseMat) {
    if(auto* Material=UMaterialInstanceDynamic::Create(BaseMat,Mesh)) {
     Material->SetVectorParameterValue(ParamName,ParamColor);
     Material->SetScalarParameterValue(TEXT("Intensity"),Intensity);
     Material->SetScalarParameterValue(TEXT("Opacity"),Opacity);
     Mesh->SetMaterial(0,Material);
    }
   }
   Mesh->RegisterComponent();
   return Mesh;
  };
  if(AircraftMesh&&AircraftMaterial) {
   FriendlyAircraftMeshes=CreateInstanced(TEXT("FriendlyAircraftPresentation"),AircraftMesh,AircraftMaterial,FLinearColor(.8f,.9f,1),TEXT("FactionTint"));
   EnemyAircraftMeshes=CreateInstanced(TEXT("EnemyAircraftPresentation"),AircraftMesh,AircraftMaterial,FLinearColor(1,.72f,.7f),TEXT("FactionTint"));
  }
  auto* StdMissileMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Missiles/StandardMissile/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* StdMissileMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Missiles/StandardMissile/M_Source.M_Source"),nullptr,LOAD_NoWarn);
  auto* AntiShipMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Missiles/AntiShipMissile/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* AntiShipMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Missiles/AntiShipMissile/M_Source.M_Source"),nullptr,LOAD_NoWarn);
  auto* CannonMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Effects/CannonShell/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* CannonMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Effects/CannonShell/M_Source.M_Source"),nullptr,LOAD_NoWarn);
  auto* BeamMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Effects/LaserBeam/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* FriendlyBeamMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Effects/LaserBeam/M_Friendly.M_Friendly"),nullptr,LOAD_NoWarn);
  auto* EnemyBeamMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Effects/LaserBeam/M_Enemy.M_Enemy"),nullptr,LOAD_NoWarn);
  auto* BurstMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Effects/ImpactBurst/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* PlumeMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Effects/ImpactBurst/M_Plume.M_Plume"),nullptr,LOAD_NoWarn);
  auto* ShieldHitMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Effects/ImpactBurst/M_ShieldImpact.M_ShieldImpact"),nullptr,LOAD_NoWarn);
  auto* HullHitMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Effects/ImpactBurst/M_HullImpact.M_HullImpact"),nullptr,LOAD_NoWarn);
  StandardMissileMeshes=CreateInstanced(TEXT("StandardMissilePresentation"),StdMissileMesh,StdMissileMat,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"));
  AntiShipMissileMeshes=CreateInstanced(TEXT("AntiShipMissilePresentation"),AntiShipMesh,AntiShipMat,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"));
  CannonProjectileMeshes=CreateInstanced(TEXT("CannonProjectilePresentation"),CannonMesh,CannonMat,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"));
  FriendlyBeamMeshes=CreateInstanced(TEXT("FriendlyBeamPresentation"),BeamMesh,FriendlyBeamMat,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"));
  EnemyBeamMeshes=CreateInstanced(TEXT("EnemyBeamPresentation"),BeamMesh,EnemyBeamMat,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"));
  MissilePlumeMeshes=CreateInstanced(TEXT("MissilePlumePresentation"),BurstMesh,PlumeMat,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"));
  ShieldImpactMeshes=CreateInstanced(TEXT("ShieldImpactPresentation"),BurstMesh,ShieldHitMat,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"));
  HullImpactMeshes=CreateInstanced(TEXT("HullImpactPresentation"),BurstMesh,HullHitMat,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"));
  auto* SkyboxMeshAsset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Environment/SpaceSkybox/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* SkyboxMatAsset=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Environment/SpaceSkybox/M_Source.M_Source"),nullptr,LOAD_NoWarn);
  if(SkyboxMeshAsset && SkyboxMatAsset) {
   SpaceSkyboxMesh=NewObject<UStaticMeshComponent>(AircraftPresentationActor,TEXT("SpaceSkyboxPresentation"));
   AircraftPresentationActor->AddInstanceComponent(SpaceSkyboxMesh);
   SpaceSkyboxMesh->SetupAttachment(ActorRoot);
   SpaceSkyboxMesh->SetMobility(EComponentMobility::Movable);
   SpaceSkyboxMesh->SetStaticMesh(SkyboxMeshAsset);
   SpaceSkyboxMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   SpaceSkyboxMesh->SetGenerateOverlapEvents(false);
   SpaceSkyboxMesh->SetCanEverAffectNavigation(false);
   SpaceSkyboxMesh->SetCastShadow(false);
   if(auto* SkyDyn=UMaterialInstanceDynamic::Create(SkyboxMatAsset,SpaceSkyboxMesh)) {
    SkyDyn->SetVectorParameterValue(TEXT("FactionTint"),FLinearColor(0.85f,0.95f,1.15f,1.f));
    SkyDyn->SetScalarParameterValue(TEXT("Intensity"),1.0f);
    SpaceSkyboxMesh->SetMaterial(0,SkyDyn);
   }
   SpaceSkyboxMesh->SetWorldScale3D(FVector(1400.f));
   SpaceSkyboxMesh->RegisterComponent();
  }
  auto* SphereShape=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"),nullptr,LOAD_NoWarn);
  auto* PlanetMatAsset=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Environment/SpaceSkybox/M_Planet.M_Planet"),nullptr,LOAD_NoWarn);
  if(SphereShape && PlanetMatAsset) {
   DistantPlanetMesh=NewObject<UStaticMeshComponent>(AircraftPresentationActor,TEXT("DistantPlanetPresentation"));
   AircraftPresentationActor->AddInstanceComponent(DistantPlanetMesh);
   DistantPlanetMesh->SetupAttachment(ActorRoot);
   DistantPlanetMesh->SetMobility(EComponentMobility::Movable);
   DistantPlanetMesh->SetStaticMesh(SphereShape);
   DistantPlanetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   DistantPlanetMesh->SetGenerateOverlapEvents(false);
   DistantPlanetMesh->SetCanEverAffectNavigation(false);
   DistantPlanetMesh->SetCastShadow(false);
   if(auto* PlanetDyn=UMaterialInstanceDynamic::Create(PlanetMatAsset,DistantPlanetMesh)) {
    PlanetDyn->SetVectorParameterValue(TEXT("FactionTint"),FLinearColor(0.85f,0.95f,1.15f,1.f));
    DistantPlanetMesh->SetMaterial(0,PlanetDyn);
   }
   DistantPlanetMesh->RegisterComponent();
  }
  auto* StarPointMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Environment/SpaceSkybox/M_StarPoint.M_StarPoint"),nullptr,LOAD_NoWarn);
  StarfieldMeshes=CreateInstanced(TEXT("StarfieldPresentation"),SphereShape?SphereShape:BurstMesh,StarPointMat?StarPointMat:ShieldHitMat,FLinearColor(0.90f,0.96f,1.0f),TEXT("FactionTint"),3.4f,1.f);
  CachedStarTransforms.Reset();
  FRandomStream StarRng(20261118);
  for(int32 S=0;S<420;++S) {
   FVector Dir=StarRng.GetUnitVector();
   if(FMath::Abs(Dir.Z)>0.92f) Dir.Z*=0.5f;
   Dir=Dir.GetSafeNormal(UE_SMALL_NUMBER,FVector::ForwardVector);
   const int32 Layer=S<120?0:(S<280?1:2);
   const float Dist=Layer==0?StarRng.FRandRange(10000.f,17000.f):(Layer==1?StarRng.FRandRange(19000.f,32000.f):StarRng.FRandRange(34000.f,48000.f));
   const float StarScale=Layer==0?StarRng.FRandRange(0.06f,0.13f):(Layer==1?StarRng.FRandRange(0.12f,0.22f):StarRng.FRandRange(0.20f,0.36f));
   CachedStarTransforms.Emplace(FQuat::Identity,Dir*Dist,FVector(StarScale));
  }
  auto* AsteroidMeshAsset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Environment/Asteroid/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* AsteroidMatAsset=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Environment/Asteroid/M_Source.M_Source"),nullptr,LOAD_NoWarn);
  auto* BlackHoleMeshAsset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Environment/BlackHole/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* BlackHoleMatAsset=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Environment/BlackHole/M_Source.M_Source"),nullptr,LOAD_NoWarn);
  auto* HazardMeshAsset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/Environment/HazardZone/SM_Source.SM_Source"),nullptr,LOAD_NoWarn);
  auto* RadiationMatAsset=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Environment/HazardZone/M_Radiation.M_Radiation"),nullptr,LOAD_NoWarn);
  auto* SolarFlareMatAsset=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Environment/HazardZone/M_SolarFlare.M_SolarFlare"),nullptr,LOAD_NoWarn);
  AsteroidMeshes=CreateInstanced(TEXT("AsteroidEnvironmentPresentation"),AsteroidMeshAsset,AsteroidMatAsset,FLinearColor(1.f,1.f,1.f),TEXT("FactionTint"),1.0f,1.f);
  BlackHoleMeshes=CreateInstanced(TEXT("BlackHoleEnvironmentPresentation"),BlackHoleMeshAsset,BlackHoleMatAsset,FLinearColor(1.f,0.85f,0.65f),TEXT("FactionTint"),2.8f,1.f);
  RadiationZoneMeshes=CreateInstanced(TEXT("RadiationZoneEnvironmentPresentation"),HazardMeshAsset,RadiationMatAsset,FLinearColor(0.35f,1.f,0.75f),TEXT("FactionTint"),2.2f,0.32f);
  SolarFlareMeshes=CreateInstanced(TEXT("SolarFlareEnvironmentPresentation"),HazardMeshAsset,SolarFlareMatAsset,FLinearColor(1.f,0.68f,0.22f),TEXT("FactionTint"),3.0f,0.42f);
 }
 InitializeCampaignSystems();
 PointDefense=GetWorld()->GetSubsystem<UOVPointDefenseSubsystem>();
 Transport=GetWorld()->GetSubsystem<UOVTransportSubsystem>();
 Interior=GetWorld()->GetSubsystem<UOVInteriorSubsystem>();
 Mission=GetWorld()->GetSubsystem<UOVMissionSubsystem>();
 Development=GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>();
 Heroes=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
 FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
 RangeCamera=GetWorld()->SpawnActor<ACameraActor>();
 RangeCamera->GetCameraComponent()->SetFieldOfView(55);
 // Keep HUD and mouse coordinates in the same full viewport on ultrawide/resized windows.
 RangeCamera->GetCameraComponent()->bConstrainAspectRatio=false;
 RangeCamera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureMethod=true;
 RangeCamera->GetCameraComponent()->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
 RangeCamera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
 RangeCamera->GetCameraComponent()->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
 SetViewTarget(RangeCamera);
 auto Spawn=[this](const FString& Name,FVector Pos,bool Friendly,float Speed,float Turn,FVector Scale,FLinearColor Color) {
  AOVShip* Ship=GetWorld()->SpawnActor<AOVShip>(Pos,FRotator::ZeroRotator);
  Ship->Configure(Name,Friendly,Speed,Turn,Scale,Color); Ships.Add(Ship); return Ship;
 };
 Spawn(TEXT("01 / KESTREL - Escort"),FVector(-1000,-650,300),true,850,90,FVector(1.4,1.4,1.4),FLinearColor(0.1f,0.8f,1));
 Spawn(TEXT("02 / ATLAS - Cruiser"),FVector(-800,650,300),true,420,24,FVector(2.5,2.5,2.5),FLinearColor(0.3f,0.5f,1));
 Spawn(TEXT("03 / RED SIGNAL - Target"),FVector(1900,900,1100),false,0,0,FVector(1.8,1.8,1.8),FLinearColor(1,0.12f,0.08f));
 for(int32 Index=0;Index<Ships.Num();++Index) {
  const FString Path=FString::Printf(TEXT("/Game/Layouts/DA_Test%dDeck.DA_Test%dDeck"),Index+1,Index+1);
  UShipLayoutDataAsset* Layout=LoadObject<UShipLayoutDataAsset>(nullptr,*Path);
  if(!Layout) Layout=UShipLayoutDataAsset::MakeTestLayout(this,Index+1);
  Ships[Index]->Systems->FactionId=Ships[Index]->bFriendly?0:1;
  Ships[Index]->Systems->Initialize(Layout);
 }
 SelectShip(Ships[0]); ResetCameraProfile();
 UE_LOG(LogTemp,Display,TEXT("OV: ControlRange ready; two friendly ships + one target"));
 // Optional local play/viewport quality harness. Normal Editor/PIE startup is unchanged.
 int32 RequestedScene=-1;
 if(FParse::Value(FCommandLine::Get(),TEXT("OV2CScene="),RequestedScene)&&StartPhase2CRange(RequestedScene)) {
  if(FParse::Param(FCommandLine::Get(),TEXT("OVAutoRun"))) { for(AOVShip* Ship:Ships)Ship->SetAutomation(false,true);bSimulationPaused=false; }
 }
 int32 RequestedMission=-1;
 if(FParse::Value(FCommandLine::Get(),TEXT("OVMission="),RequestedMission) && RequestedMission>=0 && RequestedMission<=2)
  Mission->StartMission(this,static_cast<EOVMissionMode>(RequestedMission));
 FString RequestedPlayScene;
 if (FParse::Value(FCommandLine::Get(),TEXT("OVPlayScene="),RequestedPlayScene))
  StartPlayScenario(FName(*RequestedPlayScene));
}
void AOVController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 ShieldBubbleMeshes.Reset();
 DeathThroes.Reset();
 if(IsValid(AircraftPresentationActor))AircraftPresentationActor->Destroy();
 Super::EndPlay(EndPlayReason);
}
FString AOVController::GetEnvironmentDisplayName(EOVSpaceEnvironmentType Env)
{
 switch(Env) {
  case EOVSpaceEnvironmentType::AsteroidField: return TEXT("운석지대");
  case EOVSpaceEnvironmentType::RadiationZone: return TEXT("우주 방사능 지대");
  case EOVSpaceEnvironmentType::SolarFlare: return TEXT("태양 플레어");
  case EOVSpaceEnvironmentType::BlackHole: return TEXT("소규모 블랙홀");
  default: return TEXT("안정 성계 (제약 없음)");
 }
}
FString AOVController::GetOutpostDisplayName(EOVOutpostKind Kind)
{
 switch(Kind) {
  case EOVOutpostKind::Basic: return TEXT("기본 전초기지");
  case EOVOutpostKind::MiningHub: return TEXT("자원 채굴 기지");
  case EOVOutpostKind::CombatBase: return TEXT("전투 기지");
  default: return TEXT("없음");
 }
}
void AOVController::InitializeCampaignSystems()
{
 StarSystems.Reset();
 auto AddSys=[&](FName Id,const TCHAR* Name,const TCHAR* OwnerFactionId,const TCHAR* EnemyName,FVector2D Pos,
  EOVSpaceEnvironmentType Env,EOVOutpostKind Outpost,int32 OutHP,int32 C,int32 A,int32 S,int32 Danger,FLinearColor Tint,std::initializer_list<const TCHAR*> Links) {
  FOVStarSystemNode Node;
  Node.SystemId=Id; Node.DisplayName=Name; Node.OwnerFaction=OwnerFactionId; Node.EnemyFactionName=EnemyName;
  Node.MapNormalizedPos=Pos; Node.Environment=Env; Node.Outpost=Outpost; Node.OutpostHP=OutHP; Node.OutpostMaxHP=OutHP;
  Node.CommonYield=C; Node.AdvancedYield=A; Node.SupremeYield=S; Node.DangerLevel=Danger; Node.NebulaTint=Tint;
  for(const TCHAR* L:Links) Node.Links.Add(FName(L));
  StarSystems.Add(Node);
 };
 AddSys(TEXT("home_vanguard"),TEXT("선봉 본기지 성계"),TEXT("player"),TEXT(""),FVector2D(0.14f,0.50f),
  EOVSpaceEnvironmentType::None,EOVOutpostKind::CombatBase,2600,16,8,2,1,FLinearColor(0.85f,0.95f,1.15f),
  {TEXT("vega_mining"),TEXT("procyon_border")});
 AddSys(TEXT("vega_mining"),TEXT("베가 자원 지대"),TEXT("player"),TEXT(""),FVector2D(0.31f,0.24f),
  EOVSpaceEnvironmentType::AsteroidField,EOVOutpostKind::MiningHub,1300,24,8,1,1,FLinearColor(1.1f,0.95f,0.78f),
  {TEXT("home_vanguard"),TEXT("orion_cross"),TEXT("kronos_nebula")});
 AddSys(TEXT("procyon_border"),TEXT("프로키온 변경"),TEXT("neutral"),TEXT(""),FVector2D(0.31f,0.76f),
  EOVSpaceEnvironmentType::SolarFlare,EOVOutpostKind::None,0,14,10,1,2,FLinearColor(1.2f,0.88f,0.65f),
  {TEXT("home_vanguard"),TEXT("orion_cross"),TEXT("arcadia_rim")});
 AddSys(TEXT("orion_cross"),TEXT("오리온 성계 교차로"),TEXT("enemy"),TEXT("블랙팽 해적단"),FVector2D(0.50f,0.50f),
  EOVSpaceEnvironmentType::AsteroidField,EOVOutpostKind::Basic,900,16,12,3,2,FLinearColor(0.82f,1.05f,1.15f),
  {TEXT("vega_mining"),TEXT("procyon_border"),TEXT("kronos_nebula"),TEXT("arcadia_rim"),TEXT("blackfang_den")});
 AddSys(TEXT("kronos_nebula"),TEXT("크로노스 희귀 성운"),TEXT("neutral"),TEXT(""),FVector2D(0.66f,0.22f),
  EOVSpaceEnvironmentType::RadiationZone,EOVOutpostKind::None,0,10,16,5,3,FLinearColor(0.75f,1.18f,0.95f),
  {TEXT("vega_mining"),TEXT("orion_cross"),TEXT("ironclad_core")});
 AddSys(TEXT("arcadia_rim"),TEXT("아르카디아 변경"),TEXT("enemy"),TEXT("블랙팽 해적단"),FVector2D(0.66f,0.78f),
  EOVSpaceEnvironmentType::BlackHole,EOVOutpostKind::MiningHub,1200,18,11,2,2,FLinearColor(0.95f,0.78f,1.22f),
  {TEXT("procyon_border"),TEXT("orion_cross"),TEXT("blackfang_den")});
 AddSys(TEXT("blackfang_den"),TEXT("블랙팽 약탈 소굴"),TEXT("enemy"),TEXT("블랙팽 해적단"),FVector2D(0.83f,0.65f),
  EOVSpaceEnvironmentType::RadiationZone,EOVOutpostKind::CombatBase,2200,14,14,6,3,FLinearColor(1.22f,0.75f,0.75f),
  {TEXT("orion_cross"),TEXT("arcadia_rim"),TEXT("ironclad_core")});
 AddSys(TEXT("ironclad_core"),TEXT("아이언 클라드 요새"),TEXT("enemy"),TEXT("아이언 클라드 군벌"),FVector2D(0.87f,0.30f),
  EOVSpaceEnvironmentType::BlackHole,EOVOutpostKind::CombatBase,2800,20,18,8,4,FLinearColor(1.15f,0.82f,1.10f),
  {TEXT("kronos_nebula"),TEXT("blackfang_den")});
 CurrentStarSystemId=TEXT("home_vanguard");
}
void AOVController::ResetDeathThroes()
{
 for(auto& Pair:DeathThroes) {
  AOVShip* Ship=Pair.Key.Get();
  if(!IsValid(Ship)||!Ship->Hull) continue;
  if(Pair.Value.bHasBaseTransform) {
   Ship->Hull->SetRelativeLocation(Pair.Value.BaseRelLocation);
   Ship->Hull->SetRelativeRotation(Pair.Value.BaseRelRotation);
  }
  Ship->Hull->SetVisibility(true);
  TInlineComponentArray<UOVWeaponVisualComponent*> Visuals(Ship);
  for(UOVWeaponVisualComponent* V:Visuals) if(IsValid(V)) V->SetVisibility(true,true);
 }
 DeathThroes.Reset();
}
bool AOVController::IsShipInDeathThroe(const AOVShip* Ship, float& OutRemaining) const
{
 if(const FOVDeathThroeState* DT=DeathThroes.Find(const_cast<AOVShip*>(Ship))) {
  if(DT->bActive) { OutRemaining=DT->Remaining; return true; }
 }
 OutRemaining=0.f; return false;
}
bool AOVController::IsShipVanished(const AOVShip* Ship) const
{
 if(const FOVDeathThroeState* DT=DeathThroes.Find(const_cast<AOVShip*>(Ship))) return DT->bVanished;
 return false;
}
bool AOVController::IsShipShelteredFromSolarFlare(const AOVShip* Ship) const
{
 if(!IsValid(Ship)) return false;
 const FVector Pos=Ship->GetActorLocation();
 for(const FVector4& Ast:ActiveAsteroids) {
  if(Ast.X<=Pos.X || (Ast.X-Pos.X)>2600.f*(CalibrationScenario>=0?20.f:1.f)) continue;
  const float LateralDist=FVector2D::Distance(FVector2D(Pos.Y,Pos.Z),FVector2D(Ast.Y,Ast.Z));
  if(LateralDist<=Ast.W*1.25f) return true;
 }
 return false;
}
void AOVController::SetSpaceEnvironment(EOVSpaceEnvironmentType NewEnvironment)
{
 ActiveEnvironment=NewEnvironment;
 ActiveAsteroids.Reset();
 ActiveRadiationZones.Reset();
 EnvironmentTickAccum=0.f;
 SolarFlareCycleTime=0.f;
 bSolarFlareWaveApplied=false;
 const float Scale=CalibrationScenario>=0?20.f:1.f;
 ActiveBlackHoleCenter=FVector(0.f,0.f,280.f*Scale);
 ActiveBlackHoleRadius=2500.f*Scale;
 if(NewEnvironment==EOVSpaceEnvironmentType::AsteroidField) {
  const FVector4 PresetAsteroids[]={
   FVector4(-420,-180,300,240),FVector4(-150,420,340,260),FVector4(220,-520,280,230),
   FVector4(380,260,310,275),FVector4(-850,1150,420,290),FVector4(920,-1050,360,310),
   FVector4(-1250,-1350,240,260),FVector4(1180,1220,480,285),FVector4(40,-1280,320,250),
   FVector4(-60,1380,260,270),FVector4(-1950,120,520,300),FVector4(2050,-680,440,280),
   FVector4(-620,-780,620,210),FVector4(680,820,160,220)
  };
  for(const FVector4& A:PresetAsteroids) {
   const FVector Pos(A.X*Scale,A.Y*Scale,A.Z*(CalibrationScenario>=0?6.f:1.f));
   const float Rad=A.W*Scale;
   bool bOverlapShip=false;
   for(const AOVShip* Ship:Ships) if(IsValid(Ship) && FVector::Dist(Ship->GetActorLocation(),Pos)<Rad+360.f*Scale) { bOverlapShip=true; break; }
   if(!bOverlapShip) ActiveAsteroids.Emplace(Pos.X,Pos.Y,Pos.Z,Rad);
  }
 } else if(NewEnvironment==EOVSpaceEnvironmentType::SolarFlare) {
  const FVector4 CoverAsteroids[]={
   FVector4(-320,-550,300,320),FVector4(-280,620,320,340),
   FVector4(620,-420,340,310),FVector4(580,520,280,330)
  };
  for(const FVector4& A:CoverAsteroids) {
   const FVector Pos(A.X*Scale,A.Y*Scale,A.Z*(CalibrationScenario>=0?6.f:1.f));
   ActiveAsteroids.Emplace(Pos.X,Pos.Y,Pos.Z,A.W*Scale);
  }
 } else if(NewEnvironment==EOVSpaceEnvironmentType::RadiationZone) {
  ActiveRadiationZones.Emplace(-350.f*Scale,-320.f*Scale,300.f*(CalibrationScenario>=0?6.f:1.f),1150.f*Scale);
  ActiveRadiationZones.Emplace(780.f*Scale,520.f*Scale,340.f*(CalibrationScenario>=0?6.f:1.f),1050.f*Scale);
 }
 if(Combat) {
  Combat->EnvironmentAsteroids=ActiveAsteroids;
  Combat->EnvironmentRadarDisruptRemaining=0.f;
 }
 LastAction=FString::Printf(TEXT("우주 환경 적용: %s"),*GetEnvironmentDisplayName(NewEnvironment));
}
bool AOVController::SwitchStarSystem(FName SystemId)
{
 const FOVStarSystemNode* Node=StarSystems.FindByPredicate([SystemId](const FOVStarSystemNode& N){return N.SystemId==SystemId;});
 if(!Node) return false;
 CurrentStarSystemId=SystemId;
 SetSpaceEnvironment(Node->Environment);
 if(SpaceSkyboxMesh) {
  if(auto* SkyDyn=Cast<UMaterialInstanceDynamic>(SpaceSkyboxMesh->GetMaterial(0)))
   SkyDyn->SetVectorParameterValue(TEXT("FactionTint"),Node->NebulaTint);
 }
 if(DistantPlanetMesh) {
  if(auto* PlanetDyn=Cast<UMaterialInstanceDynamic>(DistantPlanetMesh->GetMaterial(0)))
   PlanetDyn->SetVectorParameterValue(TEXT("FactionTint"),Node->NebulaTint);
 }
 LastAction=FString::Printf(TEXT("성계 전환: %s · 환경: %s"),*Node->DisplayName,*GetEnvironmentDisplayName(Node->Environment));
 return true;
}
bool AOVController::DispatchFleetFTL(FName TargetSystemId)
{
 FOVStarSystemNode* TargetNode=StarSystems.FindByPredicate([TargetSystemId](const FOVStarSystemNode& N){return N.SystemId==TargetSystemId;});
 const FOVStarSystemNode* CurrentNode=StarSystems.FindByPredicate([this](const FOVStarSystemNode& N){return N.SystemId==CurrentStarSystemId;});
 if(!TargetNode || TargetSystemId==CurrentStarSystemId) return false;
 if(CurrentNode && !CurrentNode->Links.Contains(TargetSystemId)) {
  LastAction=TEXT("FTL 항로 미연결 · 인접 성계로만 도약 가능");
  return false;
 }
 SwitchStarSystem(TargetSystemId);
 if(TargetNode->OwnerFaction==TEXT("neutral")) {
  TargetNode->OwnerFaction=TEXT("player");
  LastAction=FString::Printf(TEXT("FTL 도약 완료: %s 확보 · 환경: %s"),*TargetNode->DisplayName,*GetEnvironmentDisplayName(TargetNode->Environment));
 } else if(TargetNode->OwnerFaction==TEXT("enemy")) {
  LastAction=FString::Printf(TEXT("FTL 강습 진입: %s (%s) · 환경: %s"),*TargetNode->DisplayName,*TargetNode->EnemyFactionName,*GetEnvironmentDisplayName(TargetNode->Environment));
 } else {
  LastAction=FString::Printf(TEXT("FTL 이동 완료: %s · 환경: %s"),*TargetNode->DisplayName,*GetEnvironmentDisplayName(TargetNode->Environment));
 }
 return true;
}
bool AOVController::BuildOutpost(FName SystemId, EOVOutpostKind Kind)
{
 FOVStarSystemNode* Node=StarSystems.FindByPredicate([SystemId](const FOVStarSystemNode& N){return N.SystemId==SystemId;});
 if(!Node || Kind==EOVOutpostKind::None) return false;
 if(Node->OwnerFaction==TEXT("enemy")) {
  LastAction=TEXT("적 점령 성계에는 전초기지를 건설할 수 없습니다");
  return false;
 }
 const int32 CostC=Kind==EOVOutpostKind::Basic?80:(Kind==EOVOutpostKind::MiningHub?110:140);
 const int32 CostA=Kind==EOVOutpostKind::Basic?20:(Kind==EOVOutpostKind::MiningHub?45:65);
 const int32 CostS=Kind==EOVOutpostKind::Basic?0:(Kind==EOVOutpostKind::MiningHub?5:12);
 if(ResourceCommon<CostC || ResourceAdvanced<CostA || ResourceSupreme<CostS) {
  LastAction=FString::Printf(TEXT("자원 부족 · 필요: 일반 %d / 고급 %d / 최고급 %d"),CostC,CostA,CostS);
  return false;
 }
 ResourceCommon-=CostC; ResourceAdvanced-=CostA; ResourceSupreme-=CostS;
 Node->OwnerFaction=TEXT("player");
 Node->Outpost=Kind;
 Node->OutpostMaxHP=Kind==EOVOutpostKind::Basic?900:(Kind==EOVOutpostKind::MiningHub?1300:2400);
 Node->OutpostHP=Node->OutpostMaxHP;
 LastAction=FString::Printf(TEXT("%s에 %s 건설 완료"),*Node->DisplayName,*GetOutpostDisplayName(Kind));
 return true;
}
void AOVController::UpdateAircraftPresentation()
{
 if(!FriendlyAircraftMeshes||!EnemyAircraftMeshes||!Carrier)return;
 TArray<FTransform> Friendly,Enemy;
 for(const auto& Aircraft:Carrier->Aircraft) {
  if(!UOVCarrierSubsystem::IsAirborneState(Aircraft.State)||Aircraft.Position.ContainsNaN()||Aircraft.Direction.ContainsNaN())continue;
  const FVector Direction=Aircraft.Direction.GetSafeNormal(UE_SMALL_NUMBER,FVector::ForwardVector);
  const FQuat Rotation=Direction.Rotation().Quaternion()*FRotator(0,180,0).Quaternion();
  const float Scale=FMath::Max(1.f,Carrier->Profile.CollisionRadius)*2.f/350.f;
  (Aircraft.FactionId==0?Friendly:Enemy).Emplace(Rotation,Aircraft.Position,FVector(Scale));
 }
 auto Sync=[](UInstancedStaticMeshComponent* Mesh,const TArray<FTransform>& Transforms) {
  if(!Mesh) return;
  while(Mesh->GetInstanceCount()>Transforms.Num())Mesh->RemoveInstance(Mesh->GetInstanceCount()-1);
  for(int32 I=Mesh->GetInstanceCount();I<Transforms.Num();++I)Mesh->AddInstance(Transforms[I],true);
  if(!Transforms.IsEmpty())Mesh->BatchUpdateInstancesTransforms(0,Transforms,true,true,true);
 };
 Sync(FriendlyAircraftMeshes,Friendly);Sync(EnemyAircraftMeshes,Enemy);
}
void AOVController::UpdateCombatPresentation()
{
 if(!IsValid(AircraftPresentationActor)) return;
 auto SyncInstances=[](UInstancedStaticMeshComponent* Mesh,const TArray<FTransform>& Transforms) {
  if(!Mesh) return;
  while(Mesh->GetInstanceCount()>Transforms.Num())Mesh->RemoveInstance(Mesh->GetInstanceCount()-1);
  for(int32 I=Mesh->GetInstanceCount();I<Transforms.Num();++I)Mesh->AddInstance(Transforms[I],true);
  if(!Transforms.IsEmpty())Mesh->BatchUpdateInstancesTransforms(0,Transforms,true,true,true);
 };
 auto AddSegmentCylinder=[](TArray<FTransform>& Out,FVector Start,FVector End,float Radius) {
  const FVector Delta=End-Start;
  const double Dist=Delta.Size();
  if(Dist<1.0 || Start.ContainsNaN() || End.ContainsNaN()) return;
  const FQuat Rot=FRotationMatrix::MakeFromZ(Delta/Dist).ToQuat();
  Out.Emplace(Rot,(Start+End)*0.5,FVector(Radius/50.0,Radius/50.0,Dist/100.0));
 };
 // 1. 3D Shield Bubbles per ship (matching prototype shield_bubble.gdshader)
 static TWeakObjectPtr<UStaticMesh> CachedSphereMesh;
 static TWeakObjectPtr<UMaterialInterface> CachedShieldMat;
 if(!CachedSphereMesh.IsValid()) CachedSphereMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"),nullptr,LOAD_NoWarn);
 if(!CachedShieldMat.IsValid()) CachedShieldMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Presentation/Effects/M_ShieldBubble.M_ShieldBubble"),nullptr,LOAD_NoWarn);
 for(auto It=ShieldBubbleMeshes.CreateIterator();It;++It) {
  if(!It.Key().IsValid()||!Ships.Contains(It.Key().Get())) {
   if(IsValid(It.Value())) It.Value()->DestroyComponent();
   It.RemoveCurrent();
  }
 }
 for(AOVShip* Ship:Ships) {
  if(!IsValid(Ship)||!Ship->Systems) continue;
  TObjectPtr<UStaticMeshComponent>& Bubble=ShieldBubbleMeshes.FindOrAdd(Ship);
  if(!IsValid(Bubble) && CachedSphereMesh.IsValid() && CachedShieldMat.IsValid()) {
   Bubble=NewObject<UStaticMeshComponent>(AircraftPresentationActor);
   AircraftPresentationActor->AddInstanceComponent(Bubble);
   Bubble->SetupAttachment(AircraftPresentationActor->GetRootComponent());
   Bubble->SetMobility(EComponentMobility::Movable);
   Bubble->SetStaticMesh(CachedSphereMesh.Get());
   Bubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Bubble->SetCollisionResponseToAllChannels(ECR_Ignore);
   Bubble->SetGenerateOverlapEvents(false);
   Bubble->SetCanEverAffectNavigation(false);
   Bubble->SetCastShadow(false);
   Bubble->SetMaterial(0,UMaterialInstanceDynamic::Create(CachedShieldMat.Get(),Bubble));
   Bubble->RegisterComponent();
  }
  if(!IsValid(Bubble)) continue;
  const bool bDeparted=Combat&&Combat->bEnabled&&Combat->IsDeparted(Ship);
  const bool bVisible=!Ship->IsHidden()&&!bDeparted&&!Ship->Systems->bWrecked&&Ship->Systems->bShieldActive&&Ship->Systems->Shield>0.5f;
  Bubble->SetVisibility(bVisible);
  if(bVisible) {
   const FVector Extent=Ship->Systems->Layout?Ship->Systems->Layout->LocalHalfExtent:FVector(175,75,30);
   Bubble->SetWorldLocationAndRotation(Ship->GetActorLocation(),Ship->GetActorRotation());
   Bubble->SetWorldScale3D(Extent*Ship->GetActorScale3D()*FVector(1.22,1.32,1.42)/50.0);
   if(auto* Dyn=Cast<UMaterialInstanceDynamic>(Bubble->GetMaterial(0))) {
    const float Ratio=FMath::Clamp(Ship->Systems->Shield/FMath::Max(1.f,Ship->Systems->MaxShield),0.15f,1.f);
    float HitBoost=0.f;
    if(Combat&&Combat->bEnabled) {
     for(int32 I=Combat->RecentEvents.Num()-1;I>=0;--I) {
      const auto& E=Combat->RecentEvents[I];
      if(Combat->GameTime-E.ResolvedAt>0.28) break;
      if(E.HitShipId==Ship->GetFName()&&E.Result==EOVAttackResult::Hit) {
       HitBoost=FMath::Max(HitBoost,0.38f*(1.f-float((Combat->GameTime-E.ResolvedAt)/0.28)));
      }
     }
    }
    Dyn->SetVectorParameterValue(TEXT("ShieldColor"),Ship->bFriendly?FLinearColor(0.22f,0.72f,1.0f,1.f):FLinearColor(1.0f,0.35f,0.22f,1.f));
    Dyn->SetScalarParameterValue(TEXT("ShieldAlpha"),FMath::Clamp(0.18f+Ratio*0.28f+HitBoost,0.12f,0.85f));
   }
  }
 }
 // 2. 3D Missiles (Standard vs Anti-Ship), Cannon Bolts, Laser Beams, and Impact Effects
 TArray<FTransform> StdMissiles,AntiShipMissiles,CannonBolts,FriendlyBeams,EnemyBeams,Plumes,ShieldHits,HullHits;
 const float FX=CalibrationScenario>=0?14.f:1.f;
 if(Combat&&Combat->bEnabled) {
  for(const auto& Shot:Combat->Attacks) {
   if(Shot.Position.ContainsNaN()||Shot.Direction.ContainsNaN()) continue;
   const FVector Dir=Shot.Direction.GetSafeNormal(UE_SMALL_NUMBER,FVector::ForwardVector);
   if(Shot.Weapon.Kind==EOVAttackKind::Cannon) {
    const float BoltLen=FMath::Clamp(Shot.Weapon.Speed*0.005f,55.f*FX,140.f*FX);
    const float BoltRad=BoltLen*0.26f;
    AddSegmentCylinder(CannonBolts,Shot.Position-Dir*(BoltLen*0.8f),Shot.Position+Dir*(BoltLen*0.2f),BoltRad);
   } else {
    const FQuat Rot=Dir.Rotation().Quaternion()*FRotator(0,180,0).Quaternion();
    const float ScaleFactor=CalibrationScenario>=0?1.f:0.12f;
    if(Shot.bAntiShipMissile) {
     const float S=0.30f*ScaleFactor;
     AntiShipMissiles.Emplace(Rot,Shot.Position,FVector(S));
     Plumes.Emplace(Rot,Shot.Position-Dir*(115.f*ScaleFactor),FVector(0.55f*ScaleFactor,0.24f*ScaleFactor,0.24f*ScaleFactor));
    } else {
     const float S=0.13f*ScaleFactor;
     StdMissiles.Emplace(Rot,Shot.Position,FVector(S));
     Plumes.Emplace(Rot,Shot.Position-Dir*(85.f*ScaleFactor),FVector(0.42f*ScaleFactor,0.18f*ScaleFactor,0.18f*ScaleFactor));
    }
   }
  }
  for(const auto& Event:Combat->RecentEvents) {
   const double Age=Combat->GameTime-Event.ResolvedAt;
   if(Age<0 || Age>=0.22) continue;
   const float Alpha=FMath::Clamp(1.f-float(Age/0.22),0.1f,1.f);
   FVector ImpactPos=Event.Position;
   if(Event.Result==EOVAttackResult::Hit && Event.HullDamage<=0.f && !Event.bShieldDepleted) {
    for(AOVShip* HitShip:Ships) if(IsValid(HitShip) && HitShip->GetFName()==Event.HitShipId && HitShip->Systems && HitShip->Systems->Layout) {
     const FTransform ShipXf=HitShip->GetActorTransform();
     const FVector Radii=HitShip->Systems->Layout->LocalHalfExtent*FVector(1.22f,1.32f,1.42f);
     const FVector InvR(1.0/FMath::Max(1.0,Radii.X),1.0/FMath::Max(1.0,Radii.Y),1.0/FMath::Max(1.0,Radii.Z));
     const FVector LocalHit=ShipXf.InverseTransformPosition(ImpactPos)*InvR;
     if(LocalHit.SizeSquared()<0.98) {
      const FVector S=ShipXf.InverseTransformPosition(Event.Origin)*InvR;
      const FVector D=LocalHit-S;
      const double A=D.SizeSquared(), B=FVector::DotProduct(S,D), C=S.SizeSquared()-1.0, Disc=B*B-A*C;
      if(A>1.e-9 && Disc>=0.0) {
       const double T=(-B-FMath::Sqrt(Disc))/A;
       if(T>=0.0 && T<=1.5) ImpactPos=ShipXf.TransformPosition((S+D*T)*Radii);
      }
     }
     break;
    }
   }
   if(Event.Weapon.Kind==EOVAttackKind::Beam) {
    auto& BeamList=Event.FactionId==0?FriendlyBeams:EnemyBeams;
    AddSegmentCylinder(BeamList,Event.Origin,ImpactPos,5.2f*FX*Alpha);
   }
   if(Event.Result==EOVAttackResult::Hit && !ImpactPos.ContainsNaN()) {
    const float ImpactRadius=(16.f+(1.f-Alpha)*32.f)*FX;
    const FVector ImpactScale(ImpactRadius/50.f);
    auto& TargetHits=(Event.HullDamage<=0.f && !Event.bShieldDepleted)?ShieldHits:HullHits;
    TargetHits.Emplace(FQuat::Identity,ImpactPos,ImpactScale);
   }
  }
 }
 if(PointDefense&&PointDefense->bEnabled) {
  for(const auto& E:PointDefense->RecentEvents) {
   const double Age=PointDefense->GameTime-E.GameTime;
   if(Age<0 || Age>=0.16) continue;
   const float Alpha=FMath::Clamp(1.f-float(Age/0.16),0.15f,1.f);
   auto& BeamList=E.DefenderFaction==0?FriendlyBeams:EnemyBeams;
   AddSegmentCylinder(BeamList,E.Origin,E.TargetPosition,2.2f*FX*Alpha);
   if(E.bHit && !E.TargetPosition.ContainsNaN()) {
    HullHits.Emplace(FQuat::Identity,E.TargetPosition,FVector(8.f*FX*Alpha/50.f));
   }
  }
 }
 // 3. Ship Death Throe (완파 침몰·연쇄 폭발·소멸 연출 — prototype ShipState.gd / SpaceViewport.gd)
 const float PresentationDt=bSimulationPaused?0.f:FMath::Clamp(float(FApp::GetDeltaTime())*SimulationSpeed,0.f,0.1f);
 const double NowReal=FPlatformTime::Seconds();
 for(AOVShip* Ship:Ships) {
  if(!IsValid(Ship)||!Ship->Systems||!Ship->Hull) continue;
  if(!Ship->Systems->bWrecked) {
   if(FOVDeathThroeState* Existing=DeathThroes.Find(Ship)) {
    if(Existing->bHasBaseTransform) {
     Ship->Hull->SetRelativeLocation(Existing->BaseRelLocation);
     Ship->Hull->SetRelativeRotation(Existing->BaseRelRotation);
    }
    Ship->Hull->SetVisibility(true);
    TInlineComponentArray<UOVWeaponVisualComponent*> Visuals(Ship);
    for(UOVWeaponVisualComponent* V:Visuals) if(IsValid(V)) V->SetVisibility(true,true);
    DeathThroes.Remove(Ship);
   }
   continue;
  }
  FOVDeathThroeState& DT=DeathThroes.FindOrAdd(Ship);
  if(!DT.bActive && !DT.bVanished) {
   DT.bActive=true;
   DT.bHasBaseTransform=true;
   DT.BaseRelLocation=Ship->Hull->GetRelativeLocation();
   DT.BaseRelRotation=Ship->Hull->GetRelativeRotation();
   const bool bSmall=Ship->Systems->Layout && Ship->Systems->Layout->bSmallHull;
   const int32 Decks=Ship->Systems->Layout?Ship->Systems->Layout->DeckCount:2;
   DT.TotalDuration=bSmall?0.35f:(Decks<=2?2.6f:(Decks==3?3.1f:3.5f));
   DT.Remaining=DT.TotalDuration;
   DT.BurstAccum=0.f;
   DT.RollDirection=(GetTypeHash(Ship->GetFName())%2==0)?1.f:-1.f;
   DT.ExtraBursts.Emplace(Ship->GetActorLocation(),NowReal);
  }
  if(DT.bActive) {
   DT.Remaining=FMath::Max(0.f,DT.Remaining-PresentationDt);
   DT.BurstAccum+=PresentationDt;
   const float Progress=FMath::Clamp(1.f-(DT.Remaining/FMath::Max(0.01f,DT.TotalDuration)),0.f,1.f);
   const float Ease=Progress*Progress;
   const float SinkZ=-Ease*180.f*(CalibrationScenario>=0?10.f:1.f);
   const FRotator ThroeRot(-Ease*25.f,0.f,DT.RollDirection*Ease*40.f);
   Ship->Hull->SetRelativeLocation(DT.BaseRelLocation+FVector(0.f,0.f,SinkZ));
   Ship->Hull->SetRelativeRotation((FQuat(ThroeRot)*FQuat(DT.BaseRelRotation)).Rotator());
   const FVector HullExt=Ship->Systems->Layout?(Ship->Systems->Layout->LocalHalfExtent*Ship->GetActorScale3D()):FVector(180,80,40);
   while(DT.BurstAccum>=0.22f) {
    DT.BurstAccum-=0.22f;
    const FVector LocalRand(FMath::FRandRange(-0.85f,0.85f)*HullExt.X,FMath::FRandRange(-0.75f,0.75f)*HullExt.Y,FMath::FRandRange(-0.5f,0.8f)*HullExt.Z);
    DT.ExtraBursts.Emplace(Ship->GetActorLocation()+Ship->GetActorQuat().RotateVector(LocalRand)+FVector(0,0,SinkZ),NowReal);
   }
   if(DT.Remaining<=0.f) {
    DT.bActive=false;
    DT.bVanished=true;
    for(int32 B=-1;B<=1;++B) {
     DT.ExtraBursts.Emplace(Ship->GetActorLocation()+Ship->GetActorQuat().RotateVector(FVector(B*HullExt.X*0.55f,0,0))+FVector(0,0,SinkZ),NowReal);
    }
    Ship->Hull->SetVisibility(false);
    TInlineComponentArray<UOVWeaponVisualComponent*> Visuals(Ship);
    for(UOVWeaponVisualComponent* V:Visuals) if(IsValid(V)) V->SetVisibility(false,true);
   }
  }
  for(int32 BI=DT.ExtraBursts.Num()-1;BI>=0;--BI) {
   const double Age=NowReal-DT.ExtraBursts[BI].Value;
   if(Age<0 || Age>0.38) { if(Age>0.38) DT.ExtraBursts.RemoveAt(BI); continue; }
   const float Alpha=FMath::Clamp(1.f-float(Age/0.38),0.15f,1.f);
   const float BurstRad=(28.f+(1.f-Alpha)*54.f)*FX;
   HullHits.Emplace(FQuat::Identity,DT.ExtraBursts[BI].Key,FVector(BurstRad/50.f));
  }
 }
 // 4. 3D Space Skybox, Distant Planet, & Multi-Layer Parallax Starfield
 const FVector SkyCenter=RangeCamera?RangeCamera->GetActorLocation():CameraFocus;
 const float StarDistScale=CalibrationScenario>=0?12.f:1.f;
 if(IsValid(SpaceSkyboxMesh)) {
  SpaceSkyboxMesh->SetWorldLocation(SkyCenter);
  SpaceSkyboxMesh->SetWorldScale3D(FVector(CalibrationScenario>=0?18000.f:1400.f));
 }
 if(IsValid(DistantPlanetMesh)) {
  const uint32 SysHash=GetTypeHash(CurrentStarSystemId);
  const float AngleDeg=float(SysHash%360);
  const FVector PlanetDir=FRotator(-14.f,AngleDeg,0.f).Vector();
  const FVector PlanetAnchor=FMath::Lerp(CameraFocus,SkyCenter,0.82f);
  DistantPlanetMesh->SetWorldLocationAndRotation(PlanetAnchor+PlanetDir*(27500.f*StarDistScale),FRotator(16.f,float(NowReal*1.8),12.f));
  DistantPlanetMesh->SetWorldScale3D(FVector((72.f+float(SysHash%35))*StarDistScale));
 }
 if(IsValid(StarfieldMeshes)) {
  TArray<FTransform> Stars;
  Stars.Reserve(CachedStarTransforms.Num());
  for(int32 SI=0;SI<CachedStarTransforms.Num();++SI) {
   const FTransform& ST=CachedStarTransforms[SI];
   const float ParallaxAlpha=SI<120?0.45f:(SI<280?0.75f:0.94f);
   const FVector Anchor=FMath::Lerp(CameraFocus,SkyCenter,ParallaxAlpha);
   Stars.Emplace(ST.GetRotation(),Anchor+ST.GetLocation()*StarDistScale,ST.GetScale3D()*StarDistScale);
  }
  SyncInstances(StarfieldMeshes,Stars);
 }
 // 5. 3D Space Environments (운석지대, 우주 방사능 지대, 태양 플레어, 소규모 블랙홀)
 TArray<FTransform> AsteroidXfs,RadiationXfs,SolarFlareXfs,BlackHoleXfs;
 for(int32 AI=0;AI<ActiveAsteroids.Num();++AI) {
  const FVector4& A=ActiveAsteroids[AI];
  const float Spin=float(SimulationSeconds*(8.0+(AI%5)*3.0)+AI*47.0);
  const FRotator AstRot(Spin*0.6f,Spin,Spin*0.35f);
  const float BaseScale=A.W/50.f;
  AsteroidXfs.Emplace(AstRot.Quaternion(),FVector(A.X,A.Y,A.Z),FVector(BaseScale*1.12f,BaseScale*0.95f,BaseScale*0.88f));
 }
 for(int32 RI=0;RI<ActiveRadiationZones.Num();++RI) {
  const FVector4& RZ=ActiveRadiationZones[RI];
  const float Pulse=1.f+0.04f*FMath::Sin(float(NowReal*2.2+RI));
  const float RScale=(RZ.W/50.f)*Pulse;
  const FRotator RRot(0.f,float(NowReal*12.0*(RI%2==0?1:-1)),0.f);
  RadiationXfs.Emplace(RRot.Quaternion(),FVector(RZ.X,RZ.Y,RZ.Z),FVector(RScale,RScale,RScale*0.42f));
 }
 if(ActiveEnvironment==EOVSpaceEnvironmentType::SolarFlare) {
  const float Scale=CalibrationScenario>=0?20.f:1.f;
  if(SolarFlareCycleTime>=9.f && SolarFlareCycleTime<=16.f) {
   const float WaveAlpha=SolarFlareCycleTime<13.f?0.f:FMath::Clamp((SolarFlareCycleTime-13.f)/3.f,0.f,1.f);
   const float WaveX=SolarFlareCycleTime<13.f?(2800.f*Scale):FMath::Lerp(2800.f*Scale,-2800.f*Scale,WaveAlpha);
   const FRotator WaveRot(0.f,float(NowReal*25.0),0.f);
   SolarFlareXfs.Emplace(WaveRot.Quaternion(),FVector(WaveX,0.f,300.f*(CalibrationScenario>=0?6.f:1.f)),FVector(6.5f*Scale,48.f*Scale,14.f*Scale));
  }
 }
 if(ActiveEnvironment==EOVSpaceEnvironmentType::BlackHole) {
  const float Scale=CalibrationScenario>=0?20.f:1.f;
  const FRotator BHRot(14.f,float(NowReal*38.0),8.f);
  BlackHoleXfs.Emplace(BHRot.Quaternion(),ActiveBlackHoleCenter,FVector(11.5f*Scale));
 }
 SyncInstances(AsteroidMeshes,AsteroidXfs);
 SyncInstances(RadiationZoneMeshes,RadiationXfs);
 SyncInstances(SolarFlareMeshes,SolarFlareXfs);
 SyncInstances(BlackHoleMeshes,BlackHoleXfs);
 SyncInstances(StandardMissileMeshes,StdMissiles);
 SyncInstances(AntiShipMissileMeshes,AntiShipMissiles);
 SyncInstances(CannonProjectileMeshes,CannonBolts);
 SyncInstances(FriendlyBeamMeshes,FriendlyBeams);
 SyncInstances(EnemyBeamMeshes,EnemyBeams);
 SyncInstances(MissilePlumeMeshes,Plumes);
 SyncInstances(ShieldImpactMeshes,ShieldHits);
 SyncInstances(HullImpactMeshes,HullHits);
}
void AOVController::StartCombatRange()
{
 if(bPlayScenarioSession) ResetPlayScenarioSession(true);
 if(!Combat || Ships.Num()<3) return;
 ResetDeathThroes();
 SetSpaceEnvironment(EOVSpaceEnvironmentType::None);
 ResetPhase2C();
 Action(TEXT("Cancel")); OrbitGesture={}; Groups.Reset();
 Combat->DisableCombat();
 const bool bRestoreLegacy = CalibrationScenario >= 0;
 CalibrationScenario = -1;
 HitResultTraceDistance = LegacyTraceDistance;
 const FVector Positions[]={FVector(-1400,-700,300),FVector(-1600,800,300),FVector(1600,0,300)};
 for(int32 I=0;I<Ships.Num();++I) {
  AOVShip* Ship=Ships[I];
  if (bRestoreLegacy)
  {
   const TCHAR* Names[] = {TEXT("01 / KESTREL - Escort"), TEXT("02 / ATLAS - Cruiser"), TEXT("03 / RED SIGNAL - Target")};
   const float Speeds[] = {850,420,0}, Turns[] = {90,24,0}, Scales[] = {1.4f,2.5f,1.8f};
   const FLinearColor Colors[] = {{.1f,.8f,1},{.3f,.5f,1},{1,.12f,.08f}};
   Ship->Configure(Names[I],I!=2,Speeds[I],Turns[I],FVector(Scales[I]),Colors[I]);
   Ship->SetCalibrationHull(nullptr);
   Ship->Systems->Tuning=GetDefault<UOVShipSystemsComponent>()->Tuning;
  }
  // Transient per-run fixture. Authored 2A Content assets are never modified.
  UShipLayoutDataAsset* Layout=UShipLayoutDataAsset::MakeTestLayout(this,I+1);
  if(I==0) { Layout->WeaponMounts.RemoveAt(2); Layout->MissileSlots=0; }
  if(I==1) {
   FOVWeaponMountDefinition Guided=Layout->WeaponMounts[2];
   Guided.WeaponId=TEXT("Weapon_3"); Guided.LocalPosition=FVector(0,0,-40);
   Layout->WeaponMounts.Add(Guided); ++Layout->MissileSlots;
  }
  Layout->WeaponTier=I==0?1:2;
  for(auto& Mount:Layout->WeaponMounts) Mount.Tier=Layout->WeaponTier;
  Ship->Systems->FactionId=I==2?1:0;
  Ship->Systems->Initialize(Layout);
  Ship->DebugResetMotion();
  Ship->bFriendly=I!=2;
  Ship->SetActorHiddenInGame(false);
  Ship->SetActorEnableCollision(true);
  Ship->SetActorLocationAndRotation(Positions[I],FRotator(0,I==2?180:0,0));
  Ship->SetAutomation(I==2,I==2);
 }
 TArray<AOVShip*> Combatants; for(AOVShip* Ship:Ships) Combatants.Add(Ship);
 Combat->EnableCombat(Combatants,260112);
 Combat->RegisterShip(Ships[1],2,true);
 Combat->SetWeaponProfile(Ships[1],TEXT("Weapon_3"),TEXT("GuidedMissile_T2"));
 bSimulationPaused=true; SimulationSpeed=1; SimulationSeconds=0;
 SelectShip(Ships[1]); SelectedWeapon=1;
 // Frame the wider combat fixture; profile gains and all subsequent controls stay unchanged.
 CameraFocus=FVector(0,0,300); CameraDistance=6500; UpdateCamera();
 LastAction=TEXT("전투 준비 · T 표적 지정 → B 선택 무장 / L 전체 발사 · F1 진행");
 UE_LOG(LogTemp,Display,TEXT("OV2B: Combat range reset, seed=260112, paused"));
}
bool AOVController::StartCalibrationRange(int32 Scenario)
{
 if (!Combat || Scenario<0 || Scenario>2) return false;
 if(bPlayScenarioSession) ResetPlayScenarioSession(true);
 ResetDeathThroes();
 SetSpaceEnvironment(EOVSpaceEnvironmentType::None);
 ResetPhase2C();
 TArray<AOVShip*> Combatants;
 for (AOVShip* Ship : Ships) Combatants.Add(Ship);
 if (!OVCalibration::Start(*Combat, Combatants, Scenario)) return false;

 Action(TEXT("Cancel"));
 OrbitGesture={};
 Groups.Reset();
 CalibrationScenario=Scenario;
 HitResultTraceDistance=OVCalibration::RayLength;
 bSimulationPaused=true;
 SimulationSpeed=1;
 SimulationSeconds=0;
 SelectShip(Ships[0]);
 CameraFocus=FVector(0,0,2000);
 CameraDistance=OVCalibration::StartZoom; CameraYaw=-90; CameraPitch=-55;
 UpdateCamera();
 LastAction=TEXT("교정 전투 · F6 장면 변경 · F5 기본 전투 · T 표적 → B 발사 · F1 진행");
 return true;
}

double AOVController::CommandXY() const { return CalibrationScenario>=0 ? OVCalibration::XYLimit : 6000; }
double AOVController::CommandMinZ() const { return CalibrationScenario>=0 ? OVCalibration::MinZ : -1500; }
double AOVController::CommandMaxZ() const { return CalibrationScenario>=0 ? OVCalibration::MaxZ : 4000; }
double AOVController::CommandRayLength() const { return CalibrationScenario>=0 ? OVCalibration::RayLength : 100000; }
double AOVController::AltitudeScale() const { return CalibrationScenario>=0 ? OVCalibration::AltitudeScale : 1; }

void AOVController::SelectShip(AOVShip* Ship)
{
 CancelRightMove();
 if(Combat && Combat->bEnabled && Combat->IsDeparted(Ship)) Ship=nullptr;
 if(!Ship) { ClearSelection(); return; }
 for(AOVShip* Other:Ships) Other->bSelected=false;
 Selection.Reset(); Selection.Add(Ship);
 Selected=Ship; Selected->bSelected=true; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false;
 ViewedDeck=0; SelectedSlot=0; SelectedArmor=0; SelectedWeapon=0;
 LastAction=TEXT("선택: ")+Ship->ShipName;
 UE_LOG(LogTemp,Display,TEXT("OV: Select %s"),*Ship->ShipName);
}
void AOVController::ClearSelection()
{
 CancelRightMove();
 for(AOVShip* Ship:Ships) Ship->bSelected=false;
 Selection.Reset(); Selected=nullptr; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false;
}
void AOVController::SelectShipFromUser(AOVShip* Ship, bool bAdditive)
{
 if(bAdditive && Ship) SelectAdditional(Ship); else SelectShip(Ship);
 if(auto* HUD=Cast<AOVHUD>(GetHUD())) {
  if(Selected) HUD->OpenDetails(); else HUD->SetDetailsOpen(false);
 }
}
void AOVController::SelectAdditional(AOVShip* Ship)
{
 if(!Ship) return;
 if(Combat && Combat->bEnabled && Combat->IsDeparted(Ship)) return;
 CancelRightMove();
 if(!Selected) { SelectShip(Ship); return; }
 if(Selection.Contains(Ship)) { Selection.Remove(Ship); Ship->bSelected=false; if(Selected==Ship) { Selected=Selection.Num()?Selection[0]:nullptr; ViewedDeck=0; SelectedSlot=0; SelectedArmor=0; SelectedWeapon=0; bPlacing=false; InputMode=EOVInputMode::None; bFollow=false; } }
 else { Selection.Add(Ship); Ship->bSelected=true; }
}
void AOVController::SetTarget(AOVShip* Ship)
{
 if(Combat && Combat->bEnabled && Combat->IsDeparted(Ship)) return;
 if(Selected && Ship && Ship!=Selected && Ship->bFriendly!=Selected->bFriendly && !Ship->Systems->bWrecked) {
  for(AOVShip* Vessel:Selection) if(Vessel->bFriendly && Vessel->Systems->CanCommand()) {
   if(Combat && Combat->bEnabled) Combat->SetTarget(Vessel,Ship); else Vessel->AttackTarget=Ship;
  }
  LastAction=TEXT("표적 지정: ")+Ship->ShipName+(Combat&&Combat->bEnabled?TEXT(" / B 선택 무장, L 전체 무장 발사"):TEXT(" (weapons not active in 2A)"));
 }
 InputMode=EOVInputMode::None;
}
bool AOVController::ActionPressed(FName Name) const
{
 TArray<FInputActionKeyMapping> Mappings; GetDefault<UInputSettings>()->GetActionMappingByName(Name,Mappings);
 for(const auto& Mapping:Mappings) if(WasInputKeyJustPressed(Mapping.Key)
  && (!Mapping.bShift || IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift))
  && (!Mapping.bCtrl || IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl))
  && (!Mapping.bAlt || IsInputKeyDown(EKeys::LeftAlt)||IsInputKeyDown(EKeys::RightAlt))) return true;
 return false;
}
void AOVController::Action(FName Name)
{
 if(auto* Play=GetWorld()->GetSubsystem<UOVPlayScenarioSubsystem>();Play&&Play->HandleAction(*this,Name)) return;
 if(Name==TEXT("Env_None")) { SetSpaceEnvironment(EOVSpaceEnvironmentType::None); return; }
 if(Name==TEXT("Env_AsteroidField")) { SetSpaceEnvironment(EOVSpaceEnvironmentType::AsteroidField); return; }
 if(Name==TEXT("Env_RadiationZone")) { SetSpaceEnvironment(EOVSpaceEnvironmentType::RadiationZone); return; }
 if(Name==TEXT("Env_SolarFlare")) { SetSpaceEnvironment(EOVSpaceEnvironmentType::SolarFlare); return; }
 if(Name==TEXT("Env_BlackHole")) { SetSpaceEnvironment(EOVSpaceEnvironmentType::BlackHole); return; }
 if(Name.ToString().StartsWith(TEXT("StarSystem_Enter_"))) { SwitchStarSystem(FName(*Name.ToString().Mid(17))); return; }
 if(Name.ToString().StartsWith(TEXT("StarSystem_FTL_"))) { DispatchFleetFTL(FName(*Name.ToString().Mid(15))); return; }
 if(Name.ToString().StartsWith(TEXT("StarSystem_BuildBasic_"))) { BuildOutpost(FName(*Name.ToString().Mid(22)),EOVOutpostKind::Basic); return; }
 if(Name.ToString().StartsWith(TEXT("StarSystem_BuildMine_"))) { BuildOutpost(FName(*Name.ToString().Mid(21)),EOVOutpostKind::MiningHub); return; }
 if(Name.ToString().StartsWith(TEXT("StarSystem_BuildCombat_"))) { BuildOutpost(FName(*Name.ToString().Mid(23)),EOVOutpostKind::CombatBase); return; }
 if(Mission && Mission->HandleAction(Name)) return;
 if(Phase2CAction(Name)) return;
 if(Name==TEXT("Phase2C")) { StartPhase2CRange((Phase2CScenario+1)%6); return; }
 if(Name==TEXT("RestartScene")) { if(Phase2CScenario>=0) StartPhase2CRange(Phase2CScenario); else if(CalibrationScenario>=0) StartCalibrationRange(CalibrationScenario); else StartCombatRange(); return; }
 if(Name==TEXT("Overview")) { FrameEngagement(); return; }
 if(Name.ToString().StartsWith(TEXT("Pick_")) || Name.ToString().StartsWith(TEXT("WorldPick_"))) {
  const int32 Index=FCString::Atoi(*Name.ToString().Mid(Name.ToString().StartsWith(TEXT("WorldPick_"))?10:5));
  if(Ships.IsValidIndex(Index)) { if(InputMode==EOVInputMode::Target) SetTarget(Ships[Index]); else SelectShipFromUser(Ships[Index],false); } return;
 }
 if(Name==TEXT("CalibrationRange")) { StartCalibrationRange((CalibrationScenario+1)%3); return; }
 if(Name==TEXT("CombatRange")) { StartCombatRange(); return; }
 if(Name==TEXT("CameraProfile"))
 {
  Action(TEXT("Cancel"));
  OrbitGesture={};
  bPlayGame3=!bPlayGame3;
  ResetCameraProfile();
  return;
 }
 if(Name==TEXT("Move") || Name==TEXT("Stop") || Name==TEXT("Auto") ||
    Name==TEXT("AutoFree") || Name==TEXT("MoveAuto") || Name==TEXT("Target") || Name==TEXT("Heading") || Name==TEXT("Cancel"))
 {
  CancelRightMove();
 }
 if(Name==TEXT("Pause")) { bSimulationPaused=!bSimulationPaused; LastAction=bSimulationPaused?TEXT("일시정지 · 카메라와 명령 입력 가능"):TEXT("전투 진행"); }
 if(Name==TEXT("Speed1")) SimulationSpeed=1;
 if(Name==TEXT("Speed2")) SimulationSpeed=2;
 if(Name==TEXT("Speed3")) SimulationSpeed=3;
 if(Name==TEXT("Ship1")) SelectShip(Ships[0]);
 if(Name==TEXT("Ship2")) SelectShip(Ships[1]);
 if(Name==TEXT("Ship3")) SelectShip(Ships[2]);
 if(Name==TEXT("Deselect")) ClearSelection();
 if(Name==TEXT("ToggleDebugOutlines")) { bShowDebugOutlines=!bShowDebugOutlines; LastAction=bShowDebugOutlines?TEXT("윤곽선 표시 켜짐"):TEXT("윤곽선 표시 꺼짐"); return; }
 if(Name==TEXT("Developer")) { FlushPressedKeys(); bDeveloperMode=!bDeveloperMode; ViewedDeck=0; if(auto* HUD=Cast<AOVHUD>(GetHUD()))HUD->SyncAddedUI(); }
 if(Selected && Selected->bFriendly && Selected->Systems->CanCommand() && !(Combat&&Combat->bEnabled&&Combat->IsDeparted(Selected))) {
  if(Name==TEXT("Manual")) { Selected->SetAutomation(false,false); LastAction=TEXT("이동·사격 수동"); }
  if(Name==TEXT("Auto")) { Selected->SetAutomation(true,true); bPlacing=false; InputMode=EOVInputMode::None; LastAction=TEXT("AUTO / Hold and manual attitude preserved")+(Combat&&Combat->bEnabled?FString(TEXT(" / automatic engagement")):FString(TEXT("; combat deferred"))); }
  if(Name==TEXT("MoveAuto") && Combat && Combat->bEnabled) {
   Selected->SetAutomation(!Selected->IsMovementAutomatic(),Selected->IsAttackAutomatic());
   bPlacing=false; InputMode=EOVInputMode::None;
   LastAction=TEXT("이동 자동화 변경 · 사격과 정지/수동 자세 유지");
  }
  if(Name==TEXT("FireAuto") && Combat && Combat->bEnabled) {
   Selected->SetAutomation(Selected->IsMovementAutomatic(),!Selected->IsAttackAutomatic());
   LastAction=TEXT("사격 자동화 변경 · 이동 명령 유지");
  }
  if(Name==TEXT("AutoFree") && Combat && Combat->bEnabled) {
   Selected->SetAutomation(true,true);
   if(!Selected->bHasDestination && !Selected->bFTLCharging && !Selected->bManualAttitude) Selected->bHold=false;
   bPlacing=false; InputMode=EOVInputMode::None;
   LastAction=TEXT("AUTO FREE / release Hold only; existing destination, manual attitude and FTL remain authoritative");
  }
  if(Combat && Combat->bEnabled) {
   const auto Order=Combat->GetShipState(Selected);
   if(!Order.ForcedMount.IsNone() && (Name==TEXT("Cancel") || Name==TEXT("Move") || Name==TEXT("Stop") || Name==TEXT("Heading") || Name==TEXT("Manual") || Name==TEXT("Auto") || Name==TEXT("AutoFree") || Name==TEXT("FireAuto") || Name==TEXT("MoveAuto")))
    Combat->SetAttackIntent(Selected,Order.Intent);
   if(Name==TEXT("HullIntent")) Combat->SetAttackIntent(Selected,EOVAttackIntent::DestroyHull);
   if(Name==TEXT("GunIntent")) Combat->SetAttackIntent(Selected,EOVAttackIntent::DisableWeapons);
   if(Name==TEXT("CancelAttack")) Combat->CancelAttackOrder(Selected);
   if(Name==TEXT("Protection")) Combat->SetProtectionPolicy(Selected,static_cast<EOVProtectionPolicy>((static_cast<int32>(Order.Protection)+1)%3));
   if(Name==TEXT("ForceGun") && Selected->Systems->WeaponMountStates.IsValidIndex(SelectedWeapon)) {
    const FName M=Selected->Systems->WeaponMountStates[SelectedWeapon].WeaponMountId;
    if(Combat->SetAttackIntent(Selected,EOVAttackIntent::DisableWeapons,M)) Combat->RequestFire(Selected,M);
    LastAction=TEXT("Forced gun attack requested: missed shots can hit hull / interior ally risk");
   }
   if(bDeveloperMode && Selected->AttackTarget) {
    auto* TargetSystems=Selected->AttackTarget->Systems.Get();const int32 F=Selected->Systems->FactionId;
    if(Name==TEXT("DevInterior")) TargetSystems->SetInteriorPresence(F,!TargetSystems->HasInteriorPresence(F),false,true);
    if(Name==TEXT("DevObjective")) TargetSystems->SetInteriorPresence(F,true,!TargetSystems->IsInteriorObjectiveComplete(F),true);
   }
  }
  if(Name==TEXT("NextWeapon") && Selected->Systems->WeaponMountStates.Num()>0) {
   const auto* L=Selected->Systems->Layout.Get();
   for(int32 I=0;I<Selected->Systems->WeaponMountStates.Num();++I) { SelectedWeapon=(SelectedWeapon+1)%Selected->Systems->WeaponMountStates.Num(); if(!L||!L->WeaponMounts.IsValidIndex(SelectedWeapon)||L->WeaponMounts[SelectedWeapon].Type!=EOVWeaponType::Interception)break; }
  }
  if((Name==TEXT("Fire") || Name==TEXT("FireAll")) && Combat && Combat->bEnabled) {
   const auto& Mounts=Selected->Systems->WeaponMountStates;
   const FName Mount=Name==TEXT("FireAll")?NAME_None:(Mounts.IsValidIndex(SelectedWeapon)?Mounts[SelectedWeapon].WeaponMountId:NAME_None);
   const bool Accepted=Combat->RequestFire(Selected,Mount);
   LastAction=Accepted?TEXT("발사 요청 접수 · 조준/사거리/시설 조건 충족 시 한 차례 발사"):TEXT("발사 요청 거부 · 표적/소유/무장 상태 확인");
  }
  if(Name==TEXT("Move")) { bPlacing=true; InputMode=EOVInputMode::Move; Preview=Selected->GetActorLocation(); PlacementPlaneZ=Preview.Z; OrderInput.Reset(); LastAction=(bPlayGame3?TEXT("평면 위치 지정 · Shift 드래그 고도 · 클릭/Enter 확정"):TEXT("평면 위치 지정 · Shift/우클릭 고도 · 클릭/Enter 확정")); }
  if(Name==TEXT("Up") && bPlacing) Preview.Z=FMath::Min(Preview.Z+250*AltitudeScale(),CommandMaxZ());
  if(Name==TEXT("Down") && bPlacing) Preview.Z=FMath::Max(Preview.Z-250*AltitudeScale(),CommandMinZ());
  if(Name==TEXT("Confirm")) ConfirmMove();
  if(Name==TEXT("Stop")) { for(AOVShip* Ship:Selection) { Transport->SuspendShuttle(Ship); Ship->StopMovement(); } bPlacing=false; InputMode=EOVInputMode::None; LastAction=TEXT("감속 정지 · 위치 유지"); }
  if(Name==TEXT("Target")) { bPlacing=false; InputMode=EOVInputMode::Target; LastAction=TEXT("적을 클릭해 표적 지정 · 발사는 별도 B/L 명령"); }
  if(Name==TEXT("ClearTarget")) { if(Combat&&Combat->bEnabled) Combat->SetTarget(Selected,nullptr); else Selected->AttackTarget=nullptr; }
  if(Name==TEXT("Heading")) { Transport->SuspendShuttle(Selected); bPlacing=false; InputMode=EOVInputMode::Heading; PlacementPlaneZ=Selected->GetActorLocation().Z; LastAction=TEXT("전장을 클릭해 방향 지정 · 현재 상하 기울기 유지"); }
  if(Name==TEXT("Port") || Name==TEXT("Starboard")) { FRotator Attitude=Selected->GetActorRotation(); Attitude.Yaw+=Name==TEXT("Port")?-90:90; Selected->SetManualAttitude(Attitude); }
  if(Name==TEXT("Repair")) LastAction=TEXT("보급 수리 미지원 · 시험 수리는 DEV에서만 가능");
 }
 if(Name==TEXT("Cancel")) { bPlacing=false; InputMode=EOVInputMode::None; bRightGesture=false; }
 if(Name==TEXT("Focus") && Selected) { bFollow=!bFollow && !Selected->Systems->bWrecked; if(bFollow || !bPlayGame3) CameraFocus=Selected->GetActorLocation(); }
 if(Name==TEXT("Details")) if(AOVHUD* HUD=Cast<AOVHUD>(GetHUD())) HUD->OpenDetails();
 if(Selected && Selected->Systems->Layout) {
  UOVShipSystemsComponent* S=Selected->Systems;
  const bool Reveal=Selected->bFriendly || bDeveloperMode;
  const FString ActionString=Name.ToString();
  if(Name==TEXT("External")) { ViewedDeck=0; if(auto* HUD=Cast<AOVHUD>(GetHUD())) HUD->OpenDetails(); }
  if(Reveal && ActionString.StartsWith(TEXT("Deck"))) ViewedDeck=FMath::Clamp(FCString::Atoi(*ActionString.Mid(4)),1,S->Layout->DeckCount);
  if(Reveal && ActionString.StartsWith(TEXT("Slot_"))) SelectedSlot=FMath::Clamp(FCString::Atoi(*ActionString.Mid(5)),0,S->Slots.Num()-1);
  if(ActionString.StartsWith(TEXT("Armor_"))) SelectedArmor=FMath::Clamp(FCString::Atoi(*ActionString.Mid(6)),0,S->ArmorSections.Num()-1);
  if(ActionString.StartsWith(TEXT("Weapon_"))) SelectedWeapon=FMath::Clamp(FCString::Atoi(*ActionString.Mid(7)),0,S->WeaponMountStates.Num()-1);
  if(bDeveloperMode && S->Slots.IsValidIndex(SelectedSlot)) {
   const FName Slot=S->Slots[SelectedSlot].SlotId;
   if(Name==TEXT("TestHit")) TestArmorHit();
   if(Name==TEXT("DestroyExt")) S->DebugDestroySlot(Slot,EOVDamageCause::External);
   if(Name==TEXT("Sabotage")) S->DebugDestroySlot(Slot,EOVDamageCause::Sabotage);
   if(Name==TEXT("RepairSlot")) S->DebugRepairSlot(Slot);
   if(Name==TEXT("CrewDown")) S->DebugSetCrew(Slot,S->Slots[SelectedSlot].Crew-2);
   if(Name==TEXT("CrewUp")) S->DebugSetCrew(Slot,S->Slots[SelectedSlot].Crew+2);
   if(Name==TEXT("Occupy")) S->DebugSetOccupation(Slot,S->Slots[SelectedSlot].OccupyingFaction<0?1-S->FactionId:-1);
   if(Name==TEXT("Reset")) { S->ResetState(); Selected->DebugResetMotion(); }
   if(Name==TEXT("ArmorZero") && S->ArmorSections.IsValidIndex(SelectedArmor)) S->DebugSetArmor(S->ArmorSections[SelectedArmor].ArmorSectionId,0);
   if(Name==TEXT("RepairArmor") && S->ArmorSections.IsValidIndex(SelectedArmor)) S->DebugSetArmor(S->ArmorSections[SelectedArmor].ArmorSectionId,S->Layout->ArmorSections[SelectedArmor].MaxArmor);
   if(Name==TEXT("WeaponHit") && S->WeaponMountStates.IsValidIndex(SelectedWeapon)) S->DebugHitWeapon(S->WeaponMountStates[SelectedWeapon].WeaponMountId,TestDamage);
   if(Name==TEXT("RepairWeapon") && S->WeaponMountStates.IsValidIndex(SelectedWeapon)) S->DebugRepairWeapon(S->WeaponMountStates[SelectedWeapon].WeaponMountId);
  }
 }
 UE_LOG(LogTemp,Display,TEXT("OV: Action=%s paused=%d speed=%.0f"),*Name.ToString(),bSimulationPaused,SimulationSpeed);
}
void AOVController::ConfirmMove()
{
 if(!bPlacing || !Selected || !Selected->bFriendly || !Selected->Systems->CanCommand()) return;
 for(AOVShip* Ship:Selection) if(Ship->bFriendly && Ship->Systems->CanCommand()) { Transport->SuspendShuttle(Ship); Ship->SetAutomation(false,Ship->IsAttackAutomatic()); Ship->SetDestination(Preview+(Ship->GetActorLocation()-Selected->GetActorLocation())); }
 bPlacing=false; InputMode=EOVInputMode::None;
 LastAction=FString::Printf(TEXT("이동 명령 수락 · X %.0fm  Y %.0fm  고도 %.0fm"),Preview.X/100,Preview.Y/100,Preview.Z/100);
 UE_LOG(LogTemp,Display,TEXT("OV: %s"),*LastAction);
}
void AOVController::UpdatePreview()
{
 FVector Origin=FVector::ZeroVector,Direction=FVector::ZeroVector; float X=0,Y=0;
 int32 Width=0,Height=0;
 GetViewportSize(Width,Height);
 const bool Inside=GetMousePosition(X,Y)&&X>=0&&Y>=0&&X<Width&&Y<Height;
 const AOVHUD* HUD=Cast<AOVHUD>(GetHUD());
 const auto* Viewport=GetWorld()->GetGameViewport();
 const bool Blocked=!Inside || !Viewport || !Viewport->Viewport || !Viewport->Viewport->HasFocus() || !DeprojectMousePositionToWorld(Origin,Direction) || (HUD && HUD->IsOverPanel(X,Y));
 SampleOrderCursor(Origin,Direction,Y,IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift)||(!bPlayGame3&&IsInputKeyDown(EKeys::RightMouseButton)),Blocked);
}
void AOVController::SampleOrderCursor(FVector Origin,FVector Direction,float MouseY,bool Altitude,bool Blocked)
{
 FVector Raw=Preview; const bool Valid=!Blocked&&FOVOrderInput::Project(Origin,Direction,PlacementPlaneZ,Raw,ProjectionNearDistance,CommandRayLength());
 OrderInput.Sample(Preview,Raw,MouseY,Altitude,Blocked||!Valid,static_cast<float>(AltitudeUnitsPerPixel*AltitudeScale()),CommandXY(),CommandMinZ(),CommandMaxZ());
}
void AOVController::IssueHeading(FVector Point)
{
 if(!Selected || !Selected->Systems->CanCommand()) return;
 FRotator Heading=Selected->GetActorRotation(); const FVector To=Point-Selected->GetActorLocation();
 if(To.SizeSquared2D()>1) { Heading.Yaw=To.Rotation().Yaw; Selected->SetManualAttitude(Heading); }
 InputMode=EOVInputMode::None;
}
void AOVController::TestArmorHit()
{
 if(!bDeveloperMode || !Selected || !Selected->Systems->Layout || !Selected->Systems->Layout->ArmorSections.IsValidIndex(SelectedArmor)) return;
 const auto& Armor=Selected->Systems->Layout->ArmorSections[SelectedArmor];
 FVector Normal=Armor.LocalCenter.GetSafeNormal();
 Selected->Systems->ApplyWorldHit(Selected->GetActorTransform().TransformPosition(Armor.LocalCenter),Selected->GetActorTransform().TransformVectorNoScale(Normal),TestDamage,EOVDamageCause::External);
}
void AOVController::ResetCameraProfile()
{
 bFollow=false;
 CameraFocus=FVector::ZeroVector;
 CameraYaw=bPlayGame3?32.f:-45.f;
 CameraPitch=bPlayGame3?-38.f:-48.f;
 CameraDistance=CalibrationScenario>=0 ? OVCalibration::StartZoom : (bPlayGame3?FOVPlayGame3Profile::InitialDistance:6500.f);
 UpdateCamera();
}

void AOVController::ApplyCameraInput(FVector2D OrbitDelta, FVector LocalPan, float YawAxis, int32 Wheel, float RealSeconds)
{
 const float Dt=FMath::Clamp(RealSeconds,0.f,.1f);
 CameraYaw=FRotator::NormalizeAxis(CameraYaw+OrbitDelta.X*(bPlayGame3?.32:.3)+YawAxis*75*Dt);
 CameraPitch=FMath::Clamp(CameraPitch-OrbitDelta.Y*(bPlayGame3?.28:.3),bPlayGame3?-84.:-85.,bPlayGame3?-12.:-10.);
 if(Wheel)
 {
  float MinDistance=1200;
  float MaxDistance=16000;
  double Multiplier=FMath::Pow(.86,Wheel);
  if(bPlayGame3)
  {
   MinDistance=35*FOVPlayGame3Profile::UnitsPerReferenceUnit;
   MaxDistance=FOVPlayGame3Profile::MaxDistance;
   if(Selected && Selected->Systems->Layout && !Selected->Systems->bWrecked)
   {
    const FVector HullExtent=Selected->Systems->Layout->LocalHalfExtent*Selected->GetActorScale3D();
    MinDistance=FMath::Clamp(1.45*HullExtent.Size(),double(MinDistance),double(MaxDistance));
   }
   Multiplier=Wheel>0?FMath::Pow(.88,Wheel):FMath::Pow(1.13,-Wheel);
  }
  if (CalibrationScenario>=0)
  {
   MaxDistance=OVCalibration::MaxZoom;
   MinDistance=3000;
   if (Selected && Selected->Systems->Layout)
    MinDistance=FMath::Max(MinDistance, float(1.45*(Selected->Systems->Layout->LocalHalfExtent*Selected->GetActorScale3D()).Size()));
  }
  CameraDistance=FMath::Clamp(CameraDistance*Multiplier,double(MinDistance),double(MaxDistance));
 }
 if(!LocalPan.IsNearlyZero())
 {
  bFollow=false;
  const double Speed=CalibrationScenario>=0 ? (bPlayGame3?FOVPlayGame3Profile::PanSpeed(CameraDistance/20)*20:CameraDistance*.45)
   : (bPlayGame3?FOVPlayGame3Profile::PanSpeed(CameraDistance):CameraDistance*.45);
  CameraFocus+=FRotator(0,CameraYaw,0).RotateVector(LocalPan.GetClampedToMaxSize(1))*Speed*Dt;
 }
 if(bFollow)
 {
  if(!IsValid(Selected) || Selected->Systems->bWrecked) bFollow=false;
  else CameraFocus=FMath::Lerp(CameraFocus,Selected->GetActorLocation(),bPlayGame3?1.0-FMath::Exp(-10.0*Dt):1.0);
 }
 UpdateCamera();
}

void AOVController::UpdateCamera()
{
 if(const auto* OVHUD=Cast<AOVHUD>(GetHUD()))
 {
  if(OVHUD->IsTitleScreenOpen()) return;
 }
 if(!RangeCamera) return;
 auto* Camera=RangeCamera->GetCameraComponent();
 Camera->bOverrideAspectRatioAxisConstraint=bPlayGame3;
 Camera->AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;
 int32 Width=0, Height=0;
 GetViewportSize(Width,Height);
 const double Aspect=Width>0&&Height>0?double(Width)/Height:16.0/9.0;
 Camera->SetFieldOfView(bPlayGame3?FOVPlayGame3Profile::HorizontalFov(Aspect):55.f);
 const FRotator Rotation(CameraPitch,CameraYaw,0);
 RangeCamera->SetActorLocationAndRotation(CameraFocus-Rotation.Vector()*CameraDistance,Rotation);
}

void AOVController::CancelRightMove()
{
 RightMove={};
 RightCommander.Reset();
 RightTarget.Reset();
 bRightHeading=false;
}

void AOVController::FlushPressedKeys()
{
 Super::FlushPressedKeys();
 OrbitGesture={};
 bConsumeAltLeft=false;
 bRightGesture=false;
 CancelRightMove();
 OrderInput.bBlocked=true;
 if(auto* HUD=Cast<AOVHUD>(GetHUD())) HUD->CancelPointerDrag();
}

void AOVController::UpdateOrbit(FVector2D Cursor)
{
 if(OrbitGesture.Surface==EOVOrbitSurface::None) return;
 FVector2D Delta=OrbitGesture.Sample(Cursor);
 if(!bPlayGame3)
 {
  float DX=0,DY=0;
  GetInputMouseDelta(DX,DY);
  Delta=FVector2D(DX,DY);
 }
 if(OrbitGesture.Surface==EOVOrbitSurface::World)
 {
  ApplyCameraInput(Delta,FVector::ZeroVector,0,0,0);
 }
 else if(auto* HUD=Cast<AOVHUD>(GetHUD()))
 {
  if(HUD->HasOrbitPreview()) HUD->OrbitPreview(Delta);
  else OrbitGesture={};
 }
}

void AOVController::UpdateRightMove(FVector2D Cursor)
{
 Preview=RightMove.Sample(Cursor,AltitudeScale(),CommandMinZ(),CommandMaxZ());
 OrderInput.bShowAltitude=!bRightHeading && RightMove.MaxTravelSquared>=FMath::Square(8.0);
 if(bRightHeading) Preview.Z=RightMove.Anchor.Z;
}

void AOVController::HandleRightMove(bool Pressed, FVector2D Cursor, bool InWorld, bool Shift, bool Ctrl)
{
 if(Pressed)
 {
  if(RightMove.bActive || !InWorld || bPlacing || InputMode!=EOVInputMode::None ||
     OrbitGesture.Surface!=EOVOrbitSurface::None || !Selected || !Selected->bFriendly ||
     !Selected->Systems->CanCommand()) return;

  RightCommander=Selected;
  bRightHeading=Ctrl;
  if(!Shift && !Ctrl)
  {
   FHitResult Hit;
   if(GetHitResultAtScreenPosition(Cursor,ECC_Visibility,false,Hit))
   {
    AOVShip* Ship=Cast<AOVShip>(Hit.GetActor());
    if(Ship && !Ship->bFriendly && !Ship->Systems->bWrecked) RightTarget=Ship;
   }
  }

  PlacementPlaneZ=Selected->GetActorLocation().Z;
  FVector Origin,Direction,Point=Selected->GetActorLocation();
  const bool Valid=DeprojectScreenPositionToWorld(Cursor.X,Cursor.Y,Origin,Direction) &&
   FOVOrderInput::Project(Origin,Direction,PlacementPlaneZ,Point,ProjectionNearDistance,CommandRayLength());
  Point.X=FMath::Clamp(Point.X,-CommandXY(),CommandXY());
  Point.Y=FMath::Clamp(Point.Y,-CommandXY(),CommandXY());
  RightMove.Begin(Cursor,Point,Valid);
  UpdateRightMove(Cursor);
  LastAction=TEXT("우클릭 XY 고정 · 드래그로 고도 · 해제로 명령 확정");
  return;
 }

 if(!RightMove.bActive) return;
 UpdateRightMove(Cursor);
 const bool Allowed=InWorld && Selected && RightCommander.Get()==Selected && Selected->Systems->CanCommand();
 if(RightMove.Finish(Allowed))
 {
  if(RightTarget.IsValid() && !RightTarget->Systems->bWrecked && RightMove.MaxTravelSquared<=FMath::Square(25.0))
   SetTarget(RightTarget.Get());
  else if(RightMove.bValidPoint)
  {
   if(bRightHeading) IssueHeading(Preview);
   else { bPlacing=true; ConfirmMove(); }
  }
 }
 CancelRightMove();
}

bool AOVController::InputKey(const FInputKeyEventArgs& Params)
{
 if(auto* UI=Cast<AOVHUD>(GetHUD()); UI&&UI->IsAddedUIEditing())
 {
  OrbitGesture={}; CancelRightMove();
  // Text entry is owned by Slate. Do not place typing into gameplay key state.
  return true;
 }
 const FKey Key=Params.Key;
 const bool Pressed=Params.Event==IE_Pressed || Params.Event==IE_DoubleClick;
 const bool Released=Params.Event==IE_Released;
 if(Released && (Key==EKeys::LeftAlt || Key==EKeys::RightAlt) && OrbitGesture.bAltLeft)
  OrbitGesture.End(true);

 if(Pressed || Released)
 {
  FIntPoint Pixel(-1,-1),Size(0,0);
  if(Params.Viewport)
  {
   Params.Viewport->GetMousePos(Pixel);
   Size=Params.Viewport->GetSizeXY();
  }
  const FVector2D Cursor(Pixel);
  const bool Inside=Pixel.X>=0 && Pixel.Y>=0 && Pixel.X<Size.X && Pixel.Y<Size.Y;
  auto* HUD=Cast<AOVHUD>(GetHUD());
  const bool OverUI=Inside && HUD && HUD->IsOverPanel(Cursor.X,Cursor.Y);
  const bool Alt=Params.Viewport && (Params.Viewport->KeyState(EKeys::LeftAlt)||Params.Viewport->KeyState(EKeys::RightAlt));
  const bool AltLeft=Key==EKeys::LeftMouseButton && (bConsumeAltLeft || (bPlayGame3 && Pressed && Alt));
  if(Key==EKeys::MiddleMouseButton || AltLeft)
  {
   if(AltLeft) bConsumeAltLeft=Pressed;
   if(Pressed && Inside && !RightMove.bActive)
   {
    if(!OverUI) OrbitGesture.Begin(EOVOrbitSurface::World,AltLeft,Cursor);
    else if(HUD->IsOverOrbitPreview(Cursor)) OrbitGesture.Begin(EOVOrbitSurface::Preview,AltLeft,Cursor);
   }
   if(Released && OrbitGesture.Surface!=EOVOrbitSurface::None && OrbitGesture.bAltLeft==AltLeft)
   {
    if(bPlayGame3) UpdateOrbit(Cursor);
    OrbitGesture.End(AltLeft);
   }
   return true;
  }
  if(bPlayGame3 && Key==EKeys::RightMouseButton)
  {
   const bool Shift=Params.Viewport && (Params.Viewport->KeyState(EKeys::LeftShift)||Params.Viewport->KeyState(EKeys::RightShift));
   const bool Ctrl=Params.Viewport && (Params.Viewport->KeyState(EKeys::LeftControl)||Params.Viewport->KeyState(EKeys::RightControl));
   HandleRightMove(Pressed,Cursor,Inside&&!OverUI&&(Pressed||Params.Viewport->HasFocus()),Shift,Ctrl);
   return true;
  }
  if((RightMove.bActive || OrbitGesture.Surface!=EOVOrbitSurface::None) && Key==EKeys::LeftMouseButton && Pressed) return true;
 }
 return Super::InputKey(Params);
}

void AOVController::StepSimulation(float DeltaTime)
{
 if(bSimulationPaused || DeltaTime<=0 || !FMath::IsFinite(DeltaTime)) return;
 float Remaining=DeltaTime*SimulationSpeed; SimulationSeconds+=Remaining;
 if(!bPlayScenarioSession) CampaignMiningAccum+=Remaining;
 while(CampaignMiningAccum>=4.0f) {
  CampaignMiningAccum-=4.0f;
  for(const FOVStarSystemNode& Node:StarSystems) {
   if(Node.OwnerFaction!=TEXT("player") || Node.Outpost==EOVOutpostKind::None) continue;
   const float Mult=Node.Outpost==EOVOutpostKind::MiningHub?1.45f:1.0f;
   ResourceCommon+=FMath::RoundToInt(Node.CommonYield*Mult);
   ResourceAdvanced+=FMath::RoundToInt(Node.AdvancedYield*Mult);
   ResourceSupreme+=FMath::RoundToInt(Node.SupremeYield*Mult);
  }
 }
 if(ActiveEnvironment==EOVSpaceEnvironmentType::RadiationZone) {
  EnvironmentTickAccum+=Remaining;
  while(EnvironmentTickAccum>=1.0f) {
   EnvironmentTickAccum-=1.0f;
   for(AOVShip* Ship:Ships) {
    if(!IsValid(Ship)||!Ship->Systems||Ship->Systems->bWrecked||Ship->Systems->bNeutral) continue;
    if(Combat&&Combat->bEnabled&&Combat->IsDeparted(Ship)) continue;
    for(const FVector4& RZ:ActiveRadiationZones) {
     if(FVector::Dist(Ship->GetActorLocation(),FVector(RZ.X,RZ.Y,RZ.Z))<=RZ.W) {
      Ship->Systems->ApplyWorldHit(Ship->GetActorLocation()+FVector(0,0,40),FVector::UpVector,18.f,EOVDamageCause::External,false);
      break;
     }
    }
   }
  }
 } else if(ActiveEnvironment==EOVSpaceEnvironmentType::SolarFlare) {
  const float PrevCycle=SolarFlareCycleTime;
  SolarFlareCycleTime=FMath::Fmod(SolarFlareCycleTime+Remaining,18.0f);
  if(SolarFlareCycleTime<PrevCycle) bSolarFlareWaveApplied=false;
  if(SolarFlareCycleTime>=14.0f && !bSolarFlareWaveApplied) {
   bSolarFlareWaveApplied=true;
   bool bAnyUnsheltered=false;
   for(AOVShip* Ship:Ships) {
    if(!IsValid(Ship)||!Ship->Systems||Ship->Systems->bWrecked||Ship->Systems->bNeutral) continue;
    if(Combat&&Combat->bEnabled&&Combat->IsDeparted(Ship)) continue;
    const bool bSheltered=IsShipShelteredFromSolarFlare(Ship);
    const float SurgeDamage=bSheltered?15.f:65.f;
    Ship->Systems->ApplyWorldHit(Ship->GetActorLocation()+FVector(100,0,0),FVector::ForwardVector,SurgeDamage,EOVDamageCause::External,false);
    if(!bSheltered) bAnyUnsheltered=true;
   }
   if(Combat && bAnyUnsheltered) Combat->EnvironmentRadarDisruptRemaining=3.5f;
   LastAction=bAnyUnsheltered?TEXT("태양 플레어 파동 통과 · 비엄폐 함선 쉴드 과부하 및 레이더 일시 교란"):TEXT("태양 플레어 파동 통과 · 운석 엄폐로 피해 완화");
  }
 }
 if(Combat && Combat->bEnabled) { Combat->Simulate(Remaining); if(Selected&&Combat->IsDeparted(Selected)) ClearSelection(); return; }
 while(Remaining>UE_SMALL_NUMBER) {
  const float Dt=FMath::Min(Remaining,1.f/60.f); Remaining-=Dt;
  for(AOVShip* Ship:Ships) {
   Ship->Systems->Simulate(Dt);
   if(Ship->Systems->bWrecked || Ship->Systems->bNeutral) { Ship->StopMovement(); Ship->Velocity=FVector::ZeroVector; Ship->AttackTarget=nullptr; continue; }
   if(Ship->AttackTarget&&Ship->AttackTarget->Systems->bWrecked) Ship->AttackTarget=nullptr;
   if(!Ship->Systems->CanCommand()) { Ship->SetAutomation(true,true); Ship->StopMovement(); }
   if(Ship->bFriendly) Ship->Simulate(Dt);
  }
  ResolveContacts(Dt);
 }
}
void AOVController::ResolveContacts(float SimDelta)
{
 const auto ContactRadius=[this](const AOVShip* Ship) {
  if (CalibrationScenario>=0 && Ship->Systems->Layout)
   return (Ship->Systems->Layout->LocalHalfExtent * Ship->GetActorScale3D()).Size();
  return 100*Ship->GetActorScale3D().GetMax();
 };
 // Each unordered pair is resolved exactly once per substep. No ramming damage in 2A.
 for(int32 I=0;I<Ships.Num();++I) for(int32 J=I+1;J<Ships.Num();++J) {
  AOVShip* A=Ships[I]; AOVShip* B=Ships[J];
  if(A->Systems->bWrecked||B->Systems->bWrecked) continue;
  if(Combat && Combat->bEnabled && (Combat->IsDeparted(A)||Combat->IsDeparted(B))) continue;
  const double RadiusA=ContactRadius(A), RadiusB=ContactRadius(B);
  FVector Delta=B->GetActorLocation()-A->GetActorLocation(); const double Distance=Delta.Size();
  const FVector Normal=Distance>0.001?Delta/Distance:FVector::RightVector;
  const bool MoveA=A->Systems->CanMove()&&!A->bFTLCharging;
  const bool MoveB=B->Systems->CanMove()&&!B->bFTLCharging;
  if(!MoveA&&!MoveB) continue;
  if(Distance<RadiusA+RadiusB+80) {
   // Soft avoidance steers velocity before hard overlap correction.
   const float Push=40*SimDelta;
   if(MoveA&&!A->bHold) A->Velocity-=Normal*Push;
   if(MoveB&&!B->bHold) B->Velocity+=Normal*Push;
  }
  if(Distance<RadiusA+RadiusB) {
   const FVector Correction=Normal*(RadiusA+RadiusB-Distance+0.1);
   if(MoveA) A->AddActorWorldOffset(-Correction*(MoveB?0.5:1));
   if(MoveB) B->AddActorWorldOffset(Correction*(MoveA?0.5:1));
   ++PairResolutionCount;
  }
 }
 if(!ActiveAsteroids.IsEmpty()) {
  for(AOVShip* Ship:Ships) {
   if(!IsValid(Ship)||!Ship->Systems||Ship->Systems->bWrecked) continue;
   if(Combat&&Combat->bEnabled&&Combat->IsDeparted(Ship)) continue;
   if(!Ship->Systems->CanMove()||Ship->bFTLCharging) continue;
   const double ShipRad=ContactRadius(Ship);
   for(const FVector4& Ast:ActiveAsteroids) {
    const FVector AstCenter(Ast.X,Ast.Y,Ast.Z);
    const double AstRad=Ast.W;
    const FVector Delta=Ship->GetActorLocation()-AstCenter;
    const double Distance=Delta.Size();
    const FVector Normal=Distance>0.001?Delta/Distance:FVector::RightVector;
    if(Distance<ShipRad+AstRad+100.0 && !Ship->bHold) Ship->Velocity+=Normal*(65.f*SimDelta);
    if(Distance<ShipRad+AstRad) Ship->AddActorWorldOffset(Normal*(ShipRad+AstRad-Distance+0.1));
   }
  }
 }
 if(ActiveEnvironment==EOVSpaceEnvironmentType::BlackHole) {
  for(AOVShip* Ship:Ships) {
   if(!IsValid(Ship)||!Ship->Systems||Ship->Systems->bWrecked) continue;
   if(Combat&&Combat->bEnabled&&Combat->IsDeparted(Ship)) continue;
   if(!Ship->Systems->CanMove()||Ship->bHold||Ship->bFTLCharging) continue;
   const FVector ToCenter=ActiveBlackHoleCenter-Ship->GetActorLocation();
   const double Dist=ToCenter.Size();
   if(Dist<ActiveBlackHoleRadius && Dist>80.0) {
    const FVector Dir=ToCenter/Dist;
    const float GravityFactor=FMath::Clamp(1.f-float(Dist/ActiveBlackHoleRadius),0.f,1.f);
    Ship->Velocity+=Dir*(120.f*GravityFactor*SimDelta);
    if(FVector::DotProduct(Ship->Velocity,Dir)<0.f)
     Ship->Velocity*=FMath::Max(0.85f,1.f-0.25f*GravityFactor*SimDelta);
   }
  }
 }
}
void AOVController::PlayerTick(float DeltaTime)
{
 Super::PlayerTick(DeltaTime);
 if(!RangeCamera) return;
 const auto* Client=GetWorld()->GetGameViewport();
 FViewport* Viewport=Client?Client->Viewport:nullptr;
 const bool Focused=Viewport&&Viewport->HasFocus();
 FIntPoint Pixel(-1,-1),Size(0,0);
 if(Viewport) { Viewport->GetMousePos(Pixel); Size=Viewport->GetSizeXY(); }
 const FVector2D Cursor(Pixel);
 const float MX=Cursor.X,MY=Cursor.Y;
 const bool Inside=Focused&&Pixel.X>=0&&Pixel.Y>=0&&Pixel.X<Size.X&&Pixel.Y<Size.Y;
 AOVHUD* HUD=Cast<AOVHUD>(GetHUD());
 if(HUD&&HUD->IsAddedUIEditing())
 {
  OrbitGesture={};CancelRightMove();HUD->CancelPointerDrag();
  StepSimulation(DeltaTime);UpdateCamera();UpdateCombatPresentation();return;
 }
 const bool LeftPressed=Inside&&WasInputKeyJustPressed(EKeys::LeftMouseButton);
 const int Wheel=Focused?int(WasInputKeyJustPressed(EKeys::MouseScrollUp))-int(WasInputKeyJustPressed(EKeys::MouseScrollDown)):0;
 const bool WorldGesture=RightMove.bActive||OrbitGesture.Surface==EOVOrbitSurface::World;
 const bool OverUI=HUD&&Inside&&(HUD->HandlePointer(MX,MY,LeftPressed,IsInputKeyDown(EKeys::LeftMouseButton),WorldGesture?0:Wheel,false,0,0) || (LeftPressed && !WorldGesture && HUD->IsOverWorldMarker(Cursor)));
 const bool InWorld=Inside&&!OverUI;
 if(!Focused)
 {
  OrbitGesture={};
  CancelRightMove();
  if(HUD) HUD->CancelPointerDrag();
 }
 else
 {
  UpdateOrbit(Cursor);
  if(RightMove.bActive) UpdateRightMove(Cursor);
 }

 const bool Shift=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift);
 const bool Ctrl=IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl);
 FVector Pan=FVector::ZeroVector;
 float YawAxis=0;
 if(InWorld)
 {
  Pan.X=int(IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::Up))-int(IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::Down));
  Pan.Y=int(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right))-int(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left));
  const int QE=int(IsInputKeyDown(EKeys::E))-int(IsInputKeyDown(EKeys::Q));
  if(bPlayGame3)
  {
   YawAxis=QE;
   Pan.Z=int(IsInputKeyDown(EKeys::PageUp))-int(IsInputKeyDown(EKeys::PageDown));
  }
  else
  {
   Pan.Z=QE;
   if(WasInputKeyJustPressed(EKeys::Z)) CameraYaw-=15;
   if(WasInputKeyJustPressed(EKeys::C)) CameraYaw+=15;
   if(WasInputKeyJustPressed(EKeys::PageUp)) CameraPitch-=10;
   if(WasInputKeyJustPressed(EKeys::G)) CameraPitch+=10;
  }
  if(WasInputKeyJustPressed(EKeys::Home)) FrameEngagement();
 }
 int CameraWheel=InWorld||WorldGesture?Wheel:0;
 if(OrbitGesture.Surface==EOVOrbitSurface::Preview) CameraWheel=0;
 if(!bPlayGame3 && bPlacing && CameraWheel)
 {
  Preview.Z=FMath::Clamp(Preview.Z+CameraWheel*250*AltitudeScale(),CommandMinZ(),CommandMaxZ());
  OrderInput.bShowAltitude=true;
  OrderInput.bReanchor=true;
  CameraWheel=0;
 }
 ApplyCameraInput(FVector2D::ZeroVector,Pan,YawAxis,CameraWheel,FApp::GetDeltaTime());
 if(ActionPressed(TEXT("OVCameraProfile"))) Action(TEXT("CameraProfile"));
 if(Focused && ActionPressed(TEXT("OVPhase2C"))) Action(bDeveloperMode?TEXT("Phase2C"):TEXT("Fleet300"));
 if(Focused && ActionPressed(TEXT("OVDeveloper"))) Action(TEXT("Developer"));
 if(Focused && ActionPressed(TEXT("OVCalibrationRange"))) Action(bDeveloperMode?TEXT("CalibrationRange"):TEXT("Fleet200"));
 if(Focused && ActionPressed(TEXT("OVCombatRange"))) Action(bDeveloperMode?TEXT("CombatRange"):TEXT("Fleet100"));
 if(WasInputKeyJustPressed(EKeys::F9)) UE_LOG(LogTemp,Display,TEXT("OV VERIFY: camera=%s yaw=%.1f pitch=%.1f distance=%.1f ship=%s pos=%s manual=%d moving=%d paused=%d speed=%.0f sim=%.3f"),*CameraFocus.ToString(),CameraYaw,CameraPitch,CameraDistance,Selected?*Selected->ShipName:TEXT("none"),Selected?*Selected->GetActorLocation().ToString():TEXT("none"),Selected?Selected->bManual:0,Selected?Selected->bHasDestination:0,bSimulationPaused,SimulationSpeed,SimulationSeconds);
 if(ActionPressed(TEXT("OVPause"))) Action(TEXT("Pause"));
 if(ActionPressed(TEXT("OVSpeed1"))) Action(TEXT("Speed1"));
 if(ActionPressed(TEXT("OVSpeed2"))) Action(TEXT("Speed2"));
 if(ActionPressed(TEXT("OVSpeed3"))) Action(TEXT("Speed3"));
 if(ActionPressed(TEXT("OVCancel"))) Action(TEXT("Cancel"));
 if(InWorld) {
  if(ActionPressed(TEXT("OVFollow"))) Action(TEXT("Focus"));
  if(ActionPressed(TEXT("OVMove"))) Action(TEXT("Move"));
  if(ActionPressed(TEXT("OVTarget"))) Action(TEXT("Target"));
  if(ActionPressed(TEXT("OVHeading"))) Action(TEXT("Heading"));
  if(ActionPressed(TEXT("OVStop"))) Action(TEXT("Stop"));
  if(ActionPressed(TEXT("OVRepair"))) Action(TEXT("Repair"));
  if(ActionPressed(TEXT("OVFire"))) Action(TEXT("Fire"));
  if(ActionPressed(TEXT("OVFireAll"))) Action(TEXT("FireAll"));
  if(ActionPressed(TEXT("OVNextWeapon"))) Action(TEXT("NextWeapon"));
  for(int32 I=0;I<9;++I) if(ActionPressed(FName(*FString::Printf(TEXT("OVGroup%d"),I+1)))) {
   if(Ctrl) { auto& Group=Groups.FindOrAdd(I); Group.Reset(); for(AOVShip* S:Selection) if(S->bFriendly) Group.Add(S); }
   else if(const auto* Group=Groups.Find(I)) { ClearSelection(); for(const auto& S:*Group) if(S.IsValid()&&!S->Systems->bWrecked) SelectAdditional(S.Get()); }
  }
  if(!bPlayGame3&&WasInputKeyJustPressed(EKeys::RightMouseButton)) {
   bRightGesture=true; RightStart=FVector2D(MX,MY); RightTarget=nullptr;
   FHitResult Hit; if(!Shift&&GetHitResultUnderCursor(ECC_Visibility,false,Hit)) {
    AOVShip* Target=Cast<AOVShip>(Hit.GetActor()); if(Target&&Selected&&Target->bFriendly!=Selected->bFriendly) RightTarget=Target;
   }
   if(!RightTarget.IsValid()&&!bPlacing) { Action(TEXT("Move")); FVector O,D,Raw; if(DeprojectMousePositionToWorld(O,D)&&FOVOrderInput::Project(O,D,PlacementPlaneZ,Raw,ProjectionNearDistance,CommandRayLength())) { Preview.X=FMath::Clamp(Raw.X,-CommandXY(),CommandXY()); Preview.Y=FMath::Clamp(Raw.Y,-CommandXY(),CommandXY()); } }
  }
 }
 if(bRightGesture&&WasInputKeyJustReleased(EKeys::RightMouseButton)) {
  if(!OverUI&&RightTarget.IsValid()&&FVector2D::Distance(RightStart,FVector2D(MX,MY))<=25) SetTarget(RightTarget.Get());
  bRightGesture=false; RightTarget=nullptr;
 }
 UpdateCamera();
 if(bPlacing) UpdatePreview();
 if(InWorld&&!RightMove.bActive&&WasInputKeyJustPressed(EKeys::Enter)) ConfirmMove();
 if(LeftPressed && InWorld && !RightMove.bActive && OrbitGesture.Surface==EOVOrbitSurface::None) {
  if(bPlacing) ConfirmMove();
  else if(InputMode==EOVInputMode::Heading) { FVector O,D,P; if(DeprojectMousePositionToWorld(O,D)&&FOVOrderInput::Project(O,D,PlacementPlaneZ,P,ProjectionNearDistance,CommandRayLength())) IssueHeading(P); }
  else { FHitResult Hit; AOVShip* Ship=nullptr; if(GetHitResultUnderCursor(ECC_Visibility,false,Hit)) Ship=Cast<AOVShip>(Hit.GetActor());
   if(InputMode==EOVInputMode::Target) { if(Ship) SetTarget(Ship); }
   else SelectShipFromUser(Ship,Shift);
  }
 }
 StepSimulation(DeltaTime);
 CSV_SCOPED_TIMING_STAT_EXCLUSIVE(OV_WorldEffects);
 TRACE_CPUPROFILER_EVENT_SCOPE(OV_WorldEffects);
 UpdateCombatPresentation();
 if(bShowDebugOutlines) {
  if(Combat && Combat->bEnabled) {
   // Presentation reads resolved events. It never requests fire or applies damage.
   const float FX=CalibrationScenario>=0?20.f:1.f;
   for(const auto& Shot:Combat->Attacks) {
    const FColor Color=Shot.Weapon.Kind==EOVAttackKind::Cannon?FColor::Orange:FColor::Yellow;
    DrawDebugLine(GetWorld(),Shot.Position-Shot.Direction*FMath::Max(100.f,Shot.Weapon.Speed*.035f),Shot.Position,Color,false,-1,0,4);
    DrawDebugSphere(GetWorld(),Shot.Position,12*FX,6,Color,false,-1,0,2);
   }
   for(const auto& Event:Combat->RecentEvents) if(Combat->GameTime-Event.ResolvedAt<.20) {
    if(Event.Weapon.Kind==EOVAttackKind::Beam) DrawDebugLine(GetWorld(),Event.Origin,Event.Position,Event.FactionId==0?FColor::Cyan:FColor::Red,false,-1,0,4);
    DrawDebugSphere(GetWorld(),Event.Origin,15*FX,8,Event.FactionId==0?FColor::Cyan:FColor::Red,false,-1,0,3);
    if(Event.Result==EOVAttackResult::Hit) {
     DrawDebugSphere(GetWorld(),Event.Position,35*FX,12,FColor::Orange,false,-1,0,3);
     for(AOVShip* Hit:Ships) if(Hit->GetFName()==Event.HitShipId && Hit->Systems->bShieldActive && Hit->Systems->Layout)
      DrawDebugBox(GetWorld(),Hit->GetActorLocation(),Hit->Systems->Layout->LocalHalfExtent*Hit->GetActorScale3D()*1.06,Hit->GetActorQuat(),FColor(100,160,255),false,-1,0,4);
    }
   }
   for(const auto& State:Combat->Ships) if(IsValid(State.Ship)&&State.Ship->Systems->Layout&&!State.Ship->Systems->bWrecked&&!State.bDeparted) {
    for(const auto& Mount:State.Mounts) {
     const auto* Def=State.Ship->Systems->Layout->WeaponMounts.FindByPredicate([&](const FOVWeaponMountDefinition& M){return M.WeaponId==Mount.MountId;});
     if(!Def) continue;
     const FVector Base=State.Ship->GetActorTransform().TransformPosition(Def->LocalPosition);
     DrawDebugLine(GetWorld(),Base,Base+State.Ship->GetActorQuat().RotateVector(Mount.LocalAim.Vector())*100*FX,FColor::Silver,false,-1,0,3);
    }
   }
  }
  const double GridStep=CalibrationScenario>=0 ? OVCalibration::GridStep : 500;
  const double GridExtent=CommandXY();
  const float GridLine=CalibrationScenario>=0 ? 40.f : 2.f;
  for(int I=-12;I<=12;++I) {
   DrawDebugLine(GetWorld(),FVector(I*GridStep,-GridExtent,0),FVector(I*GridStep,GridExtent,0),I==0?FColor(50,130,180):FColor(18,35,55),false,-1,0,GridLine);
   DrawDebugLine(GetWorld(),FVector(-GridExtent,I*GridStep,0),FVector(GridExtent,I*GridStep,0),I==0?FColor(180,75,60):FColor(18,35,55),false,-1,0,GridLine);
  }
  for(AOVShip* Ship:Ships) {
   if(Combat && Combat->bEnabled && Combat->IsDeparted(Ship)) continue;
   const FVector P=Ship->GetActorLocation();
   DrawDebugLine(GetWorld(),FVector(P.X,P.Y,0),P,Ship->bFriendly?FColor(30,80,110):FColor(140,30,30),false,-1,0,2);
   if(Ship->bSelected) { FVector Center,Extent; Ship->GetActorBounds(false,Center,Extent); DrawDebugBox(GetWorld(),Center,Extent+FVector(25),FColor::Cyan,false,-1,0,3); }
   if(Ship->bHasDestination) { DrawDebugLine(GetWorld(),P,Ship->Destination,FColor(90,200,140),false,-1,0,3); DrawDebugSphere(GetWorld(),Ship->Destination,70,12,FColor(90,200,140),false,-1,0,2); }
   if(Ship->AttackTarget&&!Ship->AttackTarget->Systems->bWrecked&&!(Combat&&Combat->bEnabled&&Combat->IsDeparted(Ship->AttackTarget))) { DrawDebugLine(GetWorld(),P,Ship->AttackTarget->GetActorLocation(),FColor::Red,false,-1,0,2); DrawDebugSphere(GetWorld(),Ship->AttackTarget->GetActorLocation(),150,12,FColor::Red,false,-1,0,2); }
  }
  if(Selected&&Selected->Systems->Layout) {
   auto* S=Selected->Systems.Get(); const auto* L=S->Layout.Get(); const FTransform Transform=Selected->GetActorTransform();
   if(bDeveloperMode && L->ArmorSections.IsValidIndex(SelectedArmor)) { const auto& A=L->ArmorSections[SelectedArmor]; DrawDebugBox(GetWorld(),Transform.TransformPosition(A.LocalCenter),A.HalfExtent*Transform.GetScale3D(),Transform.GetRotation(),FColor::Orange,false,-1,0,3); }
   if(S->HitEventId>0&&bDeveloperMode) { DrawDebugSphere(GetWorld(),S->LastWorldHit,40,10,FColor::Magenta,false,-1,0,4); if(L->Slots.IsValidIndex(S->LastDamageIndex)) DrawDebugLine(GetWorld(),S->LastWorldHit,Transform.TransformPosition(L->Slots[S->LastDamageIndex].LocalCenter),FColor::Magenta,false,-1,0,3); }
  }
 }
 if(bPlacing || (RightMove.bActive && RightMove.bValidPoint)) {
  const FVector Ground(Preview.X,Preview.Y,PlacementPlaneZ);
  // The disc and projection use exactly the same selected-ship elevation.
  DrawDebugCircle(GetWorld(),Ground,600,48,FColor(30,110,140),false,-1,0,2,FVector::ForwardVector,FVector::RightVector,false);
  if(OrderInput.bShowAltitude) DrawDebugLine(GetWorld(),Ground,Preview,FColor::Yellow,false,-1,0,5);
  DrawDebugSphere(GetWorld(),Preview,90*(CalibrationScenario>=0?20:1),16,FColor::Yellow,false,-1,0,3);
  DrawDebugCircle(GetWorld(),Ground,180*(CalibrationScenario>=0?20:1),32,FColor::Yellow,false,-1,0,3,FVector::ForwardVector,FVector::RightVector,false);
  if(Selected) DrawDebugLine(GetWorld(),Selected->GetActorLocation(),Preview,FColor::Yellow,false,-1,0,2);
 }
}
