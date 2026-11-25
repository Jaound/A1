#include "OVPlayScenarioSubsystem.h"
#include "OVController.h"
#include "OVCalibration.h"
#include "OVDevelopmentSubsystem.h"
#include "OVHUD.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "OVCombatSubsystem.h"
#include "OVCarrierSubsystem.h"
#include "OVPointDefenseSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVBattleNotices.h"
#include "OVLogisticsSubsystem.h"
#include "Misc/ConfigCacheIni.h"

namespace {
struct FGoal { FName Id; FString Label,Input; };
struct FLesson { FString Title,Summary; TArray<FGoal> Goals; };
#define G(Id,Label,Input) {TEXT(#Id),TEXT(Label),TEXT(Input)}
const FLesson Lessons[]={
 {TEXT("기본 조작"),TEXT("카메라를 움직이고 두 척을 선택해 표식까지 이동하세요. 시간을 멈춘 채 명령을 준비할 수도 있습니다."),{
 G(Pan,"시야 이동","W A S D로 전장을 둘러보세요."),G(Orbit,"함선을 다른 각도에서 보기","휠 버튼을 누른 채 마우스를 움직이세요."),G(Zoom,"가까이 보기","마우스 휠을 굴리세요."),G(Single,"초계함 한 척 선택","전장의 초계함을 클릭하세요. 아래 명령창에 선택한 함명이 뜹니다."),G(Multi,"두 척 함께 선택","Shift를 누르고 호위함을 클릭하세요. 오른쪽 아래에 두 척의 카드가 표시됩니다."),G(Plane,"평면 표식까지 이동","초계함만 다시 선택하고 전장 표식을 우클릭하세요. 실제 도착을 기다리세요."),G(Height,"고도를 바꿔 이동","표식에서 오른쪽 버튼을 누른 채 마우스를 위아래로 움직여 높이를 맞추세요. V와 Enter로 정밀 지정할 수도 있습니다."),G(Pause,"전술 시간 멈추기","Space를 눌러 정지하세요."),G(Speed,"두 배속으로 재개","숫자 2 또는 상단 2배를 누르세요. 시계가 두 배로 흐릅니다.")}},
 {TEXT("인터페이스"),TEXT("항공모함의 피해, 시설, 탑승 영웅과 기체 현황을 찾아봅니다."),{
 G(Single,"항공모함 선택","전장의 항공모함을 클릭하고 오른쪽 아래 선택 카드에서 이름을 확인하세요."),G(Details,"선체 HP와 쉴드 잔량 읽기","왼쪽 아래 ‘함선 상세’를 여세요. 모델 아래 왼쪽 청록 바는 선체 HP, 오른쪽 파란 바는 쉴드입니다. 둘 다 현재 / 최대 값이며 쉴드가 먼저 피해를 받습니다."),G(Room,"시설 하나 선택","명령창의 ‘선내 지도’를 누른 뒤 지도에 표시된 방 하나를 클릭하세요. 시설명과 내구도를 확인하세요."),G(HeroPanel,"영웅 명단 열기","왼쪽 아래 ‘영웅’을 눌러 항공모함에 탑승한 영웅의 이름을 확인하세요."),G(Hangar,"격납고 열기","영웅 창을 닫고 함선 상세 상단의 ‘격납고’를 누르세요. 각 베이에서 ‘함재기 보기’로 보관 기체를 확인할 수 있습니다."),G(Feed,"전투 알림 확인","전투 알림을 열고 항모 기체 현황 항목을 클릭하세요. 함명과 기체 수를 확인하세요.")}},
 {TEXT("명령"),TEXT("수동 표적 지정과 실제 사격을 비교합니다. 이동과 사격의 자동화는 따로 조절합니다."),{
 G(Move,"이동 명령 내리기","아군을 선택하고 빈 공간을 우클릭하세요."),G(Hold,"현 위치에 정지","왼쪽 아래 함선 명령에서 ‘정지’를 누르세요."),G(Target,"적함을 수동 표적으로 지정","명령창의 ‘표적 지정’을 누르고 붉은 적함을 클릭하세요. 수동 사격은 표적만 정하면 발사하지 않습니다."),G(Fire,"선택 표적에 사격","왼쪽 아래 ‘전체 발사’를 누르세요. 무장이 적함을 향해 정렬된 뒤 첫 탄이 발사됩니다."),G(AutoFire,"자동 사격 비교","명령창의 ‘사격 수동’을 눌러 자동으로 바꾸고 다음 발사를 기다리세요."),G(Cease,"선택함 사격 중지","명령창에서 ‘사격 자동’을 눌러 수동으로 바꾼 뒤 ‘공격 취소’를 누르세요.")}},
 {TEXT("기초 전투"),TEXT("함포의 탄착, 쉴드와 선체 피해, 요격 포탑이 적 함재기를 격추하는 과정을 관찰합니다."),{
 G(Contact,"탐지된 적함 선택","전함을 선택한 뒤 왼쪽 아래 ‘표적 지정’을 누르고 붉은 적 호위함을 클릭하세요."),G(Ready,"사거리와 발사각 확인","외부 / 무장 화면에서 선택 무장의 사거리와 방향을 보세요. 장착면이 적을 향하고 포탑이 정렬되면 발사할 수 있습니다."),G(Shield,"쉴드에 명중시키기","‘전체 발사’ 또는 ‘사격 자동’으로 적 호위함의 쉴드 잔량을 줄이세요."),G(Hull,"장갑과 선체 피해 확인","사격을 이어가세요. 쉴드가 꺼지면 명중한 면의 장갑이 피해를 줄이고 시설 또는 선체 HP가 감소합니다."),G(Intercept,"방공함이 적 함재기를 격추하는 순간 보기","적 항모가 아군 전함에 함재기를 보냈습니다. 전함 곁의 방공함을 찾아 확대하세요. 작은 적 함재기가 요격 사거리 안에 들어오면 방공 포탑이 자동 사격합니다. 한 대의 실제 격추를 확인하세요.")}},
 {TEXT("함종과 역할"),TEXT("전방 초계함, 호위 방공함, 레이더함, 전함, 항모와 구축전함을 직접 지휘합니다."),{
 G(Single,"방공함 선택","전장의 방공함을 클릭하세요. 요격 포탑은 근처 아군을 노리는 미사일과 함재기를 상대합니다."),G(Escort,"방공함으로 전함 호위","방공함을 선택하고 전함 옆 표식을 우클릭하세요. 요격 범위 안에서 호위하도록 자리를 잡습니다."),
 G(Screen,"초계함을 전방에 배치","초계함을 클릭하고 아군 앞쪽 표식을 우클릭하세요. 작은 선체와 기동성을 이용해 앞에서 탐지하고 먼저 피격을 받는 위치입니다."),
 G(Survey,"레이더함으로 적 내부 정찰","레이더함을 선택한 뒤 왼쪽 아래 ‘적 내부 정찰’을 누르고 붉은 적 구축전함을 클릭하세요. 자체 탐지권 안에서 적 시설 정보를 얻습니다."),
 G(Broadside,"전함의 집중 화력 시험","아군 전함을 선택하고 ‘표적 지정’으로 적 구축전함을 클릭한 뒤 ‘전체 발사’를 누르세요. 큰 함포의 탄착을 따라가세요."),
 G(FlightDeck,"항공모함으로 원거리 공격","아군 항모를 선택하고 ‘표적 지정’으로 적 구축전함을 지정하세요. 왼쪽 아래 ‘함재기 출격’을 눌러 기체가 실제 공격할 때까지 기다리세요."),
 G(Flagship,"초대형 구축전함 지휘","아군 구축전함을 선택하고 ‘함선 상세’를 여세요. 외부 / 무장의 주포와 측면 장착점을 확인하세요. 초대형함은 결전 화력을 집중하는 전력입니다.")}},
 {TEXT("시설과 무장"),TEXT("시험함의 각 방을 직접 찾아 기능과 파괴 효과를 확인하고, 무장 세 종류를 실제 발사합니다."),{
 G(Core,"코어 · 선체 생존","시험 항공순양함의 ‘선내 지도’에서 코어를 누르세요. 코어가 파괴되면 선체 HP가 남아도 함선은 완파됩니다."),
 G(Bridge,"함교 · 명령","층 버튼으로 함교를 찾아 누르세요. 함교가 무력화되면 이동·전투 명령을 내릴 수 없습니다. 적 강습팀이 장악하면 함선 소속까지 바뀔 수 있습니다."),
 G(Engine,"엔진 · 추진","선미의 엔진을 누르세요. 가동 효율이 낮으면 함선이 느려지고 추진 기능을 잃으면 이동할 수 없습니다."),
 G(Control,"무기 제어 · 발사","무기 제어실을 누르세요. 외부 포탑이 멀쩡해도 제어 시설이 작동하지 않으면 정상적으로 사격할 수 없습니다."),
 G(LifeSupport,"생명 유지 · 승무원","생명 유지실을 누르세요. 승조원을 위한 생명 유지 설비입니다. 이 방이 파괴되면 해당 생명 유지 시설이 정지합니다."),
 G(Ammo,"탄약고 · 함포 공급","탄약고를 누르세요. 함포의 탄약 공급을 맡으며 무력화되면 함포 운용에 영향을 줍니다."),
 G(Capacitor,"축전실 · 빔 공급","축전실을 누르세요. 빔 무장의 에너지 공급을 맡고 무력화되면 빔 사격을 유지하기 어렵습니다."),
 G(MissileStorage,"미사일 저장고 · 발사체 공급","미사일 저장고를 누르세요. 미사일 계열의 공급 시설로 무력화되면 발사 능력을 잃습니다."),
 G(ShieldControl,"쉴드 제어 · 보호막","쉴드 제어실을 누르세요. 제어실이 가동되지 않으면 보호막 운용이 중단되어 선체가 노출됩니다."),
 G(Radar,"레이더 · 탐지","레이더실을 누르세요. 무력화되면 이 함선의 레이더가 탐지·공유에 기여할 수 없습니다."),
 G(Interception,"요격 관제 · 근접 방공","요격 시설을 누르세요. 관제가 무력화되면 근접 요격망에 빈틈이 생깁니다."),
 G(Boarding,"강습 시설 · 포드","강습 시설을 누르세요. 대원이 이곳에 집결해야 개인 포드를 발사할 수 있고, 파괴되면 이 출발 경로를 사용할 수 없습니다."),
 G(HangarRoom,"격납고 · 수용과 출격","격납고를 누르세요. 기체 보관과 출격, 보충 생산을 담당합니다. 파괴된 베이는 가동 정원과 생산 라인에서 제외됩니다."),
 G(FighterControl,"함재기 통제 · 지휘","함재기 통제실을 누르세요. 통제가 끊기면 출격한 기체가 통제 상실 상태에 빠질 수 있습니다."),
 G(ResourceStorage,"자원 적재실 · 보급","자원 적재실을 누르세요. 파괴되면 해당 적재 자원이 소실되어 유상 수리와 함재기 보충 생산에 쓸 수 없습니다."),
 G(Security,"보안실 · 선내 방어","보안실을 누르세요. 승무원을 배정하지 않고 가동하는 보안 시설입니다. 파괴되면 이 방의 보안 기능이 정지합니다."),
 G(CannonDemo,"함포 · 날아가는 탄환 관찰","사격 시험용 호위함을 선택하고 적 표적을 지정한 뒤 ‘전체 발사’를 누르세요. 함포의 탄은 발사 후 표적까지 날아갑니다."),
 G(BeamDemo,"빔 · 즉시 이어지는 사격선 관찰","아군 레이더함을 선택하고 같은 적 표적을 지정한 뒤 ‘전체 발사’를 누르세요. 발사 순간 이어지는 빔을 함포 탄도와 비교하세요."),
 G(MissileDemo,"미사일 · 예고 경로와 비행 관찰","아군 전략미사일함으로 적 표적을 지정하고 ‘전체 발사’를 누르세요. 느린 발사체가 곡선 예고 경로를 따라가는 동안 표적은 움직일 수 있습니다."),
 G(Equip,"같은 계열·티어의 무장 장착","아군 초계함의 외부 장착점을 교체합니다. 호환은 슬롯 계열과 허용 티어가 맞고 공급·제어 시설이 갖춰진다는 뜻입니다. 도움말 ‘호환 장비 장착’을 누르세요."),
 G(Reject,"장착 티어 제한 확인","도움말의 상위 티어 장착을 눌러 보세요. 슬롯의 상한을 넘으면 장착이 거부되고 기존 장비는 유지됩니다.")}},
 {TEXT("함재기"),TEXT("편대 출격·귀환, 대함 공격, 제공전, 자원 보급과 격납고 보충 생산을 연습합니다."),{
 G(Hangar,"항공모함 격납고 열기","항모 선택 → 왼쪽 아래 ‘함선 상세’ → 창 상단 ‘격납고’를 누르세요. 베이별 보관 수량을 확인하세요."),
 G(Launch,"격납고에서 대기 편대 출격","격납고의 ‘함재기 보기’를 누르고 보관 중인 기체 카드를 선택하세요. 목록 중앙 하단의 ‘출격’을 눌러 대기 편대를 출격시키세요."),
 G(Recall,"명령창에서 빠르게 귀환","항모를 선택한 채 격납고 창을 닫고 왼쪽 아래 함선 명령창의 ‘함재기 귀환’을 누르세요. 출격 기체가 귀환 상태로 바뀝니다."),
 G(Docked,"원래 편대의 격납 확인","격납고에서 귀환 중 → 착함 대기 → 보관을 확인하세요. 같은 기체가 모두 격납될 때까지 기다리세요."),
 G(Strike,"손상된 적 표적함 파괴","항모의 ‘표적 지정’으로 붉은 표적함을 클릭하고 명령창 ‘함재기 출격’을 누르세요. 모함은 수동 사격으로 두고 편대가 공격해 표적을 파괴하는 것을 확인하세요."),
 G(AirCover,"제공전 전환","항모 명령창의 ‘제공전’을 누르세요. 적 편대가 접근하면 아군 기체가 적 함재기를 추적합니다. 적 기체가 없으면 모함 주변을 초계합니다."),
 G(Supply,"수리함에서 항공 보급","항모 명령창 ‘항공 보급’을 누르세요. 120m 안 아군 보급수리함의 적재 자원이 항모로 이동합니다. 일반 35·고급 5가 기체 1대분입니다."),
 G(Production,"보충 생산된 기체 확인","가동 격납고에 빈자리가 있습니다. 적재 자원을 소비해 새 기체가 ‘보관’되는지 보세요. 손실 기체도 같은 방식으로 보충되며 새 기체에는 새 번호가 붙습니다."),
 G(AutoBatch,"자동 재개와 일제 출격","적 항모를 표적으로 지정하고 ‘자동 재개’ 또는 ‘자동 출격’을 켜세요. 대기 편대가 정원을 채우거나 보충 자원이 떨어지면 일제 출격합니다. 새 편대가 출격하는지 확인하세요.")}},
 {TEXT("영웅"),TEXT("명단에서 영웅을 찾아 실제 탑승 위치로 이동하고 함교 수호와 시설 보직을 전환합니다."),{
 G(HeroPanel,"영웅 명단 열기","왼쪽 아래 ‘영웅’을 누르고 아군 항모에 탄 영웅 카드를 클릭하세요."),
 G(Locate,"영웅의 현재 위치 찾기","영웅 창에서 선택한 영웅의 ‘위치 찾기’를 누른 뒤 ‘선내 지도’를 여세요. 해당 함선과 영웅의 위치가 표시됩니다."),
 G(Duty,"시설 보직 활성화","영웅 창에서 같은 영웅을 선택하세요. ‘보직 배치’ 옆의 시설 목록에서 가능한 방을 고르고 배치하세요. 영웅이 도착하면 보직 효과가 활성화됩니다."),
 G(Guard,"함교 수호 명령","영웅의 ‘선내 지도’를 열고 같은 대원을 선택하세요. 함교를 클릭하고 ‘수호’를 누르세요. 기존 보직 효과가 해제되며 영웅이 방어 위치로 이동합니다."),
 G(RestoreDuty,"보직으로 복귀","영웅 명단에서 방금 수호하던 영웅을 선택하고 시설을 지정해 ‘보직 배치’를 다시 누르세요. 도착 후 보직 효과를 확인하세요.")}},
 {TEXT("강습"),TEXT("강습은 영웅·병력이 적함에 침투해 방을 점령하거나 파괴하는 작전입니다. 활성 쉴드는 접안을 막고 비행 중 포드와 강습선은 요격될 수 있습니다. 함교 점령은 함선의 소속까지 바꿀 수 있습니다."),{
 G(BoardTarget,"A팀 강습선과 적함 선택","아군 구축전함 ‘선내 지도’에서 하단 ‘A팀’ → ‘모두 선택’으로 대원을 선택하고 ‘강습’을 누르세요. 개인 포드는 한 명씩 분산 발사하고 강습선은 팀을 함께 운송합니다. ‘A팀 강습선’을 고르고 붉은 적 구축전함을 클릭하세요."),
 G(Transit,"탑승 확인 뒤 강습선 출발","대원이 격납고로 모여 탑승할 때까지 기다리세요. 준비 알림의 ‘출격 확인’을 누르면 강습선이 출발합니다. 아군이 진입한 뒤에는 수동 사격으로 아군 피해를 피하세요."),
 G(Inside,"적함 내부 진입","적함 쉴드가 내려가야 접안할 수 있습니다. 적 구축전함의 선내 지도를 열고 진입점과 실제 도착한 A팀 대원을 찾으세요."),
 G(Operation,"시설 점령 또는 공작","적함 안의 대원을 선택하고 방을 클릭하세요. ‘점령’은 그 방의 소유권을 바꾸며 함교 점령은 함선을 장악합니다. ‘공작’은 시설을 파괴합니다. 엔진은 추진, 무기 제어실은 사격, 격납고는 항공 운용을 잃게 됩니다."),
 G(Return,"강습팀을 진입점으로 철수","적함 선내 지도에서 생존 A팀 대원을 선택하고 ‘철수’를 누르세요. 진입점으로 돌아가 원래 강습선에 승선한 뒤 강습선이 모함 근처에 귀환할 때까지 기다리세요."),
 G(Recover,"구축전함에 강습선 수납","아군 구축전함 선택 → ‘함선 상세’ → ‘격납고’ → 빈 베이의 ‘소형함 수납’을 누르세요. 전장의 A팀 강습선을 클릭하고 모함 격납고에 실제로 수납되는지 확인하세요.")}}
};
#undef G
using Facility = EOVFacilityType;
struct FRoomGoal { FName Id; Facility Type; };
const FRoomGoal RoomGoals[]={
 {"Core",Facility::Core},{"Bridge",Facility::Bridge},{"Engine",Facility::Engine},{"Control",Facility::WeaponControl},
 {"LifeSupport",Facility::LifeSupport},{"Ammo",Facility::Ammo},{"Capacitor",Facility::Capacitor},{"MissileStorage",Facility::MissileStorage},
 {"ShieldControl",Facility::ShieldControl},{"Radar",Facility::Radar},{"Interception",Facility::Interception},{"Boarding",Facility::Boarding},
 {"HangarRoom",Facility::Hangar},{"FighterControl",Facility::FighterControl},{"ResourceStorage",Facility::ResourceStorage},{"Security",Facility::Security}
};
const FRoomGoal* RoomFor(FName key){
 for(const auto& goal:RoomGoals)if(goal.Id==key)return &goal;
 return nullptr;
}
bool OutfitClassroom(AOVShip* ship){
 // Each practice ship owns its calibrated layout. Install sample rooms only in compatible empty slots.
 if(!ship || !ship->Systems || !ship->Systems->Layout)return false;
 auto* layout=DuplicateObject<UShipLayoutDataAsset>(ship->Systems->Layout.Get(),ship->Systems);
 for(const auto& goal:RoomGoals){
    if(layout->Slots.ContainsByPredicate([&](const auto& r){return r.Facility==goal.Type;}))continue;
    auto* slot=layout->Slots.FindByPredicate([&](const auto& r){return r.Facility==Facility::None && r.AllowedFacilities.Contains(goal.Type);});
    if(!slot)return false;
    slot->Facility=goal.Type;slot->MaxFacilityHP=100;slot->RequiredCrew=goal.Type==Facility::Security?0:10;
 }
 ship->Systems->Initialize(layout);
 return ship->Systems->Layout!=nullptr;
}
AOVShip* FriendlyHull(const AOVController* pc,FName hull){
 if(pc)for(AOVShip* s:pc->Ships)if(IsValid(s)&&s->bFriendly&&s->HullDefinitionId==hull)return s;
 return nullptr;
}
FName LessonHull(FName goal){
 if(goal=="Screen"||goal=="Equip"||goal=="Reject")return "corvette";
 if(goal=="Survey")return "radar";
 if(goal=="BeamDemo")return "radar";
 if(goal=="CannonDemo")return "frigate";
 if(goal=="Broadside")return "battleship";
 if(goal=="MissileDemo")return "missile_ship";
 if(goal=="FlightDeck")return "carrier";
 if(goal=="Flagship")return "dreadnought";
 return NAME_None;
}
}
FString UOVPlayScenarioSubsystem::PracticeTitle(int32 c){return c>=0&&c<UE_ARRAY_COUNT(Lessons)?Lessons[c].Title:FString();}
FString UOVPlayScenarioSubsystem::PracticeSummary(int32 c){return c>=0&&c<UE_ARRAY_COUNT(Lessons)?Lessons[c].Summary:FString();}
int32 UOVPlayScenarioSubsystem::CompletedPracticeMask(){int32 mask=0;GConfig->GetInt(TEXT("OrbitalVanguard.Practice"),TEXT("Completed"),mask,GGameUserSettingsIni);return mask;}
int32 UOVPlayScenarioSubsystem::PracticeStepCount()const{return PracticeChapter>=0&&PracticeChapter<9?Lessons[PracticeChapter].Goals.Num():0;}
FName UOVPlayScenarioSubsystem::PracticeGoalId()const{return PracticeStep>=0&&PracticeStep<PracticeStepCount()?Lessons[PracticeChapter].Goals[PracticeStep].Id:NAME_None;}
FString UOVPlayScenarioSubsystem::PracticeObjective()const{return PracticeStep<PracticeStepCount()?Lessons[PracticeChapter].Goals[PracticeStep].Label:TEXT("장 완료");}
FString UOVPlayScenarioSubsystem::PracticeInput()const{return PracticeStep<PracticeStepCount()?Lessons[PracticeChapter].Goals[PracticeStep].Input:TEXT("다른 장을 고르거나 다시 실습할 수 있습니다.");}
bool UOVPlayScenarioSubsystem::HasPracticeMarker()const{
 if(PracticeStep>=PracticeStepCount())return false;
 auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;return key=="Plane"||key=="Height"||key=="Escort"||key=="Screen";
}
bool UOVPlayScenarioSubsystem::StartPractice(AOVController* pc,int32 chapter)
{
 if(!pc||chapter<0||chapter>=UE_ARRAY_COUNT(Lessons))return false;
 TGuardValue<bool> preparing(bPreparingBoardingLesson,chapter==8);
 if(!Start(*pc,chapter==8?TEXT("HeroArena"):TEXT("Practice")))return false;
 PracticeChapter=chapter; PracticeStep=0;PracticeParticipant=INDEX_NONE;PracticeStockBefore=0;PracticeWing.Reset(); bPracticeDetail=false;PracticeRoleCard=0;
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
    const FName hull=(chapter==1||chapter==6||chapter==7)?TEXT("carrier"):chapter==3?TEXT("battleship"):chapter==4?TEXT("air_defense"):chapter==5?TEXT("battlecarrier"):TEXT("corvette");
    PracticeShip=add(hull,0,FVector(0,0,2000),4);
    if(chapter==0||chapter==4) {if(!add(chapter==0?TEXT("frigate"):TEXT("battleship"),0,FVector(-9000,9500,2000))){pc->LeavePlayScenario();return false;}}
    if(chapter==2||chapter==3)PracticeEnemy=add(TEXT("frigate"),1,FVector(21500,0,2000));
    if(chapter==3 && (!add(TEXT("air_defense"),0,FVector(4000,7000,2000)) || !add(TEXT("carrier"),1,FVector(24500,10000,2000),3))){pc->LeavePlayScenario();return false;}
    if(!PracticeShip.IsValid()||((chapter==2||chapter==3)&&!PracticeEnemy.IsValid())){pc->LeavePlayScenario();return false;}
    if(chapter==4){
       if(!add("corvette",0,{-14500,-11000,2000}) || !add("radar",0,{-4000,-19000,2000}) ||
          !add("carrier",0,{-11000,20000,2000},8) || !add("dreadnought",0,{-22000,4000,2000})){
           pc->LeavePlayScenario();return false;
       }
       PracticeEnemy=add("dreadnought",1,{28000,-3500,4300});
    }
    if(chapter==5){
       if(!OutfitClassroom(PracticeShip.Get()) || !add("frigate",0,{12000,14000,2000}) ||
          !add("missile_ship",0,{12000,-14000,2000}) || !add("corvette",0,{-14000,15000,2000}) || !add("radar",0,{16000,0,22000})){
           pc->LeavePlayScenario();return false;
       }
       PracticeEnemy=add("dreadnought",1,{42000,2000,5100});
    }
    if((chapter==4||chapter==5)&&!PracticeEnemy.IsValid()){pc->LeavePlayScenario();return false;}
    if(chapter==6){
       auto* tender=add("repair",0,{-6800,6500,2000});
       if(!tender||!GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()->SetDevMaterials(tender,FVector(70,10,0))){pc->LeavePlayScenario();return false;}
    }
    if(chapter==4)for(AOVShip* trial:pc->Ships)if(trial&&trial->bFriendly)PracticeRoleShips.Add(trial->HullDefinitionId,trial);
    if(chapter==1||chapter==7)AssignScenarioHeroes(*pc,0);
 }
 pc->SelectShip(PracticeShip.Get());pc->FrameEngagement();pc->bFollow=false;
 if(chapter==0||chapter==1||chapter==4)pc->ClearSelection();
 pc->bSimulationPaused=false;
 BeginPracticeStep();return true;
}
bool UOVPlayScenarioSubsystem::PilotPracticeRole(){
 auto* pc=Owner.Get();
 if(!pc||PracticeChapter!=4||!bPracticeDetail||!bPracticeHints)return false;
 static const TArray<FName> hulls=OVCalibration::GetHullIds();
 if(!hulls.IsValidIndex(PracticeRoleCard))return false;
 const FName hull=hulls[PracticeRoleCard];
 AOVShip* pilot=nullptr;
 if(const auto* previous=PracticeRoleShips.Find(hull)){
    pilot=previous->Get();
    if(!IsValid(pilot)||!pilot->bFriendly||pilot->Systems->bWrecked||pilot->Systems->bNeutral||pc->Combat->IsDeparted(pilot)){
       pc->LastAction=TEXT("이 실습 함선은 조종할 수 없습니다. 장을 다시 시작하면 복구됩니다.");return false;
    }
 }else{
    // Keep the original escort, its marker and the firing lanes clear. Every candidate uses the same hull clearance as the sandbox.
    const double radius=OVCalibration::GetHullWorldHalfExtent(hull).Size();
    TOptional<FVector> freeStation;
    for(int32 row=0;row<5&&!freeStation.IsSet();++row){
       for(int32 lane=0;lane<7;++lane){
           const int32 side=(lane+1)/2*(lane%2?-1:1);
           const FVector site(-46000-row*15000,side*15000,2000);
           if(UOVDevelopmentSubsystem::HasPlacementClearance(*pc,site,radius)){freeStation=site;break;}
       }
    }
    if(!freeStation.IsSet()){pc->LastAction=TEXT("실습 함선 배치 공간이 막혔습니다. 아군 후방의 함선을 옮겨 주세요.");return false;}
    pilot=Spawn(*pc,{hull,1,8,2,2},0,pc->Ships.Num(),freeStation);
    if(!pilot){pc->LastAction=TEXT("실습 함선을 준비할 수 없습니다.");return false;}
    pilot->SetAutomation(false,false);pilot->StopMovement();
    PracticeRoleShips.Add(hull,pilot);
 }
 pc->SelectShip(pilot);pc->CameraFocus=pilot->GetActorLocation();pc->UpdateCamera();
 return true;
}
void UOVPlayScenarioSubsystem::BeginPracticeStep(){
 auto* pc=Owner.Get();if(!pc||!PracticeShip.IsValid()||PracticeStep>=PracticeStepCount())return;
 bPracticeReady=false;bPracticeOrder=false;PracticeCamera=pc->CameraFocus;PracticeAngle=pc->CameraYaw;PracticePitch=pc->CameraPitch;PracticeZoom=pc->CameraDistance;
 PracticeOrigin=PracticeShip->GetActorLocation();PracticeShots=pc->Combat->ShotCount;
 auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;
 if(key=="Plane"||key=="Height")PracticeGoal=PracticeOrigin+FVector(7000,4500,key=="Height"?4000:0);
 if(key=="Escort"&&pc->Ships.Num()>1)PracticeGoal=pc->Ships[1]->GetActorLocation()+FVector(10500,0,0);
 PracticeStepTime=pc->Combat->GameTime;
 if(key=="Screen")PracticeGoal=FVector(12000,-9500,2000);
 if(key=="Strike"&&(!PracticeEnemy.IsValid()||PracticeEnemy->Systems->bWrecked)){
    // A disabled training target lets first-time players see a fighter kill without a long attrition fight.
    PracticeEnemy=Spawn(*pc,{"corvette",1,0},1,pc->Ships.Num(),FVector(32000,0,4400));
    if(auto* target=PracticeEnemy.Get()){
       target->ShipName=TEXT("손상된 적 표적함");target->SetAutomation(false,false);target->StopMovement();
       target->Systems->SetShieldManualOff(true);target->Systems->HullHP=1;
       for(auto& tile:target->Systems->ArmorSections)tile.HP=0;
    }
 }
 if(key=="AirCover"&&(!PracticeEnemy.IsValid()||PracticeEnemy->HullDefinitionId!="carrier"||PracticeEnemy->Systems->bWrecked)){
    PracticeEnemy=Spawn(*pc,{"carrier",1,3,2,0},1,pc->Ships.Num(),FVector(48000,-16000,2100));
    if(auto* opponent=PracticeEnemy.Get()){
       opponent->SetAutomation(false,false);opponent->StopMovement();
    }
 }
 if(key=="AutoBatch"){
    if(const auto* state=pc->Carrier->Carriers.FindByPredicate([&](const auto& c){return c.Carrier==PracticeShip;}))PracticeLaunchesBefore=state->LaunchBatches;
 }
 if(key=="Supply"){
    PracticeStockBefore=pc->Carrier->GetAircraftOverview(PracticeShip.Get()).ReplacementKits;
    PracticeInventoryBefore=pc->Carrier->Aircraft.Num();
 }
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
    AOVShip* mover=k=="Screen"?FriendlyHull(pc,"corvette"):ship;
    if(!mover)return;
    const FVector target=mover->Destination;
    if(mover->bHasDestination && FVector::Dist2D(target,PracticeGoal)<2000 && FMath::Abs(target.Z-PracticeGoal.Z)<1100)bPracticeOrder=true;
    ok=bPracticeOrder && FVector::Dist(mover->GetActorLocation(),PracticeGoal)<2100 && !mover->bHasDestination;
 }else if(k=="Pause")ok=pc->bSimulationPaused;
 else if(k=="Speed")ok=!pc->bSimulationPaused&&pc->SimulationSpeed==2;
 else if(k=="Details")ok=hud->bDetailsOpen&&pc->Selected==ship;
 else if(k=="Room"||RoomFor(k)){
    int i=ship->Systems->Layout->GetSlotIndex(hud->InteriorTargetSlot);
    if(hud->bInteriorOpen&&hud->InteriorShip==ship&&ship->Systems->Layout->Slots.IsValidIndex(i)){
       auto f=ship->Systems->Layout->Slots[i].Facility;
       ok=k=="Room"||f==RoomFor(k)->Type;
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
 else if(k=="Survey")ok=enemy&&!enemy->Survey.IsEmpty();
 else if(k=="Broadside"||k=="CannonDemo"||k=="BeamDemo"||k=="MissileDemo"){
    if(auto* gunship=FriendlyHull(pc,LessonHull(k));gunship&&gunship->AttackTarget==enemy){
       const EOVWeaponType family=k=="BeamDemo"?EOVWeaponType::Beam:k=="MissileDemo"?EOVWeaponType::Missile:EOVWeaponType::Cannon;
       for(const auto& mount:pc->Combat->GetShipState(gunship).Mounts)
           if(mount.Profile.Family==family&&mount.LastShotTime>=PracticeStepTime&&mount.LastShotId>PracticeShots){ok=true;break;}
    }
 }else if(k=="FlightDeck"||k=="Strike"){
    auto* carrier=k=="FlightDeck"?FriendlyHull(pc,"carrier"):ship;
    ok=enemy&&pc->Carrier->Aircraft.ContainsByPredicate([&](const auto& a){return a.Carrier.Get()==carrier&&a.ShotsFired>0;});
    if(k=="Strike")ok=ok&&enemy->Systems->bWrecked;
 }else if(k=="Flagship")ok=pc->Selected==FriendlyHull(pc,"dreadnought")&&hud->bDetailsOpen;
 else if(k=="AirCover"||k=="AutoBatch"){
    const auto* state=pc->Carrier->Carriers.FindByPredicate([ship](const auto& c){return c.Carrier.Get()==ship;});
    if(k=="AirCover"&&state&&state->bAirSuperiority&&enemy){
       if(!bPracticeOrder){
          pc->Combat->SetTarget(enemy,ship);
          bPracticeOrder=pc->Carrier->Launch(enemy)>0;
       }
       ok=pc->Carrier->Aircraft.ContainsByPredicate([&](const auto& a){return a.Carrier.Get()==ship&&a.InterceptTargetId>0;});
    }else if(k=="AutoBatch")ok=state&&state->bAutoLaunch&&!state->bRecallOrdered&&state->LaunchBatches>PracticeLaunchesBefore;
 }else if(k=="Supply")ok=pc->Carrier->GetAircraftOverview(ship).ReplacementKits>PracticeStockBefore;
 else if(k=="Production"){
    for(int32 i=PracticeInventoryBefore;i<pc->Carrier->Aircraft.Num();++i){
       const auto& craft=pc->Carrier->Aircraft[i];if(craft.Carrier.Get()==ship&&craft.State==EOVAircraftState::Stored){ok=true;break;}
    }
 }else if(k=="Launch"||k=="Recall"||k=="Docked"){
    int returned=0;for(const auto& a:pc->Carrier->Aircraft){
      if(a.FactionId!=0)continue;
      if(k=="Launch"&&a.State==EOVAircraftState::Sortie)PracticeWing.Add(a.AircraftId);
      if(!PracticeWing.Contains(a.AircraftId))continue;
      if(a.State==EOVAircraftState::Returning||a.State==EOVAircraftState::DockWaiting||a.State==EOVAircraftState::Stored)ok=true;
      returned+=a.State==EOVAircraftState::Stored?1:0;
    }
    if(k=="Launch")ok=PracticeWing.Num()>0;
    if(k=="Docked")ok=PracticeWing.Num()>0&&returned==PracticeWing.Num();
 }else if(k=="Locate"){
    if(hud->bInteriorOpen&&hud->InteriorShip==ship){
       for(int32 id:hud->InteriorSelectedUnits){
           const auto u=pc->Interior->GetUnit(id);
           if(u.HeroId.IsValid()&&u.FactionId==0&&u.Ship.Get()==ship){PracticeParticipant=id;ok=true;break;}
       }
    }
 }else if(k=="Duty"||k=="Guard"||k=="RestoreDuty"){
    for(const auto& u:GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->Units){
       if(!u.HeroId.IsValid()||u.FactionId!=0||u.Ship.Get()!=ship)continue;
       if(u.UnitId!=PracticeParticipant)continue;
       if(k=="Guard"){
          const int32 i=ship->Systems->Layout->GetSlotIndex(u.NodeId);
          ok=!u.bDutyActive&&u.Order==EOVInteriorOrder::Defend&&ship->Systems->Layout->Slots.IsValidIndex(i)&&ship->Systems->Layout->Slots[i].Facility==Facility::Bridge;
       }else ok=u.bDutyActive;
       break;
    }
 }else if(k=="BoardTarget"){
    auto isTeamA=[&](const TArray<int32>& ids){
       if(ids.IsEmpty())return false;
       int32 count=0;
       for(int32 group:ArenaGroups[0])if(const auto* members=ArenaMembers.Find(group)){
           for(int32 member:*members){if(!ids.Contains(member))return false;++count;}
       }
       return count>0&&count==ids.Num();
    };
    ok=pc->CrewOperations.ContainsByPredicate([&](const auto& op){return !op.bTransfer&&op.Origin.Get()==ship&&op.Target.Get()==enemy&&op.Shuttle.IsValid()&&op.Shuttle==ArenaWaves[0].Shuttle&&isTeamA(op.Units);});
    ok|=ArenaWaves[0].Sent&&isTeamA(ArenaWaves[0].Units)&&ArenaWaves[0].ActualShuttle.IsValid()&&ArenaWaves[0].ActualShuttle==ArenaWaves[0].Shuttle;
 }else if(k=="Recover"){
    auto* shuttle=ArenaWaves[0].ActualShuttle.Get();
    ok=shuttle && shuttle->HangarHost.Get()==ship && pc->Carrier->Hangars.ContainsByPredicate([&](const auto& h){return h.Carrier.Get()==ship&&h.StoredSmallShip.Get()==shuttle;});
 }
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
          bool home=(person.State==EOVPassengerState::Aboard || person.State==EOVPassengerState::InShuttle) &&
                    (person.CurrentShip.Get()==ship || (ArenaWaves[0].ActualShuttle.IsValid() && person.CurrentShip==ArenaWaves[0].ActualShuttle));
          recovered|=home||person.State==EOVPassengerState::RescueWaiting;
          pending|=!home && person.State!=EOVPassengerState::RescueWaiting && person.State!=EOVPassengerState::Dead;
       }
       auto* shuttle=ArenaWaves[0].ActualShuttle.Get();
       const auto* flight=pc->Transport->Shuttles.FindByPredicate([shuttle](const auto& s){return s.Ship.Get()==shuttle;});
       ok=recovered&&!pending&&flight&&flight->State==EOVShuttleState::Ready;
    }
    if(k=="Operation"&&enemy)for(const auto& op:GetWorld()->GetSubsystem<UOVInteriorSubsystem>()->Operations)ok|=op.Ship.Get()==enemy && op.FactionId==0 && op.bCompleted;
 }
 bPracticeReady=ok;
}
void UOVPlayScenarioSubsystem::ConfirmPracticeNotice(FName subject){
 if(PracticeChapter==1 && PracticeStep==5 && PracticeShip.IsValid() && PracticeShip->GetFName()==subject)bPracticeOrder=true;
}
bool UOVPlayScenarioSubsystem::PracticeEquipment(bool invalid){
 auto* pc=Owner.Get();auto* ship=FriendlyHull(pc,"corvette");
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
 if(const auto* goal=RoomFor(key)){
    const auto* pc=Owner.Get();
    auto* hud=pc?Cast<AOVHUD>(pc->GetHUD()):nullptr;
    auto* ship=PracticeShip.Get();
    if(!ship||!hud||!hud->bInteriorOpen||hud->InteriorShip!=ship)return "InteriorToggle";
    const auto* room=ship->Systems->Layout->Slots.FindByPredicate([&](const auto& r){return r.Facility==goal->Type;});
    if(room)return room->DeckId!=hud->InteriorDeck?FName(*FString::Printf(TEXT("InteriorDeck_%d"),room->DeckId)):FName(*(TEXT("InteriorSlot_")+room->SlotId.ToString()));
 }
 static const TMap<FName,FName> Controls={{"Details","Details"},{"HeroPanel","CommandHeroPanel"},{"Feed","BattleFeedToggle"},{"Hangar","HangarView"},{"Move","Move"},{"Hold","Stop"},{"Target","Target"},{"Contact","Target"},{"Fire","FireAll"},{"AutoFire","FireAuto"},{"Cease","CancelAttack"},{"Pause","Pause"},{"Speed","Speed2"},{"Launch","HangarLaunchAircraft"},{"Recall","RecallAircraft"},{"Survey","RadarSurvey"},{"FlightDeck","LaunchAircraft"},{"Broadside","FireAll"},{"CannonDemo","FireAll"},{"MissileDemo","FireAll"},{"BeamDemo","FireAll"},{"AirCover","WingAirCombat"},{"Supply","WingSupply"},{"AutoBatch","AutoWing"},{"Guard","InteriorDefend"},{"Return","InteriorRetreat"},{"Equip","PracticeEquip"},{"Reject","PracticeEquip"},{"BoardTarget","OpsAssault"}};
 return Controls.FindRef(key);
}
AOVShip* UOVPlayScenarioSubsystem::PracticeFocus() const{
 if(PracticeChapter<0||PracticeStep>=PracticeStepCount())return nullptr;
 const auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;
 if(const auto hull=LessonHull(key);!hull.IsNone())return FriendlyHull(Owner.Get(),hull);
 if(key=="Single"||key=="Multi"||RoomFor(key))return PracticeShip.Get();
 return key=="Contact"||key=="Target"||key=="BoardTarget"?PracticeEnemy.Get():nullptr;
}

void UOVPlayScenarioSubsystem::RetryPracticeStep(){
 if(PracticeChapter<0||PracticeStep>=PracticeStepCount()||!Owner.IsValid())return;
 // Boarding can lose the transport; a repeat needs a fresh isolated team and ship.
 if(PracticeChapter==8 || !PracticeShip.IsValid() || PracticeShip->Systems->bWrecked || (PracticeChapter!=6 && PracticeEnemy.IsValid() && PracticeEnemy->Systems->bWrecked)){
    StartPractice(Owner.Get(),PracticeChapter);return;
 }
 const auto key=Lessons[PracticeChapter].Goals[PracticeStep].Id;
 if(HasPracticeMarker()){if(auto* mover=key=="Screen"?FriendlyHull(Owner.Get(),"corvette"):PracticeShip.Get())mover->StopMovement();}
 if(key=="Single"||key=="Multi")Owner->ClearSelection();
 BeginPracticeStep();
}
