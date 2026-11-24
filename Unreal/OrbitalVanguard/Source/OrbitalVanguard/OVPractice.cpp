#include "OVPlayScenarioSubsystem.h"
#include "OVController.h"
#include "OVHUD.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVBattleNotices.h"
#include "Misc/ConfigCacheIni.h"

namespace {
struct FGoal { FName Id; FString Label,Input; };
struct FLesson { FString Title,Summary; TArray<FGoal> Goals; };
#define G(Id,Label,Input) {TEXT(#Id),TEXT(Label),TEXT(Input)}
const FLesson Lessons[]={
 {TEXT("기본 조작"),TEXT("두 척을 선택하고 표식까지 이동하세요.\n전술 시간을 멈춘 채로도 명령을 준비할 수 있습니다."),{
 G(Pan,"시야 이동","W A S D로 전장을 둘러보세요."),G(Orbit,"함선을 다른 각도에서 보기","휠 버튼을 누른 채 드래그하세요."),G(Zoom,"가까이 보기","마우스 휠을 굴리세요."),G(Single,"초계함 한 척 선택","첫 번째 초계함을 클릭하세요."),G(Multi,"두 척 함께 선택","Shift를 누르고 두 번째 함선을 클릭하세요."),G(Plane,"평면 표식까지 이동","첫 초계함만 선택하고 표식을 우클릭하세요."),G(Height,"고도를 바꿔 이동","우클릭 드래그로 고도를 맞추세요. V·Enter로 정밀 지정할 수도 있습니다."),G(Pause,"전술 시간 멈추기","Space를 눌러 정지하세요."),G(Speed,"두 배속으로 재개","상단 2배 버튼이나 숫자 2를 누르세요.")}},
 {TEXT("인터페이스"),TEXT("항공모함을 선택하고 쉴드·선체, 시설, 영웅과 기체 상태를 찾아보세요."),{
 G(Single,"항공모함 선택","항공모함을 클릭하고 오른쪽 아래 선택 카드에서 이름을 확인하세요."),G(Details,"선체와 쉴드 읽기","함선 상세를 열어 두 게이지와 수치를 비교하세요."),G(Room,"시설 하나 선택","선내 지도에서 방 하나를 클릭하세요."),G(HeroPanel,"영웅 명단 열기","영웅 버튼을 눌러 탑승 인원을 확인하세요."),G(Hangar,"격납고 열기","영웅 창을 닫고 함선 상세의 격납고를 여세요."),G(Feed,"전투 알림 확인","전투 알림을 열고 항모 기체 현황 항목을 클릭하세요.")}},
 {TEXT("명령"),TEXT("표적 지정과 실제 발사를 구별합니다.\n이동 자동화와 사격 자동화는 따로 설정합니다."),{
 G(Move,"이동 명령 내리기","아군을 선택하고 빈 공간을 우클릭하세요."),G(Hold,"현 위치에 정지","함선 명령에서 정지를 누르세요."),G(Target,"적함을 수동 표적으로 지정","표적 지정을 누르고 적함을 클릭하세요."),G(Fire,"선택 표적에 사격","전체 발사를 누르세요. 포탑이 돌아간 뒤 실제로 발사합니다."),G(AutoFire,"자동 사격 비교","사격 자동을 켜고 다음 발사를 기다리세요."),G(Cease,"선택함 사격 중지","사격 수동으로 바꾸고 공격 취소를 누르세요.")}},
 {TEXT("기초 전투"),TEXT("탐지에서 피격까지 관찰하세요.\n요격함 곁에서 적 함재기의 접근을 확인합니다."),{
 G(Contact,"탐지된 적함 선택","표적 지정을 눌러 적함을 선택하세요."),G(Ready,"사거리와 발사각 확인","선택 무장이 표적을 향할 때까지 기다리세요."),G(Shield,"쉴드에 명중시키기","전체 발사 또는 사격 자동으로 쉴드를 낮추세요."),G(Hull,"장갑과 선체 피해 확인","사격을 이어가며 적함의 쉴드가 꺼진 뒤 피해를 관찰하세요."),G(Intercept,"적 기체 요격 관찰","아군 방공함 가까이 접근한 기체가 격추될 때까지 기다리세요.")}},
 {TEXT("함종과 역할"),TEXT("초계함은 전방 차폐와 정찰, 방공함은 요격, 레이더함은 공유 탐지를 맡습니다. 전함·항모·초대형함은 주력 전력입니다."),{
 G(Single,"방공함 선택","방공함을 클릭하세요."),G(Escort,"주력함 호위 위치에 도착","전함 옆의 표식으로 방공함을 이동시키세요.")}},
 {TEXT("시설과 무장"),TEXT("안쪽 시설과 외부 장착점을 구별하고 시험함의 실제 장비를 바꿉니다."),{
 G(Core,"코어 선택","선내 지도에서 코어를 클릭하세요."),G(Bridge,"함교 선택","함교를 클릭하세요. 필요하면 층을 바꾸세요."),G(Engine,"엔진 선택","선체 뒤쪽 엔진을 클릭하세요."),G(Control,"무기제어실 선택","무기제어실을 클릭하세요."),G(Equip,"호환 무장으로 교체","지도 창을 닫고 도움말의 호환 장비를 누르세요."),G(Reject,"장착 티어 제한 확인","상위 티어 장착을 시도해 거부 이유를 확인하세요.")}},
 {TEXT("함재기"),TEXT("실제 항공모함 재고를 출격시키고 같은 기체가 모두 돌아올 때까지 확인합니다."),{
 G(Hangar,"항공모함 격납고 열기","항모를 선택하고 함선 상세에서 격납고를 여세요."),G(Launch,"대기 편대 출격","격납고의 출격을 누르세요."),G(Recall,"편대 귀환 명령","귀환을 누르세요. 비행 중인 기체를 확인하세요."),G(Docked,"원래 편대의 격납 확인","귀환 편대가 모두 보관 상태가 될 때까지 기다리세요.")}},
 {TEXT("영웅"),TEXT("보직에 배치한 영웅의 실제 위치와 효과를 보고 수호 임무로 바꿉니다."),{
 G(HeroPanel,"영웅 명단 열기","영웅 버튼에서 탑승 영웅을 확인하세요."),G(Duty,"보직 활성화","탑승 영웅을 선택하고 지원 보직에 배치하세요."),G(Guard,"수호로 전환","선내에서 같은 영웅을 선택해 수호를 명령하세요."),G(RestoreDuty,"보직으로 복귀","같은 영웅을 다시 보직에 배치하세요.")}},
 {TEXT("강습"),TEXT("원래 수송수단으로 출발하고 철수합니다. 아군이 적함 안에 있으면 포격에 주의하세요."),{
 G(BoardTarget,"강습 대상 선택","선내 지도에서 대원을 선택하고 강습 명령으로 적함을 지정하세요."),G(Transit,"대원 출발 확인","포드나 강습선을 선택해 탑승을 기다린 뒤 출발을 확인하세요."),G(Inside,"적함 내부 진입","쉴드 상태와 강습팀 진입 위치를 확인하세요."),G(Operation,"점령 또는 공작 수행","선내 지도에서 대원을 골라 적 시설을 점령하거나 파괴하세요."),G(Return,"강습팀 철수","철수를 명령하고 진입점으로 돌아가 원 수송수단으로 회수하세요.")}}
};
#undef G
}
FString UOVPlayScenarioSubsystem::PracticeTitle(int32 c){return c>=0&&c<UE_ARRAY_COUNT(Lessons)?Lessons[c].Title:FString();}
FString UOVPlayScenarioSubsystem::PracticeSummary(int32 c){return c>=0&&c<UE_ARRAY_COUNT(Lessons)?Lessons[c].Summary:FString();}
int32 UOVPlayScenarioSubsystem::CompletedPracticeMask(){int32 mask=0;GConfig->GetInt(TEXT("OrbitalVanguard.Practice"),TEXT("Completed"),mask,GGameUserSettingsIni);return mask;}
int32 UOVPlayScenarioSubsystem::PracticeStepCount()const{return PracticeChapter>=0&&PracticeChapter<9?Lessons[PracticeChapter].Goals.Num():0;}
FString UOVPlayScenarioSubsystem::PracticeObjective()const{return PracticeStep<PracticeStepCount()?Lessons[PracticeChapter].Goals[PracticeStep].Label:TEXT("장 완료");}
FString UOVPlayScenarioSubsystem::PracticeInput()const{return PracticeStep<PracticeStepCount()?Lessons[PracticeChapter].Goals[PracticeStep].Input:TEXT("다른 장을 고르거나 다시 실습할 수 있습니다.");}
bool UOVPlayScenarioSubsystem::HasPracticeMarker()const{
 if(PracticeStep>=PracticeStepCount())return false;
 auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;return key=="Plane"||key=="Height"||key=="Escort";
}
bool UOVPlayScenarioSubsystem::StartPractice(AOVController* pc,int32 chapter)
{
 if(!pc||chapter<0||chapter>=UE_ARRAY_COUNT(Lessons))return false;
 TGuardValue<bool> preparing(bPreparingBoardingLesson,chapter==8);
 if(!Start(*pc,chapter==8?TEXT("HeroArena"):TEXT("Practice")))return false;
 PracticeChapter=chapter; PracticeStep=0; bPracticeDetail=false;PracticeRoleCard=0;
 GConfig->GetBool(TEXT("OrbitalVanguard.Practice"),TEXT("Hints"),bPracticeHints,GGameUserSettingsIni);
 if(chapter==8){
    HandleAction(*pc,TEXT("ArenaFill")); PracticeShip=pc->Ships[0];PracticeEnemy=pc->Ships[1];
 }else{
    // Isolate the GameInstance roster for the whole lesson, including a restart.
    ArenaRoster=pc->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
    ArenaSavedRoster=MoveTemp(ArenaRoster->Heroes);ArenaSavedTiers=MoveTemp(ArenaRoster->CommandTiers);
    ArenaRoster->CommandTiers.Add(0,3);ArenaRoster->CommandTiers.Add(1,3);bArenaOwnsRoster=true;
    auto add=[&](FName hull,int side,FVector pos,int wing=0){
       auto* ship=Spawn(*pc,{hull,1,wing,2,0},side,pc->Ships.Num(),pos);
       if(ship){ship->SetAutomation(false,false);ship->StopMovement();}
       return ship;
    };
    const FName hull=(chapter==1||chapter==6||chapter==7)?TEXT("carrier"):chapter==3?TEXT("battleship"):chapter==4?TEXT("air_defense"):TEXT("corvette");
    PracticeShip=add(hull,0,FVector(0,0,2000),4);
    if(chapter==0||chapter==4) {if(!add(chapter==0?TEXT("frigate"):TEXT("battleship"),0,FVector(-9000,9500,2000))){pc->LeavePlayScenario();return false;}}
    if(chapter==2||chapter==3)PracticeEnemy=add(TEXT("frigate"),1,FVector(21500,0,2000));
    if(chapter==3 && (!add(TEXT("air_defense"),0,FVector(4000,7000,2000)) || !add(TEXT("carrier"),1,FVector(24500,10000,2000),3))){pc->LeavePlayScenario();return false;}
    if(!PracticeShip.IsValid()||((chapter==2||chapter==3)&&!PracticeEnemy.IsValid())){pc->LeavePlayScenario();return false;}
    if(chapter==1||chapter==7)AssignScenarioHeroes(*pc,0);
 }
 pc->SelectShip(PracticeShip.Get());pc->FrameEngagement();pc->bFollow=false;
 if(chapter==0||chapter==1||chapter==4)pc->ClearSelection();
 pc->bSimulationPaused=false;
 BeginPracticeStep();return true;
}
void UOVPlayScenarioSubsystem::BeginPracticeStep(){
 auto* pc=Owner.Get();if(!pc||!PracticeShip.IsValid()||PracticeStep>=PracticeStepCount())return;
 bPracticeReady=false;bPracticeOrder=false;PracticeCamera=pc->CameraFocus;PracticeAngle=pc->CameraYaw;PracticePitch=pc->CameraPitch;PracticeZoom=pc->CameraDistance;
 PracticeOrigin=PracticeShip->GetActorLocation();PracticeShots=pc->Combat->ShotCount;
 auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;
 if(key=="Plane"||key=="Height")PracticeGoal=PracticeOrigin+FVector(7000,4500,key=="Height"?4000:0);
 if(key=="Escort"&&pc->Ships.Num()>1)PracticeGoal=pc->Ships[1]->GetActorLocation()+FVector(10500,0,0);
 if(key=="Feed"){
    const int32 count=pc->Carrier->CountAircraft(PracticeShip.Get(),EOVAircraftState::Stored);
    GetWorld()->GetSubsystem<UOVBattleNotices>()->RecordAircraftAdjustment(PracticeShip.Get(),count,count,pc->SimulationSeconds);
 }
 if(key=="Intercept"){
    for(AOVShip* candidate:pc->Ships)if(candidate && !candidate->bFriendly && candidate->HullDefinitionId=="carrier"){
      pc->Combat->SetTarget(candidate,PracticeShip.Get());pc->Carrier->Launch(candidate);break;
    }
 }
}
void UOVPlayScenarioSubsystem::ObservePractice(){
 auto* pc=Owner.Get();auto* ship=PracticeShip.Get();if(!pc||!ship||bPracticeReady||PracticeStep>=PracticeStepCount())return;
 auto* hud=Cast<AOVHUD>(pc->GetHUD());if(!hud)return;
 auto* enemy=PracticeEnemy.Get();const auto k=Lessons[PracticeChapter].Goals[PracticeStep].Id;
 bool ok=false;
 if(k=="Pan")ok=FVector::Dist2D(PracticeCamera,pc->CameraFocus)>100;
 else if(k=="Orbit")ok=FMath::Abs(FMath::FindDeltaAngleDegrees(PracticeAngle,pc->CameraYaw))+FMath::Abs(FMath::FindDeltaAngleDegrees(PracticePitch,pc->CameraPitch))>5;
 else if(k=="Zoom")ok=FMath::Abs(pc->CameraDistance-PracticeZoom)>100;
 else if(k=="Single")ok=pc->Selected==ship&&pc->Selection.Num()==1;
 else if(k=="Multi")ok=pc->Selection.Num()>=2&&pc->Selection.Contains(ship);
 else if(HasPracticeMarker()){
    const FVector target=ship->Destination;
    if(ship->bHasDestination && FVector::Dist2D(target,PracticeGoal)<2000 && FMath::Abs(target.Z-PracticeGoal.Z)<1100)bPracticeOrder=true;
    ok=bPracticeOrder && FVector::Dist(ship->GetActorLocation(),PracticeGoal)<2100 && !ship->bHasDestination;
 }else if(k=="Pause")ok=pc->bSimulationPaused;
 else if(k=="Speed")ok=!pc->bSimulationPaused&&pc->SimulationSpeed==2;
 else if(k=="Details")ok=hud->bDetailsOpen&&pc->Selected==ship;
 else if(k=="Room"||k=="Core"||k=="Engine"||k=="Bridge"||k=="Control"){
    int i=ship->Systems->Layout->GetSlotIndex(hud->InteriorTargetSlot);
    if(hud->bInteriorOpen&&hud->InteriorShip==ship&&ship->Systems->Layout->Slots.IsValidIndex(i)){
       auto f=ship->Systems->Layout->Slots[i].Facility;
       ok=k=="Room"||(k=="Core"&&f==EOVFacilityType::Core)||(k=="Engine"&&f==EOVFacilityType::Engine)||(k=="Bridge"&&f==EOVFacilityType::Bridge)||(k=="Control"&&f==EOVFacilityType::WeaponControl);
    }
 }else if(k=="HeroPanel")ok=hud->IsHeroPanelOpen();
 else if(k=="Feed")ok=bPracticeOrder;
 else if(k=="Hangar")ok=pc->Selected==ship&&hud->IsPracticeHangarVisible();
 else if(k=="Move")ok=ship->bHasDestination;
 else if(k=="Hold")ok=ship->bHold&&!ship->bHasDestination;
 else if(k=="Target")ok=enemy&&ship->AttackTarget==enemy&&!ship->IsAttackAutomatic()&&pc->Combat->ShotCount==PracticeShots;
 else if(k=="Contact")ok=enemy&&ship->AttackTarget==enemy&&pc->Combat->GetDetection(ship,enemy).bDetected;
 else if(k=="Ready"){for(const auto& m:pc->Combat->GetShipState(ship).Mounts)ok|=pc->Combat->GetReadiness(ship,m.MountId).bReady;}
 else if(k=="Fire"||k=="AutoFire")ok=pc->Combat->ShotCount>PracticeShots && (k=="Fire"||ship->IsAttackAutomatic());
 else if(k=="Cease")ok=!ship->IsAttackAutomatic()&&!ship->AttackTarget;
 else if(k=="Shield")ok=enemy&&enemy->Systems->Shield<enemy->Systems->MaxShield&&pc->Combat->HitCount>0;
 else if(k=="Hull")ok=enemy&&enemy->Systems->HullHP<enemy->Systems->MaxHullHP;
 else if(k=="Intercept")ok=pc->PointDefense->KillCount>0;
 else if(k=="Launch"||k=="Recall"||k=="Docked"){
    int returned=0;for(const auto& a:pc->Carrier->Aircraft){
      if(a.FactionId!=0)continue;
      if(k=="Launch"&&a.State==EOVAircraftState::Sortie)PracticeWing.Add(a.AircraftId);
      if(!PracticeWing.Contains(a.AircraftId))continue;
      if(a.State==EOVAircraftState::Returning||a.State==EOVAircraftState::DockWaiting||a.State==EOVAircraftState::Stored)ok=true;
      returned+=a.State==EOVAircraftState::Stored?1:0;
    }
    if(k=="Launch")ok=PracticeWing.Num()>0;
    if(k=="Docked")ok=PracticeWing.Num()>0&&returned==PracticeWing.Num();
 }else if(k=="Duty"||k=="Guard"||k=="RestoreDuty"){
    for(const auto& u:GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->Units){
       if(!u.HeroId.IsValid()||u.FactionId!=0||u.Ship.Get()!=ship)continue;
       if(k=="Duty"&&u.bDutyActive){PracticeParticipant=u.UnitId;ok=true;break;}
       if(u.UnitId==PracticeParticipant)ok=k=="Guard"?(!u.bDutyActive&&u.Order==EOVInteriorOrder::Defend):u.bDutyActive;
    }
 }else if(k=="BoardTarget")ok=pc->LastCrewOperation()>0||bArenaStarted;
 else if(k=="Transit"||k=="Inside"||k=="Operation"||k=="Return"){
    for(const auto& u:GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->Units){
      if(u.FactionId!=0)continue;
      if(k=="Transit"&&(u.State==EOVInteriorState::InTransit||u.Ship.Get()==enemy))ok=true;
      if(k=="Inside"&&u.Ship.Get()==enemy){PracticeWing.Add(u.UnitId);ok=true;}
      if(u.Ship.Get()==enemy)PracticeWing.Add(u.UnitId);
    }
    if(k=="Return"){
       bool recovered=false,pending=false;
       for(const auto& person:pc->Transport->Persons)if(PracticeWing.Contains(person.RuntimeUnitId)){
          bool home=person.State==EOVPassengerState::Aboard && (person.CurrentShip.Get()==ship || (person.CurrentShip.IsValid() && person.CurrentShip->HangarHost.Get()==ship));
          recovered|=home||person.State==EOVPassengerState::RescueWaiting;
          pending|=!home && person.State!=EOVPassengerState::RescueWaiting && person.State!=EOVPassengerState::Dead;
       }
       ok=recovered&&!pending;
    }
    if(k=="Operation"&&enemy)for(const auto& op:GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->Operations)ok|=op.Ship.Get()==enemy && op.FactionId==0 && op.bCompleted;
 }
 bPracticeReady=ok;
}
void UOVPlayScenarioSubsystem::ConfirmPracticeNotice(FName subject){
 if(PracticeChapter==1 && PracticeStep==5 && PracticeShip.IsValid() && PracticeShip->GetFName()==subject)bPracticeOrder=true;
}
bool UOVPlayScenarioSubsystem::PracticeEquipment(bool invalid){
 auto* pc=Owner.Get();auto* ship=PracticeShip.Get();
 if(!pc||!ship||PracticeChapter!=5||PracticeStepCount()<=PracticeStep)return false;
 auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;if(key!=(invalid?TEXT("Reject"):TEXT("Equip")))return false;
 for(auto& m:pc->Combat->GetShipState(ship).Mounts){
   for(const auto& p:pc->Combat->Settings->Weapons){
     if(m.Profile.Family!=p.Family||m.Profile.ProfileId==p.ProfileId||(invalid?p.Tier<=m.Profile.Tier:p.Tier!=m.Profile.Tier))continue;
     const bool accepted=pc->Combat->SetWeaponProfile(ship,m.MountId,p.ProfileId);
     if(accepted!=invalid){bPracticeReady=true;pc->LastAction=invalid?TEXT("이 슬롯에는 상위 티어 무장을 장착할 수 없습니다."):TEXT("호환 무장을 장착했습니다.");return true;}
   }
 }
 return false;
}
bool UOVPlayScenarioSubsystem::AdvancePractice(){
 ObservePractice();if(!bPracticeReady||PracticeStep>=PracticeStepCount())return false;
 ++PracticeStep;bPracticeReady=false;
 if(PracticeStep==PracticeStepCount()){
    GConfig->SetInt(TEXT("OrbitalVanguard.Practice"),TEXT("Completed"),CompletedPracticeMask()|(1<<PracticeChapter),GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
 }else BeginPracticeStep();
 return true;
}

FName UOVPlayScenarioSubsystem::PracticeControl() const{
 if(PracticeChapter<0||PracticeStep>=PracticeStepCount())return NAME_None;
 const auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;
 static const TMap<FName,FName> Controls={{"Details","Details"},{"HeroPanel","CommandHeroPanel"},{"Feed","BattleFeedToggle"},{"Hangar","HangarView"},{"Move","Move"},{"Hold","Stop"},{"Target","Target"},{"Contact","Target"},{"Fire","FireAll"},{"AutoFire","FireAuto"},{"Cease","CancelAttack"},{"Pause","Pause"},{"Speed","Speed2"},{"Launch","HangarLaunchAircraft"},{"Recall","HangarRecallAircraft"},{"BoardTarget","OpsAssault"}};
 return Controls.FindRef(key);
}
AOVShip* UOVPlayScenarioSubsystem::PracticeFocus() const{
 if(PracticeChapter<0||PracticeStep>=PracticeStepCount())return nullptr;
 const auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;
 if(key=="Single"||key=="Multi")return PracticeShip.Get();
 return key=="Contact"||key=="Target"||key=="BoardTarget"?PracticeEnemy.Get():nullptr;
}

void UOVPlayScenarioSubsystem::RetryPracticeStep(){
 if(PracticeChapter<0||PracticeStep>=PracticeStepCount()||!Owner.IsValid())return;
 // Boarding can lose the transport; a repeat needs a fresh isolated team and ship.
 if(PracticeChapter==8 || !PracticeShip.IsValid() || PracticeShip->Systems->bWrecked || (PracticeEnemy.IsValid() && PracticeEnemy->Systems->bWrecked)){
    StartPractice(Owner.Get(),PracticeChapter);return;
 }
 const auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;
 if(key=="Plane"||key=="Height"||key=="Escort")PracticeShip->StopMovement();
 if(key=="Single"||key=="Multi")Owner->ClearSelection();
 BeginPracticeStep();
}
