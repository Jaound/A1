#include "OVDevelopmentWidget.h"
#include "OVCatalogCardWidget.h"
#include "OVPortraitWidget.h"
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
#include "Components/HorizontalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace {
UTextBlock* Label(UWidgetTree* T,const FString& S) { auto* W=T->ConstructWidget<UTextBlock>();auto Font=W->GetFont();Font.Size=14;W->SetFont(Font);W->SetColorAndOpacity(FSlateColor(FLinearColor(.88f,.94f,1.f)));W->SetText(FText::FromString(S));W->SetAutoWrapText(true);return W; }
void StyleButton(UButton* Button) { Button->SetBackgroundColor(FLinearColor(.045f,.15f,.23f,1));Button->SetColorAndOpacity(FLinearColor(.95f,.98f,1.f)); }
UComboBoxString* Combo(UWidgetTree* T) { auto* C=T->ConstructWidget<UOVCompactCombo>();C->SetCompactFont();return C; }
UButton* AddButton(UWidgetTree* T,UPanelWidget* P,const TCHAR* S) { auto* B=T->ConstructWidget<UButton>();StyleButton(B);B->AddChild(Label(T,S));P->AddChild(B);if(auto* Slot=Cast<UHorizontalBoxSlot>(B->Slot)){Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(3,4));}else if(auto* VerticalSlot=Cast<UVerticalBoxSlot>(B->Slot))VerticalSlot->SetPadding(FMargin(0,3));return B; }
FString ShortShipName(const AOVShip* Ship){if(!Ship)return TEXT("없음");FString Name=Ship->ShipName;int32 At=INDEX_NONE;if(Name.FindChar(TCHAR(0x00b7),At))Name=Name.Left(At).TrimEnd();return Name;}
const TCHAR* SizeName(int32 Size){const TCHAR* Names[]={TEXT("초소형"),TEXT("소형"),TEXT("중형"),TEXT("대형"),TEXT("초대형")};return Names[FMath::Clamp(Size,0,4)];}
const TCHAR* WeaponName(EOVWeaponType Type){switch(Type){case EOVWeaponType::Beam:return TEXT("빔");case EOVWeaponType::Cannon:return TEXT("함포");case EOVWeaponType::Missile:return TEXT("미사일");case EOVWeaponType::Interception:return TEXT("요격");default:return TEXT("무장");}}
UHorizontalBox* Row(UWidgetTree* T,UVerticalBox* Parent){auto* R=T->ConstructWidget<UHorizontalBox>();Parent->AddChildToVerticalBox(R)->SetPadding(FMargin(0,3));return R;}
UVerticalBox* Section(UWidgetTree* T,UVerticalBox* Parent,const FString& Title){auto* B=T->ConstructWidget<UBorder>();B->SetBrushColor(FLinearColor(.025f,.055f,.085f));B->SetPadding(FMargin(8));Parent->AddChildToVerticalBox(B)->SetPadding(FMargin(0,5));auto* V=T->ConstructWidget<UVerticalBox>();B->AddChild(V);if(!Title.IsEmpty())V->AddChildToVerticalBox(Label(T,Title))->SetPadding(FMargin(0,0,0,5));return V;}
const TCHAR* AircraftState(EOVAircraftState State){const TCHAR* V[]={TEXT("보관"),TEXT("출격"),TEXT("귀환"),TEXT("착함 대기"),TEXT("관제 상실"),TEXT("파괴")};return V[FMath::Clamp(int32(State),0,5)];}
const TCHAR* PassengerState(EOVPassengerState State){const TCHAR* V[]={TEXT("탑승"),TEXT("포드 비행"),TEXT("목표함 내부"),TEXT("강습선"),TEXT("회수 대기"),TEXT("구조 대기"),TEXT("사망")};return V[FMath::Clamp(int32(State),0,6)];}
const TCHAR* PodState(EOVPodFlightState State){const TCHAR* V[]={TEXT("발사 대기"),TEXT("접근"),TEXT("접촉"),TEXT("귀환"),TEXT("회수"),TEXT("파괴"),TEXT("고립"),TEXT("취소")};return V[FMath::Clamp(int32(State),0,7)];}
FString ReadyLabel(const FString& Key) { static const TMap<FString,FString> L={{TEXT("READY"),TEXT("발사 가능")},{TEXT("NO VALID TARGET"),TEXT("표적 없음")},{TEXT("OUT OF RANGE"),TEXT("사거리 밖")},{TEXT("NO DETECTION"),TEXT("탐지 없음")},{TEXT("MOUNT BLIND ARC"),TEXT("사각")},{TEXT("TURRET ALIGNING"),TEXT("조준 중")},{TEXT("FRIENDLY IN LINE"),TEXT("아군 사선 차단")},{TEXT("COOLDOWN"),TEXT("재사용 대기")},{TEXT("WEAPON CONTROL OFF"),TEXT("무기제어실 정지")},{TEXT("SUPPLY OFF"),TEXT("공급 정지")},{TEXT("MOUNT DESTROYED"),TEXT("무장 파괴")},{TEXT("COMBAT OFF"),TEXT("전투 비활성")},{TEXT("PD OFF"),TEXT("요격 비활성")},{TEXT("SHIP LOST"),TEXT("함선 상실")},{TEXT("NO OWNER"),TEXT("소유 세력 없음")},{TEXT("DEPARTED"),TEXT("함선 이탈")},{TEXT("NO PD MOUNTS"),TEXT("요격 장착점 없음")},{TEXT("PD CONTROL OFFLINE"),TEXT("요격 제어 정지")},{TEXT("PD MOUNTS DISABLED"),TEXT("요격 장착점 정지")},{TEXT("DEFENDING"),TEXT("자동 방어 중")}};if(const auto* V=L.Find(Key))return *V;return TEXT("발사 조건 확인"); }
FString Evidence(const TCHAR* Name,const FOVCatalogEvidence& E) { const TCHAR* States[]={TEXT("미확인"),TEXT("있음"),TEXT("부분"),TEXT("없음"),TEXT("해당 없음")}; return FString::Printf(TEXT("%s: %s · %s"),Name,States[FMath::Clamp(int32(E.Status),0,4)],*E.Reason); }
UEditableTextBox* AddInput(UWidgetTree* T,UVerticalBox* P,const TCHAR* Hint,const TCHAR* Default) { P->AddChildToVerticalBox(Label(T,Hint));auto* W=T->ConstructWidget<UEditableTextBox>();auto Style=W->GetWidgetStyle();Style.TextStyle.Font.Size=14;Style.TextStyle.ColorAndOpacity=FSlateColor(FLinearColor(.02f,.025f,.035f,1));Style.ForegroundColor=FSlateColor(FLinearColor(.02f,.025f,.035f,1));W->SetWidgetStyle(Style);W->SetForegroundColor(FLinearColor(.02f,.025f,.035f,1));W->SetHintText(FText::FromString(Hint));W->SetText(FText::FromString(Default));P->AddChildToVerticalBox(W);return W; }
}
void UOVDevelopmentItem::Pick() { if(Owner)Owner->SelectItem(Id); }
void UOVDevelopmentWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 auto* Border=WidgetTree->ConstructWidget<UBorder>();
 Border->SetBrushColor(FLinearColor(.008f,.02f,.04f,.985f));Border->SetPadding(FMargin(10));WidgetTree->RootWidget=Border;
 auto* Root=WidgetTree->ConstructWidget<UVerticalBox>();Border->AddChild(Root);
 auto* Tabs=Row(WidgetTree,Root);
 AddButton(WidgetTree,Tabs,TEXT("함선"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Hulls);
 AddButton(WidgetTree,Tabs,TEXT("영웅"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Heroes);
 AddButton(WidgetTree,Tabs,TEXT("무장"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Weapons);
 AddButton(WidgetTree,Tabs,TEXT("시험"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Overview);
 AddButton(WidgetTree,Tabs,TEXT("닫기 ×"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Close);
 Feedback=Label(WidgetTree,TEXT("시험용 생성 · 실제 전투 규칙 적용"));
 Feedback->SetColorAndOpacity(FLinearColor(.45f,.85f,.95f));Root->AddChildToVerticalBox(Feedback)->SetPadding(FMargin(3,4));
 auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Root->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 Body=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Body);
 AdvancedButton=AddButton(WidgetTree,Root,TEXT("고급 진단 ▸"));AdvancedButton->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::ToggleAdvanced);
 Rebuild();
}
void UOVDevelopmentWidget::Reply(const FString& Message)
{
 if(!Feedback)return;
 Feedback->SetToolTipText(FText::FromString(Message));
 FString Brief=Message;Brief.ReplaceInline(TEXT("READY"),TEXT("발사 가능"));
 int32 Break=INDEX_NONE;if(Brief.FindChar('\n',Break))Brief=Brief.Left(Break);
 if(Brief.Len()>64)Brief=Brief.Left(61)+TEXT("…");
 Feedback->SetText(FText::FromString(Brief));
}
FString UOVDevelopmentWidget::GetCompactSelectionText()const{return Detail?Detail->GetText().ToString():FString();}
void UOVDevelopmentWidget::ExecuteSelectedCreation(){if(Page==1&&HullCategory==0)Spawn();else if(Page==2)CreateHero();}
void UOVDevelopmentWidget::ToggleAdvanced(){SetAdvancedDiagnostics(!bAdvanced);}
void UOVDevelopmentWidget::SetAdvancedDiagnostics(bool bEnabled)
{
 bAdvanced=bEnabled;
 for(const auto& Group:AdvancedGroups)if(Group)Group->SetVisibility(bAdvanced?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if(AdvancedButton)if(auto* Text=Cast<UTextBlock>(AdvancedButton->GetContent()))Text->SetText(FText::FromString(bAdvanced?TEXT("고급 진단 ▾"):TEXT("고급 진단 ▸")));
 if(!SelectedId.IsNone())SelectItem(SelectedId);
 RefreshRuntime();
}
void UOVDevelopmentWidget::SideChanged(FString,ESelectInfo::Type)
{
 if(!SideChoice)return;
 if(Page==2){HeroFaction=SideChoice->GetSelectedIndex();Rebuild();return;}
 if(Faction)Faction->SetText(FText::AsNumber(SideChoice->GetSelectedIndex()));
}
void UOVDevelopmentWidget::PresetChanged(FString,ESelectInfo::Type)
{
 if(!PresetChoice)return;
 const int32 Preset=PresetChoice->GetSelectedIndex();
 if(Inventory)Inventory->SetText(FText::AsNumber(Preset==1?20:0));
 if(Troops)Troops->SetText(FText::AsNumber(Preset==2?10:0));
}
void UOVDevelopmentWidget::SceneCombat(){if(auto* PC=Cast<AOVController>(GetOwningPlayer()))PC->Action(TEXT("CombatRange"));Reply(TEXT("기본 전투 시험장"));}
void UOVDevelopmentWidget::SceneCalibration(){if(auto* PC=Cast<AOVController>(GetOwningPlayer()))PC->StartCalibrationRange(0);Reply(TEXT("함선 생성 시험장 · 일시정지 상태"));}
void UOVDevelopmentWidget::SceneCarrier(){if(auto* PC=Cast<AOVController>(GetOwningPlayer()))PC->StartPhase2CRange(5);Reply(TEXT("함재기·운송 혼성 시험장"));}
void UOVDevelopmentWidget::SceneMission(){if(auto* PC=Cast<AOVController>(GetOwningPlayer()))PC->Action(TEXT("MissionAttack"));Reply(TEXT("영웅 강습 시험 임무 · 임무 전용 인원"));}
void UOVDevelopmentWidget::SceneRoster(){if(auto* S=GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>()){const bool Opened=S->OpenRosterRoom();Reply(Opened?TEXT("편성실 · 시험 영웅을 생성할 수 있습니다"):S->LastMessage);}}
void UOVDevelopmentWidget::SceneRestart(){if(auto* PC=Cast<AOVController>(GetOwningPlayer()))PC->Action(TEXT("RestartScene"));Reply(TEXT("현재 시험 장면 재시작"));}
void UOVDevelopmentWidget::PauseScene(){if(auto* PC=Cast<AOVController>(GetOwningPlayer())){PC->Action(TEXT("Pause"));Reply(PC->bSimulationPaused?TEXT("일시정지"):TEXT("전투 진행"));}}
bool UOVDevelopmentWidget::IsEditing() const { return (Search&&Search->HasKeyboardFocus())||(Quantity&&Quantity->HasKeyboardFocus())||(X&&X->HasKeyboardFocus())||(Y&&Y->HasKeyboardFocus())||(Z&&Z->HasKeyboardFocus())||(Spacing&&Spacing->HasKeyboardFocus())||(Faction&&Faction->HasKeyboardFocus())||(Inventory&&Inventory->HasKeyboardFocus())||(Troops&&Troops->HasKeyboardFocus())||(HeroName&&HeroName->HasKeyboardFocus())||(XPAmount&&XPAmount->HasKeyboardFocus())||(SupportBasic&&SupportBasic->HasKeyboardFocus())||(SupportAdvanced&&SupportAdvanced->HasKeyboardFocus())||(SupportRare&&SupportRare->HasKeyboardFocus())||(SupportCharges&&SupportCharges->HasKeyboardFocus())||(DevAmount&&DevAmount->HasKeyboardFocus())||(DevRange&&DevRange->HasKeyboardFocus())||(DevNode&&DevNode->HasKeyboardFocus())||(DevBasic&&DevBasic->HasKeyboardFocus())||(DevAdvanced&&DevAdvanced->HasKeyboardFocus())||(DevRare&&DevRare->HasKeyboardFocus())||(DevQuoteBasic&&DevQuoteBasic->HasKeyboardFocus())||(DevQuoteAdvanced&&DevQuoteAdvanced->HasKeyboardFocus())||(DevQuoteRare&&DevQuoteRare->HasKeyboardFocus()); }
bool UOVDevelopmentWidget::OpenPage(int32 Index){if(Index<0||Index>3)return false;Page=Index;Rebuild();return true;}
void UOVDevelopmentWidget::Overview(){Page=0;Rebuild();}void UOVDevelopmentWidget::Hulls(){Page=1;Rebuild();}void UOVDevelopmentWidget::Heroes(){Page=2;Rebuild();}void UOVDevelopmentWidget::Weapons(){Page=3;Rebuild();}
void UOVDevelopmentWidget::Close(){if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(PC->bDeveloperMode)PC->Action(TEXT("Developer"));}
void UOVDevelopmentWidget::SearchChanged(const FText&){SelectedId=NAME_None;if(Detail)Detail->SetText(FText::FromString(TEXT("목록에서 항목을 선택하세요.")));RefreshList();}
void UOVDevelopmentWidget::CategoryChanged(FString, ESelectInfo::Type){if(Page==1){HullCategory=CategoryChoice->GetSelectedIndex();Rebuild();return;}if(Page==0)OverviewCategory=CategoryChoice->GetSelectedIndex();SelectedId=NAME_None;if(Detail)Detail->SetText(FText::FromString(TEXT("목록에서 항목을 선택하세요.")));RefreshList();}
bool UOVDevelopmentWidget::SetFilters(int32 SizeClass,FName RoleTag,int32 StatusAxis,int32 StatusValue)
{
 if(!SizeFilter||!RoleFilter||!AxisFilter||!StatusFilter||SizeClass<0||SizeClass>4||StatusAxis<INDEX_NONE||StatusAxis>5||StatusValue<INDEX_NONE||StatusValue>4||(StatusAxis==INDEX_NONE&&StatusValue!=INDEX_NONE))return false;
 const int32 RoleIndex=RoleTag.IsNone()?INDEX_NONE:FilterRoles.IndexOfByKey(RoleTag);if(!RoleTag.IsNone()&&RoleIndex==INDEX_NONE)return false;
 TGuardValue<bool> Updating(bUpdatingFilters,true);FilterSize=SizeClass;FilterRole=RoleTag;FilterAxis=StatusAxis;FilterStatus=StatusValue;
 SizeFilter->SetSelectedIndex(FilterSize);RoleFilter->SetSelectedIndex(RoleIndex+1);AxisFilter->SetSelectedIndex(FilterAxis+1);StatusFilter->SetSelectedIndex(FilterStatus+1);StatusFilter->SetIsEnabled(FilterAxis!=INDEX_NONE);
 SelectedId=NAME_None;if(Detail)Detail->SetText(FText::FromString(TEXT("목록에서 항목을 선택하세요.")));RefreshList();return true;
}
void UOVDevelopmentWidget::FiltersChanged(FString,ESelectInfo::Type)
{
 if(bUpdatingFilters||!SizeFilter||!RoleFilter||!AxisFilter||!StatusFilter)return;
 const int32 RoleIndex=RoleFilter->GetSelectedIndex()-1;const int32 Axis=AxisFilter->GetSelectedIndex()-1;const int32 Status=Axis==INDEX_NONE?INDEX_NONE:StatusFilter->GetSelectedIndex()-1;
 SetFilters(SizeFilter->GetSelectedIndex(),FilterRoles.IsValidIndex(RoleIndex)?FilterRoles[RoleIndex]:NAME_None,Axis,Status);
}
void UOVDevelopmentWidget::SelectItem(FName Id)
{
 if(Id.ToString().StartsWith(TEXT("Action_")))
 {
  if(auto* PC=Cast<AOVController>(GetOwningPlayer())){PC->Action(FName(*Id.ToString().Mid(7)));Reply(PC->LastAction);}return;
 }
 if(Id.ToString().StartsWith(TEXT("Role_")))
 {
  const int32 Role=FCString::Atoi(*Id.ToString().Mid(5));
  if(RoleChoice)RoleChoice->SetSelectedIndex(FMath::Clamp(Role-1,0,3));
  const TCHAR* Names[]={TEXT("근거리 시험 영웅"),TEXT("원거리 시험 영웅"),TEXT("지원 시험 영웅"),TEXT("지휘 시험 영웅")};
  if(HeroName)HeroName->SetText(FText::FromString(Names[FMath::Clamp(Role-1,0,3)]));
  Reply(TEXT("유형 선택 · 생성 버튼을 누르세요"));return;
 }
 SelectedId=Id;UpdateCardSelection();
 if(EquipButton)
 {
  const bool bSupported=OVCalibration::GetWeaponProfileIds().Contains(Id);
  EquipButton->SetIsEnabled(bSupported&&WeaponSubject.IsValid()&&MountIds.Num()>0);
  EquipButton->SetToolTipText(FText::FromString(bSupported?TEXT("장착 계열·티어·시설 조건을 검사합니다"):TEXT("이 종류의 직접 교체는 미지원입니다")));
 }
 if(!Detail)return;
 if(!bAdvanced)
 {
  if(Page==2)
  {
   FGuid Guid;if(!FGuid::Parse(Id.ToString(),Guid))return;
   const int32 Index=HeroIds.IndexOfByKey(Guid);if(Index==INDEX_NONE)return;
   if(HeroChoice&&HeroChoice->GetSelectedIndex()!=Index)HeroChoice->SetSelectedIndex(Index);
   auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();if(!S)return;
   const auto H=S->GetHero(Guid);const TCHAR* Roles[]={TEXT("병력"),TEXT("근거리"),TEXT("원거리"),TEXT("지원"),TEXT("지휘")};
   const TCHAR* States[]={TEXT("대기"),TEXT("편성"),TEXT("구조 대기"),TEXT("포로"),TEXT("실종"),TEXT("사망")};
   Detail->SetText(FText::FromString(FString::Printf(TEXT("%s  ·  %s Lv.%d  ·  %s\nHP %.0f  ·  배치 %d / 15"),*H.Name,Roles[FMath::Clamp(int32(H.Role),0,4)],H.Level,States[FMath::Clamp(int32(H.Status),0,5)],H.CurrentHP,S->CountDeployed(H.FactionId))));return;
  }
  if(auto* C=GetWorld()->GetSubsystem<UOVDevelopmentCatalogue>())for(const auto& D:C->GetDefinitions())if(D.DefinitionId==Id)
  {
   FString Brief=D.DisplayName;
   if(D.Category==EOVCatalogCategory::Hull)Brief+=FString::Printf(TEXT("  ·  %s  ·  코스트 %d"),SizeName(D.SizeClass),D.FleetCost);
   else if(D.Category==EOVCatalogCategory::Fighter)Brief+=TEXT("  ·  모함의 실제 재고로 출격");
   else if(D.Category==EOVCatalogCategory::Pod)Brief+=TEXT("  ·  승객 1명당 1기");
   if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(PC->Combat&&PC->Combat->Settings)
    if(const auto* W=PC->Combat->Settings->Weapons.FindByPredicate([Id](const FOVWeaponProfile& P){return P.ProfileId==Id;}))
     Brief+=FString::Printf(TEXT("\n%s T%d  ·  피해 %.0f  ·  %.0fm  ·  %.1f초"),WeaponName(W->Family),W->Tier,W->Damage,W->Range/100,W->Interval);
   if(D.Data.Status==EOVCatalogStatus::Unavailable)Brief+=TEXT("  ·  미지원");
   Detail->SetText(FText::FromString(Brief));return;
  }
  Detail->SetText(FText::FromString(TEXT("항목을 선택하세요")));return;
 }
 if(Page==2){FGuid Guid;if(!FGuid::Parse(Id.ToString(),Guid))return;const int32 Index=HeroIds.IndexOfByKey(Guid);if(Index==INDEX_NONE)return;if(HeroChoice&&HeroChoice->GetSelectedIndex()!=Index)HeroChoice->SetSelectedIndex(Index);auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();if(!S)return;const auto H=S->GetHero(Guid);const TCHAR* Roles[]={TEXT("병력"),TEXT("근거리"),TEXT("원거리"),TEXT("지원"),TEXT("지휘")};const TCHAR* States[]={TEXT("예비"),TEXT("편성 예약"),TEXT("구조 대기"),TEXT("포로"),TEXT("실종"),TEXT("사망")};FString Skills;for(int32 I=0;I<H.SkillRanks.Num();++I)Skills+=FString::Printf(TEXT(" %d:%d"),I+1,H.SkillRanks[I]);FString Place=TEXT("전장 연결 없음");if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(PC->Interior&&H.RuntimeUnitId){const auto U=PC->Interior->GetUnit(H.RuntimeUnitId);if(U.UnitId==H.RuntimeUnitId)Place=(U.Ship.IsValid()?U.Ship->ShipName:TEXT("위치 미확인"))+TEXT(" / ")+U.NodeId.ToString();}FString Summary=FString::Printf(TEXT("%s · %s · %s\n개인 ID %s\nLv%d / XP%d / 투자점수%d\n스킬 랭크%s · 촉매%d / HP %.0f\n%s"),*H.Name,Roles[FMath::Clamp(int32(H.Role),0,4)],States[FMath::Clamp(int32(H.Status),0,5)],*Guid.ToString(),H.Level,H.XP,H.SkillPoints,*Skills,H.Catalysts,H.CurrentHP,*Place);Summary+=FString::Printf(TEXT("\n정비 증폭기 충전 %d / DEV 기본 견적 %.2f·%.2f·%.2f"),H.AmplifierCharges,H.DevRepairBaseCost.X,H.DevRepairBaseCost.Y,H.DevRepairBaseCost.Z);for(int32 Skill=0;Skill<4;++Skill)Summary+=TEXT("\n")+S->GetSkillDescription(Guid,Skill);Detail->SetText(FText::FromString(Summary));return;}
 FString Text=TEXT("선택 ID: ")+Id.ToString();if(auto* C=GetWorld()->GetSubsystem<UOVDevelopmentCatalogue>())for(const auto& D:C->GetDefinitions())if(D.DefinitionId==Id){Text=D.DisplayName+TEXT(" · ")+Id.ToString()+TEXT("\n")+D.Description+TEXT("\n")+Evidence(TEXT("기획"),D.Planning)+TEXT("\n")+Evidence(TEXT("데이터"),D.Data)+TEXT("\n")+Evidence(TEXT("생성"),D.Generation)+TEXT("\n")+Evidence(TEXT("역할"),D.Role)+TEXT("\n")+Evidence(TEXT("자동 검증"),D.AutomatedVerification)+TEXT("\n")+Evidence(TEXT("직접 조작"),D.DirectInput)+TEXT("\n원본: ")+D.SourceEvidence;if(D.SizeClass>0){const TCHAR* Sizes[]={TEXT("없음"),TEXT("소형"),TEXT("중형"),TEXT("대형"),TEXT("초대형")};Text+=FString::Printf(TEXT("\n함선 체급: %s · 편성 코스트 %d"),Sizes[FMath::Clamp(D.SizeClass,0,4)],D.FleetCost);if(D.RoleTags.Num()){Text+=TEXT("\n역할:");for(FName Tag:D.RoleTags)Text+=TEXT(" ")+Tag.ToString();}}if(!D.AutomatedVerification.VerificationSHA.IsEmpty())Text+=TEXT("\n검증 리비전: ")+D.AutomatedVerification.VerificationSHA;break;}
 if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(PC->Combat&&PC->Combat->Settings)if(const auto* W=PC->Combat->Settings->Weapons.FindByPredicate([Id](const FOVWeaponProfile& P){return P.ProfileId==Id;}))Text+=FString::Printf(TEXT("\n실행 프로필: T%d / 피해 %.1f / 사거리 %.0f cm / 주기 %.2f초 / 속도 %.0f cm/s / 일제 %d"),W->Tier,W->Damage,W->Range,W->Interval,W->Speed,W->Salvo);
 Detail->SetText(FText::FromString(Text));
}
void UOVDevelopmentWidget::HeroChanged(FString,ESelectInfo::Type){const FGuid Id=HeroId();if(Id.IsValid())SelectItem(FName(*Id.ToString()));}
void UOVDevelopmentWidget::MountChanged(FString,ESelectInfo::Type){RefreshRuntime();}

FGuid UOVDevelopmentWidget::HeroId() const { const int32 I=HeroChoice?HeroChoice->GetSelectedIndex():INDEX_NONE;return HeroIds.IsValidIndex(I)?HeroIds[I]:FGuid(); }
void UOVDevelopmentWidget::Rebuild()
{
 if(!Body)return;
 Body->ClearChildren();AdvancedGroups.Reset();CardButtons.Reset();Cards=nullptr;
 Search=nullptr;List=nullptr;Detail=nullptr;RuntimeDetail=nullptr;RuntimeShips=nullptr;LogisticsTarget=nullptr;
 HeroChoice=nullptr;RoleChoice=nullptr;CategoryChoice=nullptr;SizeFilter=nullptr;RoleFilter=nullptr;AxisFilter=nullptr;StatusFilter=nullptr;
 SideChoice=nullptr;PresetChoice=nullptr;HeroDestination=nullptr;VisibleDefinitionIds.Reset();SelectedId=NAME_None;
 HeroName=nullptr;XPAmount=nullptr;SupportBasic=nullptr;SupportAdvanced=nullptr;SupportRare=nullptr;SupportCharges=nullptr;
 Quantity=nullptr;X=nullptr;Y=nullptr;Z=nullptr;Spacing=nullptr;Faction=nullptr;Inventory=nullptr;Troops=nullptr;
 MoveAuto=nullptr;FireAuto=nullptr;MountChoice=nullptr;TargetChoice=nullptr;EquipButton=nullptr;
 DevAmount=nullptr;DevRange=nullptr;DevNode=nullptr;DevBasic=nullptr;DevAdvanced=nullptr;DevRare=nullptr;DevQuoteBasic=nullptr;DevQuoteAdvanced=nullptr;DevQuoteRare=nullptr;
 auto* PC=Cast<AOVController>(GetOwningPlayer());
 auto Advanced=[&](UVerticalBox* Parent,const TCHAR* Title)
 {
  auto* V=Section(WidgetTree,Parent,Title);AdvancedGroups.Add(V->GetParent());
  V->GetParent()->SetVisibility(bAdvanced?ESlateVisibility::Visible:ESlateVisibility::Collapsed);return V;
 };
 auto InputColumn=[&](UHorizontalBox* Parent,const TCHAR* Hint,const TCHAR* Value)
 {
  auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();auto* ColumnSlot=Parent->AddChildToHorizontalBox(Column);
  ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));ColumnSlot->SetPadding(FMargin(3,0));return AddInput(WidgetTree,Column,Hint,Value);
 };
 if(Page==0)
 {
  auto* Scenes=Section(WidgetTree,Body,TEXT("시험 장면"));
  auto* First=Row(WidgetTree,Scenes);
  AddButton(WidgetTree,First,TEXT("기본 전투"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SceneCombat);
  AddButton(WidgetTree,First,TEXT("함선 생성장"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SceneCalibration);
  auto* Second=Row(WidgetTree,Scenes);
  AddButton(WidgetTree,Second,TEXT("함재기 · 운송"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SceneCarrier);
  AddButton(WidgetTree,Second,TEXT("영웅 강습 임무"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SceneMission);
  auto* Commands=Row(WidgetTree,Scenes);
  AddButton(WidgetTree,Commands,TEXT("정지 / 진행"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::PauseScene);
  AddButton(WidgetTree,Commands,TEXT("장면 재시작"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SceneRestart);
  Scenes->AddChildToVerticalBox(Label(WidgetTree,TEXT("장면 변경 시 현재 시험 전투를 초기화합니다.")));
  auto* Direct=Advanced(Body,TEXT("직접 상태 조작 · DEV"));
  auto* Tools=WidgetTree->ConstructWidget<UWrapBox>();Tools->SetInnerSlotPadding(FVector2D(5,5));Direct->AddChildToVerticalBox(Tools);
  const TCHAR* ToolIds[]={TEXT("TestHit"),TEXT("ArmorZero"),TEXT("RepairArmor"),TEXT("Reset"),TEXT("DestroyExt"),TEXT("Sabotage"),TEXT("RepairSlot"),TEXT("Occupy"),TEXT("CrewDown"),TEXT("CrewUp"),TEXT("WeaponHit"),TEXT("RepairWeapon"),TEXT("DevInterior"),TEXT("DevObjective"),TEXT("DevEntryReturn"),TEXT("TrialFTL"),TEXT("DevFighterRepair"),TEXT("DevLoseShuttle")};
  const TCHAR* ToolNames[]={TEXT("피해 주기"),TEXT("장갑 제거"),TEXT("장갑 복원"),TEXT("상태 초기화"),TEXT("시설 피격"),TEXT("시설 공작"),TEXT("시설 복원"),TEXT("점령 전환"),TEXT("인원 감소"),TEXT("인원 복원"),TEXT("무장 피격"),TEXT("무장 복원"),TEXT("내부 보호 입력"),TEXT("목표 달성 입력"),TEXT("DEV 진입점 복귀"),TEXT("FTL 시험 충전"),TEXT("기체 시험 수리"),TEXT("강습선 상실")};
  for(int32 I=0;I<UE_ARRAY_COUNT(ToolIds);++I){auto* Button=WidgetTree->ConstructWidget<UOVDevelopmentItem>();Button->Owner=this;Button->Id=FName(*(FString(TEXT("Action_"))+ToolIds[I]));StyleButton(Button);Button->AddChild(Label(WidgetTree,ToolNames[I]));Button->OnClicked.AddDynamic(Button,&UOVDevelopmentItem::Pick);Tools->AddChildToWrapBox(Button);}
 }
 if(Page==0||Page==1||Page==3)
 {
  UVerticalBox* CatalogueBody=Page==0?Advanced(Body,TEXT("카탈로그 진단")):Body.Get();
  Search=AddInput(WidgetTree,CatalogueBody,TEXT("이름 검색"),TEXT(""));Search->OnTextChanged.AddDynamic(this,&UOVDevelopmentWidget::SearchChanged);
  if(Page==0||Page==1)
  {
   CategoryChoice=Combo(WidgetTree);CatalogueBody->AddChildToVerticalBox(CategoryChoice);
   if(Page==0)CategoryChoice->AddOption(TEXT("전체"));
   for(const TCHAR* Name:{TEXT("함선"),TEXT("초소형 함재기"),TEXT("운송 포드")})CategoryChoice->AddOption(Name);
   if(Page==0)for(const TCHAR* Name:{TEXT("무장"),TEXT("영웅"),TEXT("요격 무장")})CategoryChoice->AddOption(Name);
   CategoryChoice->SetSelectedIndex(Page==0?OverviewCategory:HullCategory);CategoryChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::CategoryChanged);
  }
  auto* Catalogue=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentCatalogue>():nullptr;
  FilterRoles.Reset();if(Catalogue)for(const auto& Def:Catalogue->GetDefinitions())for(FName Tag:Def.RoleTags)if(!Tag.IsNone())FilterRoles.AddUnique(Tag);
  FilterRoles.Sort([](FName A,FName B){return A.ToString()<B.ToString();});
  auto* Filters=Row(WidgetTree,CatalogueBody);
  SizeFilter=Combo(WidgetTree);Filters->AddChildToHorizontalBox(SizeFilter)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  for(const TCHAR* Name:{TEXT("모든 체급"),TEXT("소형"),TEXT("중형"),TEXT("대형"),TEXT("초대형")})SizeFilter->AddOption(Name);SizeFilter->SetSelectedIndex(FilterSize);
  RoleFilter=Combo(WidgetTree);Filters->AddChildToHorizontalBox(RoleFilter)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));RoleFilter->AddOption(TEXT("모든 역할"));
  for(FName Tag:FilterRoles)RoleFilter->AddOption(Tag.ToString());
  const int32 RoleIndex=FilterRoles.IndexOfByKey(FilterRole);if(RoleIndex==INDEX_NONE)FilterRole=NAME_None;RoleFilter->SetSelectedIndex(RoleIndex+1);
  auto* Diagnostics=Advanced(CatalogueBody,TEXT("검증 상태 필터"));
  auto* StatusFilters=Row(WidgetTree,Diagnostics);
  AxisFilter=Combo(WidgetTree);StatusFilters->AddChildToHorizontalBox(AxisFilter)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  for(const TCHAR* Name:{TEXT("모든 축"),TEXT("기획"),TEXT("데이터"),TEXT("생성"),TEXT("역할"),TEXT("자동 검증"),TEXT("직접 입력")})AxisFilter->AddOption(Name);AxisFilter->SetSelectedIndex(FilterAxis+1);
  StatusFilter=Combo(WidgetTree);StatusFilters->AddChildToHorizontalBox(StatusFilter)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  for(const TCHAR* Name:{TEXT("모든 상태"),TEXT("미확인"),TEXT("있음"),TEXT("부분"),TEXT("없음"),TEXT("해당 없음")})StatusFilter->AddOption(Name);StatusFilter->SetSelectedIndex(FilterStatus+1);StatusFilter->SetIsEnabled(FilterAxis!=INDEX_NONE);
  SizeFilter->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::FiltersChanged);RoleFilter->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::FiltersChanged);AxisFilter->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::FiltersChanged);StatusFilter->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::FiltersChanged);
  List=WidgetTree->ConstructWidget<UScrollBox>();auto* ListSize=WidgetTree->ConstructWidget<USizeBox>();ListSize->SetHeightOverride(176);ListSize->AddChild(List);CatalogueBody->AddChildToVerticalBox(ListSize)->SetPadding(FMargin(0,5));
  Cards=WidgetTree->ConstructWidget<UWrapBox>();Cards->SetInnerSlotPadding(FVector2D(5,5));List->AddChild(Cards);
  Detail=Label(WidgetTree,TEXT("카드를 선택하세요"));CatalogueBody->AddChildToVerticalBox(Detail)->SetPadding(FMargin(3,5));RefreshList();
  if(Page==0)
  {
   if(Catalogue)Diagnostics->AddChildToVerticalBox(Label(WidgetTree,Catalogue->GetOverviewText()));
   return;
  }
  if(Page==1&&HullCategory!=0)
  {
   auto* Group=Section(WidgetTree,Body,HullCategory==1?TEXT("선택 모함"):TEXT("선택 출발함"));
   RuntimeDetail=Label(WidgetTree,TEXT(""));Group->AddChildToVerticalBox(RuntimeDetail);
   auto* Commands=Row(WidgetTree,Group);
   if(HullCategory==1)
   {
    AddButton(WidgetTree,Commands,TEXT("출격"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::LaunchAircraft);
    AddButton(WidgetTree,Commands,TEXT("복귀"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::RecallAircraft);
   }
   else
   {
    AddButton(WidgetTree,Commands,TEXT("포드 출격"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::LaunchPods);
    AddButton(WidgetTree,Commands,TEXT("포드 회항"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::RecallPods);
   }
   AddButton(WidgetTree,Commands,TEXT("새로고침"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::RefreshRuntime);
   Group->AddChildToVerticalBox(Label(WidgetTree,TEXT("현재 재고와 탑승 인원만 사용")));RefreshRuntime();return;
  }
  if(Page==1)
  {
   auto* Create=Section(WidgetTree,Body,TEXT("시험 함선 생성"));
   auto* Setup=Row(WidgetTree,Create);
   SideChoice=Combo(WidgetTree);Setup->AddChildToHorizontalBox(SideChoice)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));SideChoice->AddOption(TEXT("아군"));SideChoice->AddOption(TEXT("적군"));SideChoice->SetSelectedIndex(0);
   Quantity=InputColumn(Setup,TEXT("수량 1~16"),TEXT("1"));
   PresetChoice=Combo(WidgetTree);Create->AddChildToVerticalBox(PresetChoice);
   PresetChoice->AddOption(TEXT("기본 · 기체 0 / 병력 0"));PresetChoice->AddOption(TEXT("항모 시험 · 격납고당 20기"));PresetChoice->AddOption(TEXT("강습 시험 · 병력 10명"));PresetChoice->SetSelectedIndex(0);
   auto* Auto=Row(WidgetTree,Create);Auto->AddChild(Label(WidgetTree,TEXT("이동 자동  ")));MoveAuto=WidgetTree->ConstructWidget<UCheckBox>();Auto->AddChild(MoveAuto);Auto->AddChild(Label(WidgetTree,TEXT("   사격 자동  ")));FireAuto=WidgetTree->ConstructWidget<UCheckBox>();Auto->AddChild(FireAuto);
   AddButton(WidgetTree,Create,TEXT("+ 생성 · 빈 공간에 배치"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Spawn);
   auto* Placement=Advanced(Body,TEXT("직접 배치 · cm 단위"));auto* XYZ=Row(WidgetTree,Placement);
   X=InputColumn(XYZ,TEXT("X"),TEXT("0"));Y=InputColumn(XYZ,TEXT("Y"),TEXT("0"));Z=InputColumn(XYZ,TEXT("Z"),TEXT("0"));
   auto* Stock=Row(WidgetTree,Placement);Spacing=InputColumn(Stock,TEXT("배치 간격"),TEXT("12000"));Inventory=InputColumn(Stock,TEXT("격납고당 기체"),TEXT("0"));Troops=InputColumn(Stock,TEXT("병력"),TEXT("0"));
   Faction=WidgetTree->ConstructWidget<UEditableTextBox>();Faction->SetText(FText::FromString(TEXT("0")));
   SideChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::SideChanged);PresetChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::PresetChanged);
   auto* Runtime=Section(WidgetTree,Body,TEXT("배치된 함선"));
   RuntimeShips=Combo(WidgetTree);Runtime->AddChildToVerticalBox(RuntimeShips);LogisticsTarget=Combo(WidgetTree);RefreshShips();
   auto* Actions=Row(WidgetTree,Runtime);
   AddButton(WidgetTree,Actions,TEXT("선택"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SelectRuntimeShip);
   AddButton(WidgetTree,Actions,TEXT("추적"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::FollowRuntimeShip);
   AddButton(WidgetTree,Actions,TEXT("선내"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::InteriorRuntimeShip);
   AddButton(WidgetTree,Actions,TEXT("삭제"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Delete);
   RuntimeDetail=Label(WidgetTree,TEXT(""));Runtime->AddChildToVerticalBox(RuntimeDetail);
   auto* Logistics=Advanced(Body,TEXT("유한 자원 시험 공급 · 고급"));Logistics->AddChildToVerticalBox(LogisticsTarget);
   auto* Amount=Row(WidgetTree,Logistics);DevAmount=InputColumn(Amount,TEXT("요청량"),TEXT("100"));DevRange=InputColumn(Amount,TEXT("거리"),TEXT("12000"));DevNode=InputColumn(Amount,TEXT("노드"),TEXT("1"));
   auto* Materials=Row(WidgetTree,Logistics);DevBasic=InputColumn(Materials,TEXT("일반"),TEXT("0"));DevAdvanced=InputColumn(Materials,TEXT("고급"),TEXT("0"));DevRare=InputColumn(Materials,TEXT("최고급"),TEXT("0"));
   auto* Quote=Row(WidgetTree,Logistics);DevQuoteBasic=InputColumn(Quote,TEXT("HP당 일반"),TEXT("1"));DevQuoteAdvanced=InputColumn(Quote,TEXT("HP당 고급"),TEXT("0"));DevQuoteRare=InputColumn(Quote,TEXT("HP당 최고급"),TEXT("0"));
   auto* SupplyActions=Row(WidgetTree,Logistics);
   AddButton(WidgetTree,SupplyActions,TEXT("재고 설정"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SetMaterials);
   AddButton(WidgetTree,SupplyActions,TEXT("노드 생성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::AddNode);
   auto* Jobs=Row(WidgetTree,Logistics);AddButton(WidgetTree,Jobs,TEXT("채굴"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Mine);AddButton(WidgetTree,Jobs,TEXT("수송"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Transfer);AddButton(WidgetTree,Jobs,TEXT("수리"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Repair);
  }
  else
  {
   WeaponSubject=PC?PC->Selected.Get():nullptr;
   auto* Mounts=Section(WidgetTree,Body,TEXT("장착 대상 · ")+ShortShipName(WeaponSubject.Get()));
   MountChoice=Combo(WidgetTree);Mounts->AddChildToVerticalBox(MountChoice);MountIds.Reset();
   if(PC&&PC->Selected&&PC->Selected->Systems&&PC->Selected->Systems->Layout)
    for(const auto& M:PC->Selected->Systems->Layout->WeaponMounts)
    {MountIds.Add(M.WeaponId);MountChoice->AddOption(FString::Printf(TEXT("%s %d · T%d"),WeaponName(M.Type),MountIds.Num(),FMath::RoundToInt(M.Tier)));}
   if(PC&&PC->PointDefense&&WeaponSubject.IsValid())for(const auto& M:PC->PointDefense->GetShipState(WeaponSubject.Get()).Mounts)if(!MountIds.Contains(M.MountId)){MountIds.Add(M.MountId);MountChoice->AddOption(FString::Printf(TEXT("요격 %d"),MountIds.Num()));}
   if(MountIds.Num())MountChoice->SetSelectedIndex(0);MountChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::MountChanged);
   RuntimeDetail=Label(WidgetTree,TEXT(""));Mounts->AddChildToVerticalBox(RuntimeDetail);
   TargetChoice=Combo(WidgetTree);Mounts->AddChildToVerticalBox(TargetChoice);Targets.Reset();
   if(PC)for(const auto& Ship:PC->Ships)if(IsValid(Ship)&&Ship!=WeaponSubject){Targets.Add(Ship.Get());TargetChoice->AddOption(ShortShipName(Ship));}if(Targets.Num())TargetChoice->SetSelectedIndex(0);
   auto* Actions=Row(WidgetTree,Mounts);
   EquipButton=AddButton(WidgetTree,Actions,TEXT("장착 / 교체"));EquipButton->SetIsEnabled(false);EquipButton->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Equip);
   AddButton(WidgetTree,Actions,TEXT("표적 지정"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Target);
   AddButton(WidgetTree,Actions,TEXT("발사 요청"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Fire);
   AddButton(WidgetTree,Mounts,TEXT("선택 함선 다시 읽기"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Weapons);
  }
  RefreshRuntime();return;
 }
 auto* S=GetGameInstance()?GetGameInstance()->GetSubsystem<UOVHeroSubsystem>():nullptr;
 auto* Create=Section(WidgetTree,Body,TEXT("시험 영웅 생성"));
 AddButton(WidgetTree,Create,TEXT("비전투 편성실 열기"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SceneRoster);
 auto* Templates=WidgetTree->ConstructWidget<UWrapBox>();Templates->SetInnerSlotPadding(FVector2D(5,5));Create->AddChildToVerticalBox(Templates);
 const TCHAR* Roles[]={TEXT("근거리"),TEXT("원거리"),TEXT("지원"),TEXT("지휘")};
 for(int32 I=0;I<4;++I)
 {
  auto* Button=WidgetTree->ConstructWidget<UOVDevelopmentItem>();Button->Owner=this;Button->Id=FName(*FString::Printf(TEXT("Role_%d"),I+1));StyleButton(Button);
  auto* Card=WidgetTree->ConstructWidget<UVerticalBox>();Button->AddChild(Card);
  auto* Portrait=WidgetTree->ConstructWidget<UOVPortraitWidget>();Portrait->Configure(I+1);
  auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(115);Size->SetHeightOverride(66);Size->AddChild(Portrait);Card->AddChildToVerticalBox(Size);Card->AddChildToVerticalBox(Label(WidgetTree,Roles[I]));
  Button->OnClicked.AddDynamic(Button,&UOVDevelopmentItem::Pick);Templates->AddChildToWrapBox(Button);
 }
 RoleChoice=Combo(WidgetTree);for(const TCHAR* Name:Roles)RoleChoice->AddOption(Name);RoleChoice->SetSelectedIndex(0);
 auto* Options=Row(WidgetTree,Create);Options->AddChildToHorizontalBox(RoleChoice)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 SideChoice=Combo(WidgetTree);Options->AddChildToHorizontalBox(SideChoice)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));SideChoice->AddOption(TEXT("아군"));SideChoice->AddOption(TEXT("적군"));SideChoice->SetSelectedIndex(HeroFaction);SideChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::SideChanged);
 HeroDestination=Combo(WidgetTree);Options->AddChildToHorizontalBox(HeroDestination)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));HeroDestination->AddOption(TEXT("대기 명단에 생성"));HeroDestination->AddOption(TEXT("선택 함선에 생성"));HeroDestination->SetSelectedIndex(0);
 HeroName=AddInput(WidgetTree,Create,TEXT("이름"),TEXT("시험 영웅"));
 auto* Creation=Row(WidgetTree,Create);AddButton(WidgetTree,Creation,TEXT("+ 영웅 생성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::CreateHero);AddButton(WidgetTree,Creation,TEXT("대표 4명 생성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::CreateFour);
 auto* Roster=Section(WidgetTree,Body,S?FString::Printf(TEXT("보유 영웅 · 배치 %d / 15"),S->CountDeployed(HeroFaction)):TEXT("보유 영웅"));
 Search=AddInput(WidgetTree,Roster,TEXT("이름 검색"),TEXT(""));Search->OnTextChanged.AddDynamic(this,&UOVDevelopmentWidget::SearchChanged);
 List=WidgetTree->ConstructWidget<UScrollBox>();auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(160);Size->AddChild(List);Roster->AddChildToVerticalBox(Size);
 Cards=WidgetTree->ConstructWidget<UWrapBox>();Cards->SetInnerSlotPadding(FVector2D(5,5));List->AddChild(Cards);
 HeroChoice=Combo(WidgetTree);HeroIds.Reset();if(S)for(const auto& H:S->GetHeroes(HeroFaction)){HeroIds.Add(H.HeroId);HeroChoice->AddOption(H.Name);}if(HeroIds.Num())HeroChoice->SetSelectedIndex(0);HeroChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::HeroChanged);
 Detail=Label(WidgetTree,TEXT("보유 영웅 카드를 선택하세요"));Roster->AddChildToVerticalBox(Detail);RefreshList();if(HeroIds.Num())SelectItem(FName(*HeroIds[0].ToString()));
 auto* Actions=Row(WidgetTree,Roster);AddButton(WidgetTree,Actions,TEXT("편성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::DeployHero);AddButton(WidgetTree,Actions,TEXT("해제"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::ReleaseHero);AddButton(WidgetTree,Actions,TEXT("함선 배치"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::SpawnHero);AddButton(WidgetTree,Actions,TEXT("위치 찾기"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::LocateHero);
 auto* Growth=Advanced(Body,TEXT("성장 · 저장 · 유한 시험 공급"));Growth->AddChildToVerticalBox(HeroChoice);
 XPAmount=AddInput(WidgetTree,Growth,TEXT("시험 경험치"),TEXT("120"));AddButton(WidgetTree,Growth,TEXT("경험치 지급"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::GrantXP);
 auto* Skills=Row(WidgetTree,Growth);AddButton(WidgetTree,Skills,TEXT("스킬 1"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Skill0);AddButton(WidgetTree,Skills,TEXT("스킬 2"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Skill1);AddButton(WidgetTree,Skills,TEXT("스킬 3"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Skill2);AddButton(WidgetTree,Skills,TEXT("스킬 4"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Skill3);
 auto* Quote=Row(WidgetTree,Growth);SupportBasic=InputColumn(Quote,TEXT("HP당 일반"),TEXT("1"));SupportAdvanced=InputColumn(Quote,TEXT("HP당 고급"),TEXT("0"));SupportRare=InputColumn(Quote,TEXT("HP당 최고급"),TEXT("0"));SupportCharges=InputColumn(Quote,TEXT("증폭기"),TEXT("1"));AddButton(WidgetTree,Growth,TEXT("유한 수리 시험 설정"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::ConfigureSupport);
 auto* SaveButtons=Row(WidgetTree,Growth);AddButton(WidgetTree,SaveButtons,TEXT("명단 저장"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Save);AddButton(WidgetTree,SaveButtons,TEXT("명단 불러오기"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Load);
}
void UOVDevelopmentWidget::RefreshList()
{
 VisibleDefinitionIds.Reset();CardButtons.Reset();if(!List||!Cards||!GetWorld())return;Cards->ClearChildren();
 const FString Filter=Search?Search->GetText().ToString():FString();
 auto Add=[&](FName Id,const FString& Name,const FString& Subtitle,int32 IconKind,int32 Variant,int32 HeroRole=0)
 {
  auto* Item=WidgetTree->ConstructWidget<UOVDevelopmentItem>();Item->Owner=this;Item->Id=Id;StyleButton(Item);
  auto* Card=WidgetTree->ConstructWidget<UHorizontalBox>();Item->AddChild(Card);
  auto* ArtSize=WidgetTree->ConstructWidget<USizeBox>();ArtSize->SetWidthOverride(78);ArtSize->SetHeightOverride(58);
  if(HeroRole>0){auto* Portrait=WidgetTree->ConstructWidget<UOVPortraitWidget>();Portrait->Configure(HeroRole);ArtSize->AddChild(Portrait);}
  else {auto* Icon=WidgetTree->ConstructWidget<UOVCatalogIcon>();Icon->Kind=IconKind;Icon->Variant=Variant;ArtSize->AddChild(Icon);}
  Card->AddChildToHorizontalBox(ArtSize)->SetPadding(FMargin(4));
  auto* Copy=WidgetTree->ConstructWidget<UVerticalBox>();Card->AddChildToHorizontalBox(Copy)->SetPadding(FMargin(4));
  auto* NameBox=WidgetTree->ConstructWidget<USizeBox>();NameBox->SetWidthOverride(190);NameBox->AddChild(Label(WidgetTree,Name));Copy->AddChildToVerticalBox(NameBox);
  auto* Sub=Label(WidgetTree,Subtitle);auto Font=Sub->GetFont();Font.Size=12;Sub->SetFont(Font);Sub->SetColorAndOpacity(FLinearColor(.55f,.72f,.83f));Copy->AddChildToVerticalBox(Sub);
  Item->OnClicked.AddDynamic(Item,&UOVDevelopmentItem::Pick);Cards->AddChildToWrapBox(Item);CardButtons.Add(Item);
 };
 if(Page==2)
 {
  if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>())for(const auto& H:S->GetHeroes(HeroFaction))
  {
   const FString Id=H.HeroId.ToString();
   if(H.Name.Contains(Filter)||Id.Contains(Filter))
   {const TCHAR* States[]={TEXT("대기"),TEXT("편성"),TEXT("구조 대기"),TEXT("포로"),TEXT("실종"),TEXT("사망")};Add(FName(*Id),H.Name,FString::Printf(TEXT("Lv.%d · %s"),H.Level,States[FMath::Clamp(int32(H.Status),0,5)]),0,1,int32(H.Role));}
  }
  UpdateCardSelection();return;
 }
 auto* C=GetWorld()->GetSubsystem<UOVDevelopmentCatalogue>();if(!C)return;const int32 Category=CategoryChoice?CategoryChoice->GetSelectedIndex():0;
 for(const auto& D:C->GetDefinitions())
 {
  bool Match=false;
  if(Page==3)Match=D.Category==EOVCatalogCategory::Weapon||D.Category==EOVCatalogCategory::PointDefense;
  else if(Page==1)Match=Category==0?D.Category==EOVCatalogCategory::Hull:Category==1?D.Category==EOVCatalogCategory::Fighter:D.Category==EOVCatalogCategory::Pod;
  else Match=Category==0||(Category==1&&D.Category==EOVCatalogCategory::Hull)||(Category==2&&D.Category==EOVCatalogCategory::Fighter)||(Category==3&&D.Category==EOVCatalogCategory::Pod)||(Category==4&&D.Category==EOVCatalogCategory::Weapon)||(Category==5&&D.Category==EOVCatalogCategory::Hero)||(Category==6&&D.Category==EOVCatalogCategory::PointDefense);
  if(FilterSize!=0&&D.SizeClass!=FilterSize)Match=false;
  if(!FilterRole.IsNone()&&!D.RoleTags.Contains(FilterRole))Match=false;
  if(FilterAxis!=INDEX_NONE&&FilterStatus!=INDEX_NONE){const FOVCatalogEvidence* Axes[]={&D.Planning,&D.Data,&D.Generation,&D.Role,&D.AutomatedVerification,&D.DirectInput};if(int32(Axes[FilterAxis]->Status)!=FilterStatus)Match=false;}
  if(!Match||(!D.DefinitionId.ToString().Contains(Filter)&&!D.DisplayName.Contains(Filter))||VisibleDefinitionIds.Contains(D.DefinitionId))continue;
  VisibleDefinitionIds.Add(D.DefinitionId);
  int32 Icon=0;FString Subtitle;
  if(D.Category==EOVCatalogCategory::Hull)Subtitle=SizeName(D.SizeClass);
  else if(D.Category==EOVCatalogCategory::Fighter){Icon=5;Subtitle=TEXT("모함 재고에서 출격");}
  else if(D.Category==EOVCatalogCategory::Pod){Icon=6;Subtitle=TEXT("개인 운송체");}
  else if(D.Category==EOVCatalogCategory::PointDefense){Icon=4;Subtitle=TEXT("자동 요격 · 교체 미지원");}
  else {Icon=D.DisplayName.Contains(TEXT("빔"))?2:D.DisplayName.Contains(TEXT("미사일"))?3:1;Subtitle=TEXT("실제 장착점에 교체");}
  if(D.Data.Status==EOVCatalogStatus::Unavailable)Subtitle=TEXT("미지원 · 기획 미정");
  Add(D.DefinitionId,D.DisplayName,Subtitle,Icon,FMath::Max(1,D.SizeClass));
 }
 UpdateCardSelection();
}
void UOVDevelopmentWidget::UpdateCardSelection()
{
 for(const auto& Button:CardButtons)if(Button)Button->SetBackgroundColor(Button->Id==SelectedId?FLinearColor(.08f,.4f,.53f,1):FLinearColor(.045f,.15f,.23f,1));
}
void UOVDevelopmentWidget::CreateHero()
{
 auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();auto* PC=Cast<AOVController>(GetOwningPlayer());if(!S||!RoleChoice||!HeroName)return;
 const bool bOnShip=HeroDestination&&HeroDestination->GetSelectedIndex()==1;
 if(bOnShip&&(!PC||!PC->Selected||!PC->Selected->Systems||PC->Selected->Systems->FactionId!=HeroFaction))
 {Reply(TEXT("같은 세력의 함선을 먼저 선택하세요"));return;}
 const FGuid Id=S->CreateDevHero(HeroFaction,static_cast<EOVInteriorRole>(FMath::Clamp(RoleChoice->GetSelectedIndex()+1,1,4)),HeroName->GetText().ToString());
 FString Message=S->LastMessage;
 if(Id.IsValid())
 {
  Message=TEXT("시험 영웅 생성 · 대기 명단");
  if(bOnShip)
  {
   const bool bReserved=S->SetDeployment(Id,true);
   const int32 Unit=bReserved&&PC->Interior?PC->Interior->SpawnRosterHero(PC->Selected,Id,NAME_None):0;
   Message=Unit>0?TEXT("시험 영웅 생성 · 함선 배치 완료"):TEXT("생성 완료 · 배치 실패: ")+(bReserved&&PC->Interior?PC->Interior->LastMessage:S->LastMessage);
  }
 }
 Rebuild();if(Id.IsValid())SelectItem(FName(*Id.ToString()));Reply(Message);
}
void UOVDevelopmentWidget::LocateHero()
{
 auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();auto* PC=Cast<AOVController>(GetOwningPlayer());if(!S||!PC||!PC->Interior)return;
 const auto H=S->GetHero(HeroId());const auto U=PC->Interior->GetUnit(H.RuntimeUnitId);
 if(!U.UnitId||!U.Ship.IsValid()){Reply(TEXT("전장 위치 없음 · 대기 또는 손실 상태"));return;}
 PC->SelectShip(U.Ship.Get());PC->Action(TEXT("Focus"));
 if(auto* HUD=Cast<AOVHUD>(PC->GetHUD()))HUD->HandleInteriorAction(HUD->bInteriorOpen?FName(TEXT("InteriorSelectedShip")):FName(TEXT("InteriorToggle")));
 Reply(TEXT("영웅이 있는 함선으로 이동"));
}
void UOVDevelopmentWidget::DeployHero(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SetDeployment(HeroId(),true);Reply(S->LastMessage);SelectItem(FName(*HeroId().ToString()));}}
void UOVDevelopmentWidget::ReleaseHero(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SetDeployment(HeroId(),false);Reply(S->LastMessage);SelectItem(FName(*HeroId().ToString()));}}
void UOVDevelopmentWidget::Spend(int32 I){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SpendSkillPoint(HeroId(),I);Reply(S->LastMessage);SelectItem(FName(*HeroId().ToString()));}}
void UOVDevelopmentWidget::Skill0(){Spend(0);}void UOVDevelopmentWidget::Skill1(){Spend(1);}void UOVDevelopmentWidget::Skill2(){Spend(2);}void UOVDevelopmentWidget::Skill3(){Spend(3);}
void UOVDevelopmentWidget::Save(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SaveRoster(TEXT("OVHeroRoster"),0);Reply(S->LastMessage);}}
void UOVDevelopmentWidget::Load(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->LoadRoster(TEXT("OVHeroRoster"),0);const FString Message=S->LastMessage;Rebuild();Reply(Message);}}

void UOVDevelopmentWidget::Spawn()
{
 auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;auto* PC=Cast<AOVController>(GetOwningPlayer());
 if(!S||!PC||!Quantity||!Faction||!Inventory||!Troops||!X||!Y||!Z||!Spacing||!MoveAuto||!FireAuto)return;
 int32 Count=0,Side=0,Aircraft=0,People=0;float PX=0,PY=0,PZ=0,Gap=0;
 if(SelectedId.IsNone()||!LexTryParseString(Count,*Quantity->GetText().ToString())||!LexTryParseString(Side,*Faction->GetText().ToString())||!LexTryParseString(Aircraft,*Inventory->GetText().ToString())||!LexTryParseString(People,*Troops->GetText().ToString())||!LexTryParseString(PX,*X->GetText().ToString())||!LexTryParseString(PY,*Y->GetText().ToString())||!LexTryParseString(PZ,*Z->GetText().ToString())||!LexTryParseString(Gap,*Spacing->GetText().ToString())||!FMath::IsFinite(PX)||!FMath::IsFinite(PY)||!FMath::IsFinite(PZ)||!FMath::IsFinite(Gap)||Count<1||Count>16)
 {Reply(TEXT("함종과 수량 1~16을 확인하세요"));return;}
 FVector Position(PX,PY,PZ);
 if(!bAdvanced)
 {
  // A conservative UI placement proposal; SpawnShips remains the authority for all actual collision/range checks.
  const double Radius=FMath::Max(1000.,OVCalibration::GetHullGeometryScale(SelectedId)*250.);
  Gap=FMath::Max(5000.,Radius*2+1000.);bool Found=false;
  FVector Center=PC->Selected?PC->Selected->GetActorLocation():PC->CameraFocus;
  Center.Z=FMath::Clamp(Center.Z,OVCalibration::MinZ,OVCalibration::MaxZ);
  for(int32 Ring=1;Ring<=12&&!Found;++Ring)for(int32 Direction=0;Direction<8&&!Found;++Direction)
  {
   const double Angle=Direction*PI/4.;
   const FVector Candidate=Center+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Gap*Ring;
   bool Clear=true;
   for(int32 I=0;I<Count&&Clear;++I)
   {
    const FVector Point=Candidate+FVector(0,Gap*I,0);
    if(FMath::Abs(Point.X)>OVCalibration::XYLimit||FMath::Abs(Point.Y)>OVCalibration::XYLimit){Clear=false;break;}
    for(const auto& Other:PC->Ships)if(IsValid(Other)&&!Other->IsHidden()&&Other->Systems&&Other->Systems->Layout)
    {
     const double OtherRadius=(Other->Systems->Layout->LocalHalfExtent*Other->GetActorScale3D()).Size();
     if(FVector::Dist(Point,Other->GetActorLocation())<Radius+OtherRadius+500){Clear=false;break;}
    }
   }
   if(Clear){Position=Candidate;Found=true;}
  }
  if(!Found){Reply(TEXT("배치할 빈 공간 없음 · 고급에서 위치를 지정하세요"));return;}
 }
 const auto Created=S->SpawnShips(SelectedId,Side,Count,Position,Gap,Aircraft,People,MoveAuto->IsChecked(),FireAuto->IsChecked());
 RefreshShips();
 if(Created.Num())
 {
  const int32 Index=RuntimeShipIds.IndexOfByPredicate([&](const auto& Ship){return Ship.Get()==Created.Last();});
  if(RuntimeShips&&Index!=INDEX_NONE)RuntimeShips->SetSelectedIndex(Index);
  PC->Action(TEXT("Focus"));
 }
 Reply(Created.Num()==Count?FString::Printf(TEXT("%d척 생성 · 선택 및 추적"),Count):S->LastMessage);RefreshRuntime();
}
void UOVDevelopmentWidget::Delete(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){S->DeleteShip(PC->Selected);Reply(S->LastMessage);RefreshShips();}}
void UOVDevelopmentWidget::Equip(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=MountChoice->GetSelectedIndex();S->EquipWeapon(WeaponSubject.Get(),MountIds.IsValidIndex(I)?MountIds[I]:NAME_None,SelectedId);Reply(S->LastMessage);RefreshRuntime();}}
void UOVDevelopmentWidget::Target(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=TargetChoice->GetSelectedIndex();S->SetShipTarget(WeaponSubject.Get(),Targets.IsValidIndex(I)?Targets[I].Get():nullptr);Reply(S->LastMessage);RefreshRuntime();}}
void UOVDevelopmentWidget::Fire(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=MountChoice->GetSelectedIndex();S->FireWeapon(WeaponSubject.Get(),MountIds.IsValidIndex(I)?MountIds[I]:NAME_None);Reply(S->LastMessage);RefreshRuntime();}}
void UOVDevelopmentWidget::SpawnHero(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(PC&&PC->Interior){PC->Interior->SpawnRosterHero(PC->Selected,HeroId(),NAME_None);Reply(PC->Interior->LastMessage);}else if(Feedback)Reply(TEXT("실제 배치 불가: 선내 시스템을 사용할 수 없습니다."));}

void UOVDevelopmentWidget::CreateFour(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){int32 Created=0;const TCHAR* Names[]={TEXT("근거리 시험 영웅"),TEXT("원거리 시험 영웅"),TEXT("지원 시험 영웅"),TEXT("지휘 시험 영웅")};for(int32 I=0;I<4;++I)if(S->CreateDevHero(HeroFaction,static_cast<EOVInteriorRole>(I+1),Names[I]).IsValid())++Created;const FString Message=FString::Printf(TEXT("%d/4명 생성 · %s"),Created,*S->LastMessage);Rebuild();Reply(Message);}}
void UOVDevelopmentWidget::GrantXP(){auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();if(!S)return;int32 Amount=0;if(!LexTryParseString(Amount,*XPAmount->GetText().ToString())){Reply(TEXT("시험 XP는 정수로 입력하세요."));return;}S->GrantDevXP(HeroId(),Amount);Reply(S->LastMessage);SelectItem(FName(*HeroId().ToString()));}

void UOVDevelopmentWidget::RefreshRuntime()
{
 if(!RuntimeDetail)return;auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC){RuntimeDetail->SetText(FText::FromString(TEXT("플레이어 없음")));return;}
 if(!bAdvanced)
 {
  FString Brief;
  if(Page==1)
  {
   const auto* Ship=HullCategory==0?RuntimeShip():PC->Selected.Get();Brief=ShortShipName(Ship);
   if(HullCategory==1&&PC->Carrier)
   {
    int32 Stored=0,Flying=0,Lost=0;
    for(const auto& A:PC->Carrier->Aircraft)if(A.Carrier.Get()==Ship){if(A.State==EOVAircraftState::Stored)++Stored;else if(A.State==EOVAircraftState::Wrecked)++Lost;else ++Flying;}
    Brief+=FString::Printf(TEXT(" · 보관 %d / 비행 %d / 손실 %d"),Stored,Flying,Lost);
   }
   else if(HullCategory==2&&PC->Transport)
   {
    int32 Aboard=0,Flying=0;
    for(const auto& Person:PC->Transport->Persons)if(Person.CurrentShip.Get()==Ship&&Person.State==EOVPassengerState::Aboard)++Aboard;
    for(const auto& Pod:PC->Transport->Pods)if(Pod.Origin.Get()==Ship&&(Pod.State==EOVPodFlightState::Outbound||Pod.State==EOVPodFlightState::Returning))++Flying;
    Brief+=FString::Printf(TEXT(" · 탑승 %d / 비행 포드 %d"),Aboard,Flying);
   }
  }
  else if(Page==3)
  {
   auto* Ship=WeaponSubject.Get();const int32 I=MountChoice?MountChoice->GetSelectedIndex():INDEX_NONE;const FName Id=MountIds.IsValidIndex(I)?MountIds[I]:NAME_None;
   Brief=TEXT("함선과 장착점을 선택하세요");
   if(Ship&&PC->Combat&&!Id.IsNone())
   {
    const auto State=PC->Combat->GetShipState(Ship);
    if(const auto* Mount=State.Mounts.FindByPredicate([Id](const FOVCombatMountState& M){return M.MountId==Id;}))
    {
     const auto Ready=PC->Combat->GetReadiness(Ship,Id);
     Brief=FString::Printf(TEXT("%s T%d · %s · 대기 %.1f초 · 발사 %d"),WeaponName(Mount->Profile.Family),Mount->Profile.Tier,*ReadyLabel(Ready.Reason),Ready.Cooldown,Mount->ShotsFired);
    }
    else if(PC->PointDefense)Brief=TEXT("자동 요격 · ")+ReadyLabel(PC->PointDefense->GetReadiness(Ship));
   }
  }
  RuntimeDetail->SetText(FText::FromString(Brief));return;
 }
 FString Text;
 if(Page==1){auto* Ship=HullCategory==0?RuntimeShip():PC->Selected.Get();Text=TEXT("선택 함선: ")+ShortShipName(Ship);if(HullCategory==0){if(auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>()){for(const auto& C:L->Cargo)if(C.Ship==Ship)Text+=FString::Printf(TEXT("\n재고 일반 %.1f / 고급 %.1f / 최고급 %.1f / 적재한도 %.1f"),C.Amount,C.Advanced,C.Rare,C.Capacity);for(const auto& N:L->Nodes)Text+=FString::Printf(TEXT("\n노드 #%d · 잔량 %.1f"),N.Id,N.Amount);for(const auto& Link:L->RepairLinks)if(Link.Source==Ship)Text+=TEXT("\n수리: ")+Link.State;}}else if(HullCategory==1&&PC->Carrier){Text+=TEXT("\n")+PC->Carrier->GetCarrierStatus(Ship);for(const auto& A:PC->Carrier->Aircraft)if(A.Carrier.Get()==Ship)Text+=FString::Printf(TEXT("\n기체 #%d · 상태 %s · HP %.0f"),A.AircraftId,AircraftState(A.State),A.HP);}else if(HullCategory==2&&PC->Transport){Text+=TEXT("\n표적: ")+(Ship&&Ship->AttackTarget?Ship->AttackTarget->ShipName:TEXT("없음"));for(const auto& Person:PC->Transport->Persons)if(Person.CurrentShip.Get()==Ship)Text+=FString::Printf(TEXT("\n인원 #%d / 유닛 #%d · 상태 %s"),Person.PersonId,Person.RuntimeUnitId,PassengerState(Person.State));for(const auto& P:PC->Transport->Pods)if(P.Origin.Get()==Ship)Text+=FString::Printf(TEXT("\n포드 #%d / 인원 #%d · %s · HP %.0f"),P.PodId,P.PersonId,PodState(P.State),P.HP);}}
 else if(Page==3){AOVShip* Ship=WeaponSubject.Get();const int32 I=MountChoice?MountChoice->GetSelectedIndex():INDEX_NONE;const FName Id=MountIds.IsValidIndex(I)?MountIds[I]:NAME_None;Text=TEXT("장착점 상태: 선택 함선/장착점 없음");if(Ship&&PC->Combat&&Id!=NAME_None){const auto State=PC->Combat->GetShipState(Ship);if(const auto* M=State.Mounts.FindByPredicate([Id](const FOVCombatMountState& V){return V.MountId==Id;})){const auto R=PC->Combat->GetReadiness(Ship,Id);const auto* HP=Ship->Systems?Ship->Systems->WeaponMountStates.FindByPredicate([Id](const FOVWeaponMountState& V){return V.WeaponMountId==Id;}):nullptr;int32 RecentHits=0;for(const auto& E:PC->Combat->RecentEvents)if(E.Attacker.Get()==Ship&&E.WeaponId==Id&&E.Result==EOVAttackResult::Hit)++RecentHits;Text=FString::Printf(TEXT("%s / %s · HP %.0f\n발사 %d · 최근 이벤트 명중 %d (최대 128건)\n준비: %s · 재사용 %.2f초"),*Id.ToString(),*M->Profile.ProfileId.ToString(),HP?HP->HP:0,M->ShotsFired,RecentHits,*ReadyLabel(R.Reason),R.Cooldown);}}
  if(Ship&&PC->PointDefense&&Id!=NAME_None){const auto PD=PC->PointDefense->GetShipState(Ship);if(const auto* M=PD.Mounts.FindByPredicate([Id](const FOVPDMountState& V){return V.MountId==Id;})){int32 RecentHits=0;for(const auto& E:PC->PointDefense->RecentEvents)if(E.DefenderId==Ship->GetFName()&&E.MountId==Id&&E.bHit)++RecentHits;Text=FString::Printf(TEXT("요격 %s / %s · HP %.0f\n발사 %d · 최근 이벤트 명중 %d\n%s · 재사용 %.2f초 · 자동 요격"),*Id.ToString(),*M->Profile.ProfileId.ToString(),M->HP,M->ShotCount,RecentHits,*ReadyLabel(PC->PointDefense->GetReadiness(Ship)),M->Cooldown);}}
 }
 RuntimeDetail->SetText(FText::FromString(Text));
}
void UOVDevelopmentWidget::LaunchAircraft(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!PC->Carrier)return;const int32 Count=PC->Carrier->Launch(PC->Selected);Reply(FString::Printf(TEXT("출격 %d기 · %s"),Count,*PC->Carrier->GetCarrierStatus(PC->Selected)));RefreshRuntime();}
void UOVDevelopmentWidget::RecallAircraft(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!PC->Carrier)return;PC->Carrier->Recall(PC->Selected);Reply(PC->Carrier->GetCarrierStatus(PC->Selected));RefreshRuntime();}
void UOVDevelopmentWidget::LaunchPods(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!PC->Transport)return;AOVShip* Ship=PC->Selected.Get();PC->Transport->LaunchPods(Ship,Ship?Ship->AttackTarget.Get():nullptr);Reply(PC->Transport->LastMessage);RefreshRuntime();}
void UOVDevelopmentWidget::RecallPods(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!PC->Transport)return;PC->Transport->RecallPods(PC->Selected);Reply(PC->Transport->LastMessage);RefreshRuntime();}

void UOVDevelopmentWidget::RefreshShips(){if(!RuntimeShips||!LogisticsTarget)return;AOVShip* Previous=RuntimeShip();RuntimeShips->ClearOptions();LogisticsTarget->ClearOptions();RuntimeShipIds.Reset();if(auto* PC=Cast<AOVController>(GetOwningPlayer()))for(const auto& S:PC->Ships)if(IsValid(S)){RuntimeShipIds.Add(S.Get());const FString Label=ShortShipName(S)+FString::Printf(TEXT(" · %s %d"),S->bFriendly?TEXT("아군"):TEXT("적군"),RuntimeShipIds.Num());RuntimeShips->AddOption(Label);LogisticsTarget->AddOption(Label);}int32 I=RuntimeShipIds.IndexOfByPredicate([Previous](const auto& S){return S.Get()==Previous;});if(I==INDEX_NONE&&RuntimeShipIds.Num())I=0;if(I!=INDEX_NONE){RuntimeShips->SetSelectedIndex(I);LogisticsTarget->SetSelectedIndex(I);}}
AOVShip* UOVDevelopmentWidget::RuntimeShip() const{const int32 I=RuntimeShips?RuntimeShips->GetSelectedIndex():INDEX_NONE;return RuntimeShipIds.IsValidIndex(I)?RuntimeShipIds[I].Get():nullptr;}
void UOVDevelopmentWidget::SelectRuntimeShip(){if(auto* PC=Cast<AOVController>(GetOwningPlayer())){auto* Ship=RuntimeShip();if(!Ship){Reply(TEXT("실행 함선을 선택하세요."));return;}PC->SelectShip(Ship);Reply(TEXT("함선 선택: ")+ShortShipName(Ship));RefreshRuntime();}}
void UOVDevelopmentWidget::FollowRuntimeShip(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(!PC||!RuntimeShip()){if(Feedback)Reply(TEXT("실행 함선을 선택하세요."));return;}PC->SelectShip(RuntimeShip());PC->Action(TEXT("Focus"));Reply(PC->bFollow?TEXT("함선 추적 중"):TEXT("추적 불가: 함선 상태를 확인하세요."));}
void UOVDevelopmentWidget::InteriorRuntimeShip(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* Ship=RuntimeShip();if(!PC||!Ship||!PC->Interior||!PC->Interior->bEnabled){if(Feedback)Reply(TEXT("선내 지도 불가: 함선 및 선내 시스템 확인"));return;}PC->SelectShip(Ship);if(auto* H=Cast<AOVHUD>(PC->GetHUD())){H->HandleInteriorAction(H->bInteriorOpen?FName(TEXT("InteriorSelectedShip")):FName(TEXT("InteriorToggle")));Reply(TEXT("선내 지도 요청"));}}
namespace { bool DevNumber(UEditableTextBox* Input,float& Value){return Input&&LexTryParseString(Value,*Input->GetText().ToString())&&FMath::IsFinite(Value);} }
void UOVDevelopmentWidget::SetMaterials(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();float B=0,A=0,R=0;if(!L||!DevNumber(DevBasic,B)||!DevNumber(DevAdvanced,A)||!DevNumber(DevRare,R)){Reply(TEXT("재고 숫자를 확인하세요."));return;}const bool Ok=L->SetDevMaterials(RuntimeShip(),FVector(B,A,R));Reply(Ok?TEXT("DEV 재고 설정 완료"):TEXT("재고 설정 불가: 등록 함선·양·적재한도 확인"));RefreshRuntime();}
void UOVDevelopmentWidget::AddNode(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();auto* Ship=RuntimeShip();float Amount=0;if(!L||!Ship||!DevNumber(DevAmount,Amount)){Reply(TEXT("함선과 노드량을 확인하세요."));return;}const int32 Id=L->AddDevNode(Ship->GetActorLocation(),Amount);if(Id)DevNode->SetText(FText::AsNumber(Id));Reply(Id?FString::Printf(TEXT("DEV 자원 노드 #%d 생성"),Id):TEXT("노드 생성 불가: 수량 확인"));RefreshRuntime();}
void UOVDevelopmentWidget::Mine(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();float Amount=0,Range=0;int32 Id=0;if(!L||!DevNumber(DevAmount,Amount)||!DevNumber(DevRange,Range)||!LexTryParseString(Id,*DevNode->GetText().ToString())){Reply(TEXT("채굴 입력을 확인하세요."));return;}const float Done=L->DevMine(RuntimeShip(),Id,Amount,Range);Reply(FString::Printf(TEXT("%.1f 채굴 · %s"),Done,*L->LastReason));RefreshRuntime();}
void UOVDevelopmentWidget::Transfer(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();float Amount=0,Range=0;const int32 I=LogisticsTarget?LogisticsTarget->GetSelectedIndex():INDEX_NONE;if(!L||!DevNumber(DevAmount,Amount)||!DevNumber(DevRange,Range)){Reply(TEXT("수송 입력을 확인하세요."));return;}const float Done=L->DevTransfer(RuntimeShip(),RuntimeShipIds.IsValidIndex(I)?RuntimeShipIds[I].Get():nullptr,Amount,Range);Reply(FString::Printf(TEXT("%.1f 수송 · %s"),Done,*L->LastReason));RefreshRuntime();}
void UOVDevelopmentWidget::Repair(){auto* L=GetWorld()->GetSubsystem<UOVLogisticsSubsystem>();float B=0,A=0,R=0,Range=0;const int32 I=LogisticsTarget?LogisticsTarget->GetSelectedIndex():INDEX_NONE;if(!L||!DevNumber(DevQuoteBasic,B)||!DevNumber(DevQuoteAdvanced,A)||!DevNumber(DevQuoteRare,R)||!DevNumber(DevRange,Range)){Reply(TEXT("수리 견적과 거리를 확인하세요."));return;}L->StartDevRepair(RuntimeShip(),RuntimeShipIds.IsValidIndex(I)?RuntimeShipIds[I].Get():nullptr,false,FGuid(),FVector(B,A,R),Range,0,0,0);Reply(L->LastReason);RefreshRuntime();}

void UOVDevelopmentWidget::ConfigureSupport(){auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();float B=0,A=0,R=0;int32 Charges=0;if(!S||!DevNumber(SupportBasic,B)||!DevNumber(SupportAdvanced,A)||!DevNumber(SupportRare,R)||!SupportCharges||!LexTryParseString(Charges,*SupportCharges->GetText().ToString())){if(Feedback)Reply(TEXT("DEV 견적과 충전 횟수를 확인하세요."));return;}S->ConfigureDevSupportRepair(HeroId(),FVector(B,A,R),Charges);Reply(S->LastMessage);SelectItem(FName(*HeroId().ToString()));}
