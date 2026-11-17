#include "OVDevelopmentWidget.h"
#include "OVCalibration.h"
#include "OVDevelopmentSubsystem.h"
#include "OVDevelopmentCatalogue.h"
#include "OVShipSystemsComponent.h"
#include "OVShipLayout.h"
#include "OVCarrierSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVCombatSubsystem.h"
#include "OVLogisticsSubsystem.h"
#include "OVHUD.h"
#include "OVPointDefenseSubsystem.h"
#include "OVHeroSubsystem.h"
#include "OVController.h"
#include "OVShip.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace {
UTextBlock* Label(UWidgetTree* T,const FString& S) { auto* W=T->ConstructWidget<UTextBlock>();W->SetText(FText::FromString(S));W->SetAutoWrapText(true);return W; }
UButton* AddButton(UWidgetTree* T,UPanelWidget* P,const TCHAR* S) { auto* B=T->ConstructWidget<UButton>();B->AddChild(Label(T,S));P->AddChild(B);return B; }
const TCHAR* AircraftState(EOVAircraftState State){const TCHAR* V[]={TEXT("보관"),TEXT("출격"),TEXT("귀환"),TEXT("착함 대기"),TEXT("관제 상실"),TEXT("파괴")};return V[FMath::Clamp(int32(State),0,5)];}
const TCHAR* PassengerState(EOVPassengerState State){const TCHAR* V[]={TEXT("탑승"),TEXT("포드 비행"),TEXT("목표함 내부"),TEXT("강습선"),TEXT("회수 대기"),TEXT("구조 대기"),TEXT("사망")};return V[FMath::Clamp(int32(State),0,6)];}
const TCHAR* PodState(EOVPodFlightState State){const TCHAR* V[]={TEXT("발사 대기"),TEXT("접근"),TEXT("접촉"),TEXT("귀환"),TEXT("회수"),TEXT("파괴"),TEXT("고립"),TEXT("취소")};return V[FMath::Clamp(int32(State),0,7)];}
FString ReadyLabel(const FString& Key) { static const TMap<FString,FString> L={{TEXT("READY"),TEXT("발사 가능")},{TEXT("NO VALID TARGET"),TEXT("표적 없음")},{TEXT("OUT OF RANGE"),TEXT("사거리 밖")},{TEXT("NO DETECTION"),TEXT("탐지 없음")},{TEXT("MOUNT BLIND ARC"),TEXT("사각")},{TEXT("TURRET ALIGNING"),TEXT("조준 중")},{TEXT("FRIENDLY IN LINE"),TEXT("아군 사선 차단")},{TEXT("COOLDOWN"),TEXT("재사용 대기")},{TEXT("WEAPON CONTROL OFF"),TEXT("무기제어실 정지")},{TEXT("SUPPLY OFF"),TEXT("공급 정지")},{TEXT("MOUNT DESTROYED"),TEXT("무장 파괴")},{TEXT("COMBAT OFF"),TEXT("전투 비활성")},{TEXT("PD OFF"),TEXT("요격 비활성")},{TEXT("SHIP LOST"),TEXT("함선 상실")},{TEXT("NO OWNER"),TEXT("소유 세력 없음")},{TEXT("DEPARTED"),TEXT("함선 이탈")},{TEXT("NO PD MOUNTS"),TEXT("요격 장착점 없음")},{TEXT("PD CONTROL OFFLINE"),TEXT("요격 제어 정지")},{TEXT("PD MOUNTS DISABLED"),TEXT("요격 장착점 정지")},{TEXT("DEFENDING"),TEXT("자동 방어 중")}};if(const auto* V=L.Find(Key))return *V;return TEXT("상태 코드: ")+Key; }
FString Evidence(const TCHAR* Name,const FOVCatalogEvidence& E) { const TCHAR* States[]={TEXT("미확인"),TEXT("있음"),TEXT("부분"),TEXT("없음"),TEXT("해당 없음")}; return FString::Printf(TEXT("%s: %s · %s"),Name,States[FMath::Clamp(int32(E.Status),0,4)],*E.Reason); }
UEditableTextBox* AddInput(UWidgetTree* T,UVerticalBox* P,const TCHAR* Hint,const TCHAR* Default) { P->AddChildToVerticalBox(Label(T,Hint));auto* W=T->ConstructWidget<UEditableTextBox>();W->SetHintText(FText::FromString(Hint));W->SetText(FText::FromString(Default));P->AddChildToVerticalBox(W);return W; }
}
void UOVDevelopmentItem::Pick() { if(Owner)Owner->SelectItem(Id); }
void UOVDevelopmentWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();auto* Border=WidgetTree->ConstructWidget<UBorder>();Border->SetBrushColor(FLinearColor(.008f,.02f,.04f,.985f));WidgetTree->RootWidget=Border;
 auto* Root=WidgetTree->ConstructWidget<UVerticalBox>();Border->AddChild(Root);
 auto* Tabs=WidgetTree->ConstructWidget<UHorizontalBox>();Root->AddChildToVerticalBox(Tabs);
 AddButton(WidgetTree,Tabs,TEXT("통합 현황"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Overview);
 AddButton(WidgetTree,Tabs,TEXT("함선·초소형함"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Hulls);
 AddButton(WidgetTree,Tabs,TEXT("영웅"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Heroes);
 AddButton(WidgetTree,Tabs,TEXT("무장"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Weapons);
 AddButton(WidgetTree,Tabs,TEXT("닫기"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Close);
 auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();auto* ScrollSlot=Root->AddChildToVerticalBox(Scroll);ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 Body=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Body);Rebuild();
}
bool UOVDevelopmentWidget::IsEditing() const { return (Search&&Search->HasKeyboardFocus())||(Quantity&&Quantity->HasKeyboardFocus())||(X&&X->HasKeyboardFocus())||(Y&&Y->HasKeyboardFocus())||(Z&&Z->HasKeyboardFocus())||(Spacing&&Spacing->HasKeyboardFocus())||(Faction&&Faction->HasKeyboardFocus())||(Inventory&&Inventory->HasKeyboardFocus())||(Troops&&Troops->HasKeyboardFocus())||(HeroName&&HeroName->HasKeyboardFocus())||(XPAmount&&XPAmount->HasKeyboardFocus())||(DevAmount&&DevAmount->HasKeyboardFocus())||(DevRange&&DevRange->HasKeyboardFocus())||(DevNode&&DevNode->HasKeyboardFocus())||(DevBasic&&DevBasic->HasKeyboardFocus())||(DevAdvanced&&DevAdvanced->HasKeyboardFocus())||(DevRare&&DevRare->HasKeyboardFocus())||(DevQuoteBasic&&DevQuoteBasic->HasKeyboardFocus())||(DevQuoteAdvanced&&DevQuoteAdvanced->HasKeyboardFocus())||(DevQuoteRare&&DevQuoteRare->HasKeyboardFocus()); }
void UOVDevelopmentWidget::Overview(){Page=0;Rebuild();}void UOVDevelopmentWidget::Hulls(){Page=1;Rebuild();}void UOVDevelopmentWidget::Heroes(){Page=2;Rebuild();}void UOVDevelopmentWidget::Weapons(){Page=3;Rebuild();}
void UOVDevelopmentWidget::Close(){if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(PC->bDeveloperMode)PC->Action(TEXT("Developer"));}
void UOVDevelopmentWidget::SearchChanged(const FText&){SelectedId=NAME_None;if(Detail)Detail->SetText(FText::FromString(TEXT("목록에서 항목을 선택하세요.")));RefreshList();}
void UOVDevelopmentWidget::CategoryChanged(FString, ESelectInfo::Type){if(Page==1){HullCategory=CategoryChoice->GetSelectedIndex();Rebuild();return;}if(Page==0)OverviewCategory=CategoryChoice->GetSelectedIndex();SelectedId=NAME_None;if(Detail)Detail->SetText(FText::FromString(TEXT("목록에서 항목을 선택하세요.")));RefreshList();}
void UOVDevelopmentWidget::SelectItem(FName Id)
{
 SelectedId=Id;if(!Detail)return;
 if(Page==2){FGuid Guid;if(!FGuid::Parse(Id.ToString(),Guid))return;const int32 Index=HeroIds.IndexOfByKey(Guid);if(Index==INDEX_NONE)return;if(HeroChoice&&HeroChoice->GetSelectedIndex()!=Index)HeroChoice->SetSelectedIndex(Index);auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();if(!S)return;const auto H=S->GetHero(Guid);const TCHAR* Roles[]={TEXT("병력"),TEXT("근거리"),TEXT("원거리"),TEXT("지원"),TEXT("지휘")};const TCHAR* States[]={TEXT("예비"),TEXT("편성 예약"),TEXT("구조 대기"),TEXT("포로"),TEXT("실종"),TEXT("사망")};FString Skills;for(int32 I=0;I<H.SkillRanks.Num();++I)Skills+=FString::Printf(TEXT(" %d:%d"),I+1,H.SkillRanks[I]);FString Place=TEXT("전장 연결 없음");if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(PC->Interior&&H.RuntimeUnitId){const auto U=PC->Interior->GetUnit(H.RuntimeUnitId);if(U.UnitId==H.RuntimeUnitId)Place=(U.Ship.IsValid()?U.Ship->ShipName:TEXT("위치 미확인"))+TEXT(" / ")+U.NodeId.ToString();}FString Summary=FString::Printf(TEXT("%s · %s · %s\n개인 ID %s\nLv%d / XP%d / 투자점수%d\n스킬 랭크%s · 촉매%d / HP %.0f\n%s"),*H.Name,Roles[FMath::Clamp(int32(H.Role),0,4)],States[FMath::Clamp(int32(H.Status),0,5)],*Guid.ToString(),H.Level,H.XP,H.SkillPoints,*Skills,H.Catalysts,H.CurrentHP,*Place);for(int32 Skill=0;Skill<4;++Skill)Summary+=TEXT("\n")+S->GetSkillDescription(Guid,Skill);Detail->SetText(FText::FromString(Summary));return;}
 FString Text=TEXT("선택 ID: ")+Id.ToString();if(auto* C=GetWorld()->GetSubsystem<UOVDevelopmentCatalogue>())for(const auto& D:C->GetDefinitions())if(D.DefinitionId==Id){Text=D.DisplayName+TEXT(" · ")+Id.ToString()+TEXT("\n")+D.Description+TEXT("\n")+Evidence(TEXT("기획"),D.Planning)+TEXT("\n")+Evidence(TEXT("데이터"),D.Data)+TEXT("\n")+Evidence(TEXT("생성"),D.Generation)+TEXT("\n")+Evidence(TEXT("역할"),D.Role)+TEXT("\n")+Evidence(TEXT("자동 검증"),D.AutomatedVerification)+TEXT("\n")+Evidence(TEXT("직접 조작"),D.DirectInput)+TEXT("\n원본: ")+D.SourceEvidence;if(!D.AutomatedVerification.VerificationSHA.IsEmpty())Text+=TEXT("\n검증 리비전: ")+D.AutomatedVerification.VerificationSHA;break;}
 if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(PC->Combat&&PC->Combat->Settings)if(const auto* W=PC->Combat->Settings->Weapons.FindByPredicate([Id](const FOVWeaponProfile& P){return P.ProfileId==Id;}))Text+=FString::Printf(TEXT("\n실행 프로필: T%d / 피해 %.1f / 사거리 %.0f cm / 주기 %.2f초 / 속도 %.0f cm/s / 일제 %d"),W->Tier,W->Damage,W->Range,W->Interval,W->Speed,W->Salvo);
 Detail->SetText(FText::FromString(Text));
}
void UOVDevelopmentWidget::HeroChanged(FString,ESelectInfo::Type){const FGuid Id=HeroId();if(Id.IsValid())SelectItem(FName(*Id.ToString()));}
void UOVDevelopmentWidget::MountChanged(FString,ESelectInfo::Type){RefreshRuntime();}

FGuid UOVDevelopmentWidget::HeroId() const { const int32 I=HeroChoice?HeroChoice->GetSelectedIndex():INDEX_NONE;return HeroIds.IsValidIndex(I)?HeroIds[I]:FGuid(); }
void UOVDevelopmentWidget::Rebuild()
{
 Body->ClearChildren();Search=nullptr;List=nullptr;Detail=nullptr;Feedback=nullptr;RuntimeDetail=nullptr;RuntimeShips=nullptr;LogisticsTarget=nullptr;DevAmount=nullptr;DevRange=nullptr;DevNode=nullptr;DevBasic=nullptr;DevAdvanced=nullptr;DevRare=nullptr;DevQuoteBasic=nullptr;DevQuoteAdvanced=nullptr;DevQuoteRare=nullptr;HeroChoice=nullptr;RoleChoice=nullptr;CategoryChoice=nullptr;HeroName=nullptr;XPAmount=nullptr;Quantity=nullptr;X=nullptr;Y=nullptr;Z=nullptr;Spacing=nullptr;Faction=nullptr;Inventory=nullptr;Troops=nullptr;MoveAuto=nullptr;FireAuto=nullptr;MountChoice=nullptr;TargetChoice=nullptr;SelectedId=NAME_None;
 auto* PC=Cast<AOVController>(GetOwningPlayer());
 if(Page==0)Body->AddChildToVerticalBox(Label(WidgetTree,FString::Printf(TEXT("개발 도구 · 현재 함선 %d척\n목록을 선택해 근거와 상태를 확인하세요."),PC?PC->Ships.Num():0)));
 if(Page==0||Page==1||Page==3){
  Search=AddInput(WidgetTree,Body,TEXT("이름 또는 ID 검색"),TEXT(""));Search->OnTextChanged.AddDynamic(this,&UOVDevelopmentWidget::SearchChanged);
  if(Page==0||Page==1){CategoryChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(CategoryChoice);if(Page==0)CategoryChoice->AddOption(TEXT("전체"));CategoryChoice->AddOption(TEXT("함선"));CategoryChoice->AddOption(TEXT("초소형 함재기"));CategoryChoice->AddOption(TEXT("포드"));if(Page==0){CategoryChoice->AddOption(TEXT("무장"));CategoryChoice->AddOption(TEXT("영웅"));CategoryChoice->AddOption(TEXT("요격 무장"));}CategoryChoice->SetSelectedIndex(Page==0?OverviewCategory:HullCategory);CategoryChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::CategoryChanged);}
  List=WidgetTree->ConstructWidget<UScrollBox>();auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(150);Size->AddChild(List);Body->AddChildToVerticalBox(Size);
  Detail=Label(WidgetTree,TEXT("목록에서 항목을 선택하세요."));Body->AddChildToVerticalBox(Detail);RefreshList();
  if(Page==0)return;
  if(Page==1&&HullCategory!=0){
   RuntimeDetail=Label(WidgetTree,TEXT(""));Body->AddChildToVerticalBox(RuntimeDetail);
   AddButton(WidgetTree,Body,TEXT("현재 상태 새로고침"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::RefreshRuntime);
   if(HullCategory==1){AddButton(WidgetTree,Body,TEXT("선택 모함의 기체 출격"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::LaunchAircraft);AddButton(WidgetTree,Body,TEXT("선택 모함의 기체 회수"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::RecallAircraft);}
   else{AddButton(WidgetTree,Body,TEXT("선택 함선의 표적에 포드 출격"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::LaunchPods);AddButton(WidgetTree,Body,TEXT("선택 출발함의 포드 회항"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::RecallPods);}
   Feedback=Label(WidgetTree,TEXT("기존 인원과 재고를 사용합니다. 표적은 선택 함선의 현재 표적입니다."));Body->AddChildToVerticalBox(Feedback);RefreshRuntime();return;
  }
  if(Page==1){
   Body->AddChildToVerticalBox(Label(WidgetTree,TEXT("실행 중인 함선 (개별 ID)")));RuntimeShips=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(RuntimeShips);LogisticsTarget=WidgetTree->ConstructWidget<UComboBoxString>();RefreshShips();
   AddButton(WidgetTree,Body,TEXT("목록의 함선 선택"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SelectRuntimeShip);
   AddButton(WidgetTree,Body,TEXT("목록의 함선 추적"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::FollowRuntimeShip);
   AddButton(WidgetTree,Body,TEXT("목록의 함선 선내 지도"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::InteriorRuntimeShip);
   Quantity=AddInput(WidgetTree,Body,TEXT("수량"),TEXT("1"));X=AddInput(WidgetTree,Body,TEXT("X (cm)"),TEXT("0"));Y=AddInput(WidgetTree,Body,TEXT("Y (cm)"),TEXT("0"));Z=AddInput(WidgetTree,Body,TEXT("Z (cm)"),TEXT("0"));Spacing=AddInput(WidgetTree,Body,TEXT("간격 (cm)"),TEXT("1000"));Faction=AddInput(WidgetTree,Body,TEXT("소속 (0 또는 1)"),TEXT("0"));Inventory=AddInput(WidgetTree,Body,TEXT("격납고당 기체 재고"),TEXT("0"));Troops=AddInput(WidgetTree,Body,TEXT("병력"),TEXT("0"));
   auto* Toggles=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Toggles);Toggles->AddChild(Label(WidgetTree,TEXT("이동 자동")));MoveAuto=WidgetTree->ConstructWidget<UCheckBox>();Toggles->AddChild(MoveAuto);Toggles->AddChild(Label(WidgetTree,TEXT("사격 자동")));FireAuto=WidgetTree->ConstructWidget<UCheckBox>();Toggles->AddChild(FireAuto);
   AddButton(WidgetTree,Body,TEXT("선택 함종 생성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Spawn);
   AddButton(WidgetTree,Body,TEXT("시험 함선·격납 시험 재고 폐기 (무료 반환 없음)"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Delete);
   Body->AddChildToVerticalBox(Label(WidgetTree,TEXT("DEV 유한 자원 작업 · 출발함은 위 실행 함선, 대상은 아래 선택")));Body->AddChildToVerticalBox(LogisticsTarget);
   DevAmount=AddInput(WidgetTree,Body,TEXT("요청량 / 시험 자원 노드량"),TEXT("100"));DevRange=AddInput(WidgetTree,Body,TEXT("작업 거리 (cm)"),TEXT("12000"));DevNode=AddInput(WidgetTree,Body,TEXT("시험 자원 노드 ID"),TEXT("1"));
   DevBasic=AddInput(WidgetTree,Body,TEXT("DEV 일반 재고"),TEXT("0"));DevAdvanced=AddInput(WidgetTree,Body,TEXT("DEV 고급 재고"),TEXT("0"));DevRare=AddInput(WidgetTree,Body,TEXT("DEV 최고급 재고"),TEXT("0"));
   DevQuoteBasic=AddInput(WidgetTree,Body,TEXT("DEV HP당 기본 견적: 일반"),TEXT("1"));DevQuoteAdvanced=AddInput(WidgetTree,Body,TEXT("DEV HP당 기본 견적: 고급"),TEXT("0"));DevQuoteRare=AddInput(WidgetTree,Body,TEXT("DEV HP당 기본 견적: 최고급"),TEXT("0"));
   AddButton(WidgetTree,Body,TEXT("DEV 재고 설정"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SetMaterials);AddButton(WidgetTree,Body,TEXT("DEV 현재 위치에 유한 자원 노드 생성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::AddNode);
   AddButton(WidgetTree,Body,TEXT("DEV 채굴"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Mine);AddButton(WidgetTree,Body,TEXT("DEV 재고 수송"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Transfer);AddButton(WidgetTree,Body,TEXT("DEV 견적으로 수리 시작"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Repair);
   RuntimeDetail=Label(WidgetTree,TEXT(""));Body->AddChildToVerticalBox(RuntimeDetail);AddButton(WidgetTree,Body,TEXT("DEV 재고·작업 상태 새로고침"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::RefreshRuntime);
  } else {
   WeaponSubject=PC?PC->Selected.Get():nullptr;
   Body->AddChildToVerticalBox(Label(WidgetTree,TEXT("선택 함선: ")+(PC&&PC->Selected?PC->Selected->ShipName:TEXT("없음"))+TEXT(" · 함선을 바꾸면 무장 탭을 다시 여세요.")));
   MountChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(MountChoice);MountIds.Reset();
   if(PC&&PC->Selected&&PC->Selected->Systems&&PC->Selected->Systems->Layout)for(const auto& M:PC->Selected->Systems->Layout->WeaponMounts){MountIds.Add(M.WeaponId);MountChoice->AddOption(M.WeaponId.ToString());}if(PC&&PC->PointDefense&&WeaponSubject.IsValid())for(const auto& M:PC->PointDefense->GetShipState(WeaponSubject.Get()).Mounts)if(!MountIds.Contains(M.MountId)){MountIds.Add(M.MountId);MountChoice->AddOption(M.MountId.ToString());}if(MountIds.Num())MountChoice->SetSelectedIndex(0);MountChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::MountChanged);
   RuntimeDetail=Label(WidgetTree,TEXT(""));Body->AddChildToVerticalBox(RuntimeDetail);AddButton(WidgetTree,Body,TEXT("장착 상태 새로고침"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::RefreshRuntime);
   TargetChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(TargetChoice);Targets.Reset();if(PC)for(const auto& Ship:PC->Ships)if(IsValid(Ship)){Targets.Add(Ship.Get());TargetChoice->AddOption(Ship->ShipName+FString::Printf(TEXT(" [%d]"),Targets.Num()));}if(Targets.Num())TargetChoice->SetSelectedIndex(0);
   AddButton(WidgetTree,Body,TEXT("선택 무장 장착"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Equip);
   AddButton(WidgetTree,Body,TEXT("선택 함선 표적 지정"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Target);
   AddButton(WidgetTree,Body,TEXT("선택 장착점 발사"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Fire);
  }
  Feedback=Label(WidgetTree,TEXT("항목을 선택하세요. 실행 결과와 제한 사유가 여기에 표시됩니다."));Body->AddChildToVerticalBox(Feedback);if(Page==3||(Page==1&&HullCategory==0))RefreshRuntime();return;
 }
 auto* S=GetGameInstance()?GetGameInstance()->GetSubsystem<UOVHeroSubsystem>():nullptr;
 Search=AddInput(WidgetTree,Body,TEXT("영웅 이름 또는 ID 검색"),TEXT(""));Search->OnTextChanged.AddDynamic(this,&UOVDevelopmentWidget::SearchChanged);
 List=WidgetTree->ConstructWidget<UScrollBox>();auto* HeroListSize=WidgetTree->ConstructWidget<USizeBox>();HeroListSize->SetHeightOverride(120);HeroListSize->AddChild(List);Body->AddChildToVerticalBox(HeroListSize);
 HeroChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(HeroChoice);HeroIds.Reset();if(S)for(const auto& H:S->GetHeroes(0)){HeroIds.Add(H.HeroId);HeroChoice->AddOption(H.Name+TEXT(" · ")+H.HeroId.ToString(EGuidFormats::Short));}if(HeroIds.Num())HeroChoice->SetSelectedIndex(0);HeroChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::HeroChanged);
 Detail=Label(WidgetTree,TEXT("명단에서 영웅을 선택하세요."));Body->AddChildToVerticalBox(Detail);RefreshList();if(HeroIds.Num())SelectItem(FName(*HeroIds[0].ToString()));
 RoleChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(RoleChoice);RoleChoice->AddOption(TEXT("근거리"));RoleChoice->AddOption(TEXT("원거리"));RoleChoice->AddOption(TEXT("지원"));RoleChoice->AddOption(TEXT("지휘"));RoleChoice->SetSelectedIndex(0);HeroName=AddInput(WidgetTree,Body,TEXT("영웅 이름"),TEXT("시험 영웅"));
 AddButton(WidgetTree,Body,TEXT("영웅 생성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::CreateHero);
 AddButton(WidgetTree,Body,TEXT("대표 시험 영웅 4명 생성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::CreateFour);
 XPAmount=AddInput(WidgetTree,Body,TEXT("시험 XP"),TEXT("120"));AddButton(WidgetTree,Body,TEXT("선택 영웅에 시험 XP 지급"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::GrantXP);
 AddButton(WidgetTree,Body,TEXT("편성 예약"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::DeployHero);
 AddButton(WidgetTree,Body,TEXT("편성 해제"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::ReleaseHero);
 AddButton(WidgetTree,Body,TEXT("선택 함선에 실제 배치"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SpawnHero);
 auto* Skills=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Skills);
 AddButton(WidgetTree,Skills,TEXT("스킬 1"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Skill0);AddButton(WidgetTree,Skills,TEXT("스킬 2"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Skill1);AddButton(WidgetTree,Skills,TEXT("스킬 3"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Skill2);AddButton(WidgetTree,Skills,TEXT("스킬 4"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Skill3);
 AddButton(WidgetTree,Body,TEXT("명단 저장"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Save);AddButton(WidgetTree,Body,TEXT("명단 불러오기"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Load);
 Feedback=Label(WidgetTree,TEXT("시험 XP 및 편제 변경은 비전투 상태에서 사용하세요."));Body->AddChildToVerticalBox(Feedback);
}
void UOVDevelopmentWidget::RefreshList()
{
 if(!List||!GetWorld())return;List->ClearChildren();const FString Filter=Search?Search->GetText().ToString():FString();
 auto Add=[&](FName Id,const FString& Name){auto* Item=WidgetTree->ConstructWidget<UOVDevelopmentItem>();Item->Owner=this;Item->Id=Id;Item->AddChild(Label(WidgetTree,Name));Item->OnClicked.AddDynamic(Item,&UOVDevelopmentItem::Pick);List->AddChild(Item);};
 if(Page==2){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>())for(const auto& H:S->GetHeroes(0)){const FString Id=H.HeroId.ToString();if(H.Name.Contains(Filter)||Id.Contains(Filter))Add(FName(*Id),H.Name+TEXT(" · ")+Id);}return;}
 auto* C=GetWorld()->GetSubsystem<UOVDevelopmentCatalogue>();if(!C)return;const int32 Category=CategoryChoice?CategoryChoice->GetSelectedIndex():0;
 for(const auto& D:C->GetDefinitions()){
  bool Match=false;if(Page==3)Match=D.Category==EOVCatalogCategory::Weapon||D.Category==EOVCatalogCategory::PointDefense;else if(Page==1)Match=Category==0?D.Category==EOVCatalogCategory::Hull:Category==1?D.Category==EOVCatalogCategory::Fighter:D.Category==EOVCatalogCategory::Pod;else Match=Category==0||(Category==1&&D.Category==EOVCatalogCategory::Hull)||(Category==2&&D.Category==EOVCatalogCategory::Fighter)||(Category==3&&D.Category==EOVCatalogCategory::Pod)||(Category==4&&D.Category==EOVCatalogCategory::Weapon)||(Category==5&&D.Category==EOVCatalogCategory::Hero)||(Category==6&&D.Category==EOVCatalogCategory::PointDefense);
  if(Match&&(D.DefinitionId.ToString().Contains(Filter)||D.DisplayName.Contains(Filter)))Add(D.DefinitionId,D.DisplayName+TEXT(" · ")+D.DefinitionId.ToString());
 }
}
void UOVDevelopmentWidget::CreateHero(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->CreateDevHero(0,static_cast<EOVInteriorRole>(FMath::Clamp(RoleChoice->GetSelectedIndex()+1,1,4)),HeroName->GetText().ToString());const FString Message=S->LastMessage;Rebuild();Feedback->SetText(FText::FromString(Message));}}
void UOVDevelopmentWidget::DeployHero(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SetDeployment(HeroId(),true);Feedback->SetText(FText::FromString(S->LastMessage));SelectItem(FName(*HeroId().ToString()));}}
void UOVDevelopmentWidget::ReleaseHero(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SetDeployment(HeroId(),false);Feedback->SetText(FText::FromString(S->LastMessage));SelectItem(FName(*HeroId().ToString()));}}
void UOVDevelopmentWidget::Spend(int32 I){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SpendSkillPoint(HeroId(),I);Feedback->SetText(FText::FromString(S->LastMessage));SelectItem(FName(*HeroId().ToString()));}}
void UOVDevelopmentWidget::Skill0(){Spend(0);}void UOVDevelopmentWidget::Skill1(){Spend(1);}void UOVDevelopmentWidget::Skill2(){Spend(2);}void UOVDevelopmentWidget::Skill3(){Spend(3);}
void UOVDevelopmentWidget::Save(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SaveRoster(TEXT("OVHeroRoster"),0);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::Load(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->LoadRoster(TEXT("OVHeroRoster"),0);const FString Message=S->LastMessage;Rebuild();Feedback->SetText(FText::FromString(Message));}}

void UOVDevelopmentWidget::Spawn()
{
 auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(!S||!Feedback)return;
 int32 Count=0,Side=0,Aircraft=0,People=0;float PX=0,PY=0,PZ=0,Gap=0;
 if(SelectedId.IsNone()||!LexTryParseString(Count,*Quantity->GetText().ToString())||!LexTryParseString(Side,*Faction->GetText().ToString())||!LexTryParseString(Aircraft,*Inventory->GetText().ToString())||!LexTryParseString(People,*Troops->GetText().ToString())||!LexTryParseString(PX,*X->GetText().ToString())||!LexTryParseString(PY,*Y->GetText().ToString())||!LexTryParseString(PZ,*Z->GetText().ToString())||!LexTryParseString(Gap,*Spacing->GetText().ToString())||!FMath::IsFinite(PX)||!FMath::IsFinite(PY)||!FMath::IsFinite(PZ)||!FMath::IsFinite(Gap)){Feedback->SetText(FText::FromString(TEXT("생성 불가: 함종과 올바른 숫자를 입력하세요.")));return;}
 S->SpawnShips(SelectedId,Side,Count,FVector(PX,PY,PZ),Gap,Aircraft,People,MoveAuto->IsChecked(),FireAuto->IsChecked());Feedback->SetText(FText::FromString(S->LastMessage));RefreshShips();
}
void UOVDevelopmentWidget::Delete(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){S->DeleteShip(PC->Selected);Feedback->SetText(FText::FromString(S->LastMessage));RefreshShips();}}
void UOVDevelopmentWidget::Equip(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=MountChoice->GetSelectedIndex();S->EquipWeapon(WeaponSubject.Get(),MountIds.IsValidIndex(I)?MountIds[I]:NAME_None,SelectedId);Feedback->SetText(FText::FromString(S->LastMessage));RefreshRuntime();}}
void UOVDevelopmentWidget::Target(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=TargetChoice->GetSelectedIndex();S->SetShipTarget(WeaponSubject.Get(),Targets.IsValidIndex(I)?Targets[I].Get():nullptr);Feedback->SetText(FText::FromString(S->LastMessage));RefreshRuntime();}}
void UOVDevelopmentWidget::Fire(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=MountChoice->GetSelectedIndex();S->FireWeapon(WeaponSubject.Get(),MountIds.IsValidIndex(I)?MountIds[I]:NAME_None);Feedback->SetText(FText::FromString(S->LastMessage));RefreshRuntime();}}
void UOVDevelopmentWidget::SpawnHero(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(PC&&PC->Interior){PC->Interior->SpawnRosterHero(PC->Selected,HeroId(),NAME_None);Feedback->SetText(FText::FromString(PC->Interior->LastMessage));}else if(Feedback)Feedback->SetText(FText::FromString(TEXT("실제 배치 불가: 선내 시스템을 사용할 수 없습니다.")));}

void UOVDevelopmentWidget::CreateFour(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){int32 Created=0;const TCHAR* Names[]={TEXT("근거리 시험 영웅"),TEXT("원거리 시험 영웅"),TEXT("지원 시험 영웅"),TEXT("지휘 시험 영웅")};for(int32 I=0;I<4;++I)if(S->CreateDevHero(0,static_cast<EOVInteriorRole>(I+1),Names[I]).IsValid())++Created;const FString Message=FString::Printf(TEXT("%d/4명 생성 · %s"),Created,*S->LastMessage);Rebuild();Feedback->SetText(FText::FromString(Message));}}
void UOVDevelopmentWidget::GrantXP(){auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();if(!S)return;int32 Amount=0;if(!LexTryParseString(Amount,*XPAmount->GetText().ToString())){Feedback->SetText(FText::FromString(TEXT("시험 XP는 정수로 입력하세요.")));return;}S->GrantDevXP(HeroId(),Amount);Feedback->SetText(FText::FromString(S->LastMessage));SelectItem(FName(*HeroId().ToString()));}

void UOVDevelopmentWidget::RefreshRuntime()
{
 if(!RuntimeDetail)return;auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC){RuntimeDetail->SetText(FText::FromString(TEXT("플레이어 없음")));return;}
 FString Text;
 if(Page==1){auto* Ship=HullCategory==0?RuntimeShip():PC->Selected.Get();Text=TEXT("선택 함선: ")+(Ship?Ship->ShipName:TEXT("없음"));if(HullCategory==0){if(auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()){for(const auto& C:L->Cargo)if(C.Ship==Ship)Text+=FString::Printf(TEXT("\n재고 일반 %.1f / 고급 %.1f / 최고급 %.1f / 적재한도 %.1f"),C.Amount,C.Advanced,C.Rare,C.Capacity);for(const auto& N:L->Nodes)Text+=FString::Printf(TEXT("\n노드 #%d · 잔량 %.1f"),N.Id,N.Amount);for(const auto& Link:L->RepairLinks)if(Link.Source==Ship)Text+=TEXT("\n수리: ")+Link.State;}}else if(HullCategory==1&&PC->Carrier){Text+=TEXT("\n")+PC->Carrier->GetCarrierStatus(Ship);for(const auto& A:PC->Carrier->Aircraft)if(A.Carrier.Get()==Ship)Text+=FString::Printf(TEXT("\n기체 #%d · 상태 %s · HP %.0f"),A.AircraftId,AircraftState(A.State),A.HP);}else if(HullCategory==2&&PC->Transport){Text+=TEXT("\n표적: ")+(Ship&&Ship->AttackTarget?Ship->AttackTarget->ShipName:TEXT("없음"));for(const auto& Person:PC->Transport->Persons)if(Person.CurrentShip.Get()==Ship)Text+=FString::Printf(TEXT("\n인원 #%d / 유닛 #%d · 상태 %s"),Person.PersonId,Person.RuntimeUnitId,PassengerState(Person.State));for(const auto& P:PC->Transport->Pods)if(P.Origin.Get()==Ship)Text+=FString::Printf(TEXT("\n포드 #%d / 인원 #%d · %s · HP %.0f"),P.PodId,P.PersonId,PodState(P.State),P.HP);}}
 else if(Page==3){AOVShip* Ship=WeaponSubject.Get();const int32 I=MountChoice?MountChoice->GetSelectedIndex():INDEX_NONE;const FName Id=MountIds.IsValidIndex(I)?MountIds[I]:NAME_None;Text=TEXT("장착점 상태: 선택 함선/장착점 없음");if(Ship&&PC->Combat&&Id!=NAME_None){const auto State=PC->Combat->GetShipState(Ship);if(const auto* M=State.Mounts.FindByPredicate([Id](const FOVCombatMountState& V){return V.MountId==Id;})){const auto R=PC->Combat->GetReadiness(Ship,Id);const auto* HP=Ship->Systems?Ship->Systems->WeaponMountStates.FindByPredicate([Id](const FOVWeaponMountState& V){return V.WeaponMountId==Id;}):nullptr;int32 RecentHits=0;for(const auto& E:PC->Combat->RecentEvents)if(E.Attacker.Get()==Ship&&E.WeaponId==Id&&E.Result==EOVAttackResult::Hit)++RecentHits;Text=FString::Printf(TEXT("%s / %s · HP %.0f\n발사 %d · 최근 이벤트 명중 %d (최대 128건)\n준비: %s · 재사용 %.2f초"),*Id.ToString(),*M->Profile.ProfileId.ToString(),HP?HP->HP:0,M->ShotsFired,RecentHits,*ReadyLabel(R.Reason),R.Cooldown);}}
  if(Ship&&PC->PointDefense&&Id!=NAME_None){const auto PD=PC->PointDefense->GetShipState(Ship);if(const auto* M=PD.Mounts.FindByPredicate([Id](const FOVPDMountState& V){return V.MountId==Id;})){int32 RecentHits=0;for(const auto& E:PC->PointDefense->RecentEvents)if(E.DefenderId==Ship->GetFName()&&E.MountId==Id&&E.bHit)++RecentHits;Text=FString::Printf(TEXT("요격 %s / %s · HP %.0f\n발사 %d · 최근 이벤트 명중 %d\n%s · 재사용 %.2f초 · 자동 요격"),*Id.ToString(),*M->Profile.ProfileId.ToString(),M->HP,M->ShotCount,RecentHits,*ReadyLabel(PC->PointDefense->GetReadiness(Ship)),M->Cooldown);}}
 }
 RuntimeDetail->SetText(FText::FromString(Text));
}
void UOVDevelopmentWidget::LaunchAircraft(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!PC->Carrier)return;const int32 Count=PC->Carrier->Launch(PC->Selected);Feedback->SetText(FText::FromString(FString::Printf(TEXT("출격 %d기 · %s"),Count,*PC->Carrier->GetCarrierStatus(PC->Selected))));RefreshRuntime();}
void UOVDevelopmentWidget::RecallAircraft(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!PC->Carrier)return;PC->Carrier->Recall(PC->Selected);Feedback->SetText(FText::FromString(PC->Carrier->GetCarrierStatus(PC->Selected)));RefreshRuntime();}
void UOVDevelopmentWidget::LaunchPods(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!PC->Transport)return;AOVShip* Ship=PC->Selected.Get();PC->Transport->LaunchPods(Ship,Ship?Ship->AttackTarget.Get():nullptr);Feedback->SetText(FText::FromString(PC->Transport->LastMessage));RefreshRuntime();}
void UOVDevelopmentWidget::RecallPods(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!PC->Transport)return;PC->Transport->RecallPods(PC->Selected);Feedback->SetText(FText::FromString(PC->Transport->LastMessage));RefreshRuntime();}

void UOVDevelopmentWidget::RefreshShips(){if(!RuntimeShips||!LogisticsTarget)return;AOVShip* Previous=RuntimeShip();RuntimeShips->ClearOptions();LogisticsTarget->ClearOptions();RuntimeShipIds.Reset();if(auto* PC=Cast<AOVController>(GetOwningPlayer()))for(const auto& S:PC->Ships)if(IsValid(S)){RuntimeShipIds.Add(S.Get());const FString Label=S->ShipName+TEXT(" · 실행 ID ")+S->GetFName().ToString();RuntimeShips->AddOption(Label);LogisticsTarget->AddOption(Label);}int32 I=RuntimeShipIds.IndexOfByPredicate([Previous](const auto& S){return S.Get()==Previous;});if(I==INDEX_NONE&&RuntimeShipIds.Num())I=0;if(I!=INDEX_NONE){RuntimeShips->SetSelectedIndex(I);LogisticsTarget->SetSelectedIndex(I);}}
AOVShip* UOVDevelopmentWidget::RuntimeShip() const{const int32 I=RuntimeShips?RuntimeShips->GetSelectedIndex():INDEX_NONE;return RuntimeShipIds.IsValidIndex(I)?RuntimeShipIds[I].Get():nullptr;}
void UOVDevelopmentWidget::SelectRuntimeShip(){if(auto* PC=Cast<AOVController>(GetOwningPlayer())){auto* Ship=RuntimeShip();if(!Ship){Feedback->SetText(FText::FromString(TEXT("실행 함선을 선택하세요.")));return;}PC->SelectShip(Ship);Feedback->SetText(FText::FromString(TEXT("함선 선택: ")+Ship->ShipName));RefreshRuntime();}}
void UOVDevelopmentWidget::FollowRuntimeShip(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!RuntimeShip()){if(Feedback)Feedback->SetText(FText::FromString(TEXT("실행 함선을 선택하세요.")));return;}PC->SelectShip(RuntimeShip());PC->Action(TEXT("Focus"));Feedback->SetText(FText::FromString(PC->bFollow?TEXT("함선 추적 중"):TEXT("추적 불가: 함선 상태를 확인하세요.")));}
void UOVDevelopmentWidget::InteriorRuntimeShip(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* Ship=RuntimeShip();if(!PC||!Ship||!PC->Interior||!PC->Interior->bEnabled){if(Feedback)Feedback->SetText(FText::FromString(TEXT("선내 지도 불가: 함선 및 선내 시스템 확인")));return;}PC->SelectShip(Ship);if(auto* H=Cast<AOVHUD>(PC->GetHUD())){H->HandleInteriorAction(H->bInteriorOpen?FName(TEXT("InteriorSelectedShip")):FName(TEXT("InteriorToggle")));Feedback->SetText(FText::FromString(TEXT("선내 지도 요청")));}}
namespace { bool DevNumber(UEditableTextBox* Input,float& Value){return Input&&LexTryParseString(Value,*Input->GetText().ToString())&&FMath::IsFinite(Value);} }
void UOVDevelopmentWidget::SetMaterials(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();float B=0,A=0,R=0;if(!L||!DevNumber(DevBasic,B)||!DevNumber(DevAdvanced,A)||!DevNumber(DevRare,R)){Feedback->SetText(FText::FromString(TEXT("재고 숫자를 확인하세요.")));return;}const bool Ok=L->SetDevMaterials(RuntimeShip(),FVector(B,A,R));Feedback->SetText(FText::FromString(Ok?TEXT("DEV 재고 설정 완료"):TEXT("재고 설정 불가: 등록 함선·양·적재한도 확인")));RefreshRuntime();}
void UOVDevelopmentWidget::AddNode(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();auto* Ship=RuntimeShip();float Amount=0;if(!L||!Ship||!DevNumber(DevAmount,Amount)){Feedback->SetText(FText::FromString(TEXT("함선과 노드량을 확인하세요.")));return;}const int32 Id=L->AddDevNode(Ship->GetActorLocation(),Amount);if(Id)DevNode->SetText(FText::AsNumber(Id));Feedback->SetText(FText::FromString(Id?FString::Printf(TEXT("DEV 자원 노드 #%d 생성"),Id):TEXT("노드 생성 불가: 수량 확인")));RefreshRuntime();}
void UOVDevelopmentWidget::Mine(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();float Amount=0,Range=0;int32 Id=0;if(!L||!DevNumber(DevAmount,Amount)||!DevNumber(DevRange,Range)||!LexTryParseString(Id,*DevNode->GetText().ToString())){Feedback->SetText(FText::FromString(TEXT("채굴 입력을 확인하세요.")));return;}const float Done=L->DevMine(RuntimeShip(),Id,Amount,Range);Feedback->SetText(FText::FromString(FString::Printf(TEXT("%.1f 채굴 · %s"),Done,*L->LastReason)));RefreshRuntime();}
void UOVDevelopmentWidget::Transfer(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();float Amount=0,Range=0;const int32 I=LogisticsTarget?LogisticsTarget->GetSelectedIndex():INDEX_NONE;if(!L||!DevNumber(DevAmount,Amount)||!DevNumber(DevRange,Range)){Feedback->SetText(FText::FromString(TEXT("수송 입력을 확인하세요.")));return;}const float Done=L->DevTransfer(RuntimeShip(),RuntimeShipIds.IsValidIndex(I)?RuntimeShipIds[I].Get():nullptr,Amount,Range);Feedback->SetText(FText::FromString(FString::Printf(TEXT("%.1f 수송 · %s"),Done,*L->LastReason)));RefreshRuntime();}
void UOVDevelopmentWidget::Repair(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();float B=0,A=0,R=0,Range=0;const int32 I=LogisticsTarget?LogisticsTarget->GetSelectedIndex():INDEX_NONE;if(!L||!DevNumber(DevQuoteBasic,B)||!DevNumber(DevQuoteAdvanced,A)||!DevNumber(DevQuoteRare,R)||!DevNumber(DevRange,Range)){Feedback->SetText(FText::FromString(TEXT("수리 견적과 거리를 확인하세요.")));return;}L->StartDevRepair(RuntimeShip(),RuntimeShipIds.IsValidIndex(I)?RuntimeShipIds[I].Get():nullptr,false,FGuid(),FVector(B,A,R),Range,0,0,0);Feedback->SetText(FText::FromString(L->LastReason));RefreshRuntime();}
