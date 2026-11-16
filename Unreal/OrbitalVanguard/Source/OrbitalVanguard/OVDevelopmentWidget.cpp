#include "OVDevelopmentWidget.h"
#include "OVCalibration.h"
#include "OVDevelopmentSubsystem.h"
#include "OVDevelopmentCatalogue.h"
#include "OVShipSystemsComponent.h"
#include "OVShipLayout.h"
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
FString Evidence(const TCHAR* Name,const FOVCatalogEvidence& E) { const TCHAR* States[]={TEXT("미확인"),TEXT("있음"),TEXT("부분"),TEXT("없음"),TEXT("해당 없음")}; return FString::Printf(TEXT("%s: %s · %s"),Name,States[FMath::Clamp(int32(E.Status),0,4)],*E.Reason); }
UEditableTextBox* AddInput(UWidgetTree* T,UVerticalBox* P,const TCHAR* Hint,const TCHAR* Default) { auto* W=T->ConstructWidget<UEditableTextBox>();W->SetHintText(FText::FromString(Hint));W->SetText(FText::FromString(Default));P->AddChildToVerticalBox(W);return W; }
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
bool UOVDevelopmentWidget::IsEditing() const { return (Search&&Search->HasKeyboardFocus())||(Quantity&&Quantity->HasKeyboardFocus())||(X&&X->HasKeyboardFocus())||(Y&&Y->HasKeyboardFocus())||(Z&&Z->HasKeyboardFocus())||(Spacing&&Spacing->HasKeyboardFocus())||(Faction&&Faction->HasKeyboardFocus())||(Inventory&&Inventory->HasKeyboardFocus())||(Troops&&Troops->HasKeyboardFocus())||(HeroName&&HeroName->HasKeyboardFocus())||(XPAmount&&XPAmount->HasKeyboardFocus()); }
void UOVDevelopmentWidget::Overview(){Page=0;Rebuild();}void UOVDevelopmentWidget::Hulls(){Page=1;Rebuild();}void UOVDevelopmentWidget::Heroes(){Page=2;Rebuild();}void UOVDevelopmentWidget::Weapons(){Page=3;Rebuild();}
void UOVDevelopmentWidget::Close(){if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(PC->bDeveloperMode)PC->Action(TEXT("Developer"));}
void UOVDevelopmentWidget::SearchChanged(const FText&){SelectedId=NAME_None;if(Detail)Detail->SetText(FText::FromString(TEXT("목록에서 항목을 선택하세요.")));RefreshList();}
void UOVDevelopmentWidget::CategoryChanged(FString, ESelectInfo::Type){SelectedId=NAME_None;if(Detail)Detail->SetText(FText::FromString(TEXT("목록에서 항목을 선택하세요.")));RefreshList();}
void UOVDevelopmentWidget::SelectItem(FName Id){SelectedId=Id;if(!Detail)return;FString Text=TEXT("선택 ID: ")+Id.ToString();if(auto* C=GetWorld()->GetSubsystem<UOVDevelopmentCatalogue>())for(const auto& D:C->GetDefinitions())if(D.DefinitionId==Id){Text=D.DisplayName+TEXT(" · ")+Id.ToString()+TEXT("\n")+D.Description+TEXT("\n")+Evidence(TEXT("기획"),D.Planning)+TEXT("\n")+Evidence(TEXT("데이터"),D.Data)+TEXT("\n")+Evidence(TEXT("생성"),D.Generation)+TEXT("\n")+Evidence(TEXT("역할"),D.Role)+TEXT("\n")+Evidence(TEXT("자동 검증"),D.AutomatedVerification)+TEXT("\n")+Evidence(TEXT("직접 조작"),D.DirectInput);break;}Detail->SetText(FText::FromString(Text));}
FGuid UOVDevelopmentWidget::HeroId() const { const int32 I=HeroChoice?HeroChoice->GetSelectedIndex():INDEX_NONE;return HeroIds.IsValidIndex(I)?HeroIds[I]:FGuid(); }
void UOVDevelopmentWidget::Rebuild()
{
 Body->ClearChildren();Search=nullptr;List=nullptr;Detail=nullptr;Feedback=nullptr;HeroChoice=nullptr;RoleChoice=nullptr;CategoryChoice=nullptr;HeroName=nullptr;XPAmount=nullptr;Quantity=nullptr;X=nullptr;Y=nullptr;Z=nullptr;Spacing=nullptr;Faction=nullptr;Inventory=nullptr;Troops=nullptr;MoveAuto=nullptr;FireAuto=nullptr;MountChoice=nullptr;TargetChoice=nullptr;SelectedId=NAME_None;
 auto* PC=Cast<AOVController>(GetOwningPlayer());
 if(Page==0){Body->AddChildToVerticalBox(Label(WidgetTree,FString::Printf(TEXT("개발 도구 · 함선 %d척\n함체 %d종 · 무장 %d종\n원본 지문: %s"),PC?PC->Ships.Num():0,OVCalibration::GetHullIds().Num(),OVCalibration::GetWeaponProfileIds().Num(),*OVCalibration::GetSourceHash())));return;}
 if(Page==1||Page==3){
  Search=AddInput(WidgetTree,Body,TEXT("이름 또는 ID 검색"),TEXT(""));Search->OnTextChanged.AddDynamic(this,&UOVDevelopmentWidget::SearchChanged);
  if(Page==1){CategoryChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(CategoryChoice);CategoryChoice->AddOption(TEXT("함선"));CategoryChoice->AddOption(TEXT("초소형 함재기"));CategoryChoice->AddOption(TEXT("포드"));CategoryChoice->SetSelectedIndex(0);CategoryChoice->OnSelectionChanged.AddDynamic(this,&UOVDevelopmentWidget::CategoryChanged);}
  List=WidgetTree->ConstructWidget<UScrollBox>();auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(150);Size->AddChild(List);Body->AddChildToVerticalBox(Size);
  Detail=Label(WidgetTree,TEXT("목록에서 항목을 선택하세요."));Body->AddChildToVerticalBox(Detail);RefreshList();
  if(Page==1){Quantity=AddInput(WidgetTree,Body,TEXT("수량"),TEXT("1"));X=AddInput(WidgetTree,Body,TEXT("X"),TEXT("0"));Y=AddInput(WidgetTree,Body,TEXT("Y"),TEXT("0"));Z=AddInput(WidgetTree,Body,TEXT("Z"),TEXT("0"));Spacing=AddInput(WidgetTree,Body,TEXT("간격"),TEXT("1000"));Faction=AddInput(WidgetTree,Body,TEXT("소속 (0 또는 1)"),TEXT("0"));Inventory=AddInput(WidgetTree,Body,TEXT("격납고당 기체 재고"),TEXT("0"));Troops=AddInput(WidgetTree,Body,TEXT("병력"),TEXT("0"));
   auto* Toggles=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Toggles);Toggles->AddChild(Label(WidgetTree,TEXT("이동 자동")));MoveAuto=WidgetTree->ConstructWidget<UCheckBox>();Toggles->AddChild(MoveAuto);Toggles->AddChild(Label(WidgetTree,TEXT("사격 자동")));FireAuto=WidgetTree->ConstructWidget<UCheckBox>();Toggles->AddChild(FireAuto);
   AddButton(WidgetTree,Body,TEXT("선택 함종 생성"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Spawn);
   AddButton(WidgetTree,Body,TEXT("전장에서 선택한 함선 삭제"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Delete);
  } else {
   WeaponSubject=PC?PC->Selected.Get():nullptr;
   Body->AddChildToVerticalBox(Label(WidgetTree,TEXT("선택 함선: ")+(PC&&PC->Selected?PC->Selected->ShipName:TEXT("없음"))+TEXT(" · 함선을 바꾸면 무장 탭을 다시 여세요.")));
   MountChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(MountChoice);MountIds.Reset();
   if(PC&&PC->Selected&&PC->Selected->Systems&&PC->Selected->Systems->Layout)for(const auto& M:PC->Selected->Systems->Layout->WeaponMounts){MountIds.Add(M.WeaponId);MountChoice->AddOption(M.WeaponId.ToString());}if(MountIds.Num())MountChoice->SetSelectedIndex(0);
   TargetChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(TargetChoice);Targets.Reset();if(PC)for(const auto& Ship:PC->Ships)if(IsValid(Ship)){Targets.Add(Ship.Get());TargetChoice->AddOption(Ship->ShipName+FString::Printf(TEXT(" [%d]"),Targets.Num()));}if(Targets.Num())TargetChoice->SetSelectedIndex(0);
   AddButton(WidgetTree,Body,TEXT("선택 무장 장착"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Equip);
   AddButton(WidgetTree,Body,TEXT("선택 함선 표적 지정"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Target);
   AddButton(WidgetTree,Body,TEXT("선택 장착점 발사"))->OnClicked.AddDynamic(this,&UOVDevelopmentWidget::Fire);
  }
  Feedback=Label(WidgetTree,TEXT("항목을 선택하세요. 실행 결과와 제한 사유가 여기에 표시됩니다."));Body->AddChildToVerticalBox(Feedback);return;
 }
 auto* S=GetGameInstance()?GetGameInstance()->GetSubsystem<UOVHeroSubsystem>():nullptr;
 HeroChoice=WidgetTree->ConstructWidget<UComboBoxString>();Body->AddChildToVerticalBox(HeroChoice);HeroIds.Reset();if(S)for(const auto& H:S->GetHeroes(0)){HeroIds.Add(H.HeroId);HeroChoice->AddOption(H.Name+TEXT(" · ")+H.HeroId.ToString(EGuidFormats::Short));}if(HeroIds.Num())HeroChoice->SetSelectedIndex(0);
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
void UOVDevelopmentWidget::RefreshList(){if(!List||!GetWorld())return;List->ClearChildren();auto* C=GetWorld()->GetSubsystem<UOVDevelopmentCatalogue>();if(!C)return;const FString Filter=Search?Search->GetText().ToString():FString();const int32 Category=CategoryChoice?CategoryChoice->GetSelectedIndex():0;for(const auto& D:C->GetDefinitions()){const bool Match=Page==3?D.Category==EOVCatalogCategory::Weapon:(Category==0?D.Category==EOVCatalogCategory::Hull:Category==1?D.Category==EOVCatalogCategory::Fighter:D.Category==EOVCatalogCategory::Pod);if(!Match||(!D.DefinitionId.ToString().Contains(Filter)&&!D.DisplayName.Contains(Filter)))continue;auto* Item=WidgetTree->ConstructWidget<UOVDevelopmentItem>();Item->Owner=this;Item->Id=D.DefinitionId;Item->AddChild(Label(WidgetTree,D.DisplayName+TEXT(" · ")+D.DefinitionId.ToString()));Item->OnClicked.AddDynamic(Item,&UOVDevelopmentItem::Pick);List->AddChild(Item);}}
void UOVDevelopmentWidget::CreateHero(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->CreateDevHero(0,static_cast<EOVInteriorRole>(FMath::Clamp(RoleChoice->GetSelectedIndex()+1,1,4)),HeroName->GetText().ToString());const FString Message=S->LastMessage;Rebuild();Feedback->SetText(FText::FromString(Message));}}
void UOVDevelopmentWidget::DeployHero(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SetDeployment(HeroId(),true);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::ReleaseHero(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SetDeployment(HeroId(),false);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::Spend(int32 I){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SpendSkillPoint(HeroId(),I);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::Skill0(){Spend(0);}void UOVDevelopmentWidget::Skill1(){Spend(1);}void UOVDevelopmentWidget::Skill2(){Spend(2);}void UOVDevelopmentWidget::Skill3(){Spend(3);}
void UOVDevelopmentWidget::Save(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SaveRoster(TEXT("OVHeroRoster"),0);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::Load(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->LoadRoster(TEXT("OVHeroRoster"),0);const FString Message=S->LastMessage;Rebuild();Feedback->SetText(FText::FromString(Message));}}

void UOVDevelopmentWidget::Spawn()
{
 auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(!S||!Feedback)return;
 int32 Count=0,Side=0,Aircraft=0,People=0;float PX=0,PY=0,PZ=0,Gap=0;
 if(SelectedId.IsNone()||!LexTryParseString(Count,*Quantity->GetText().ToString())||!LexTryParseString(Side,*Faction->GetText().ToString())||!LexTryParseString(Aircraft,*Inventory->GetText().ToString())||!LexTryParseString(People,*Troops->GetText().ToString())||!LexTryParseString(PX,*X->GetText().ToString())||!LexTryParseString(PY,*Y->GetText().ToString())||!LexTryParseString(PZ,*Z->GetText().ToString())||!LexTryParseString(Gap,*Spacing->GetText().ToString())||!FMath::IsFinite(PX)||!FMath::IsFinite(PY)||!FMath::IsFinite(PZ)||!FMath::IsFinite(Gap)){Feedback->SetText(FText::FromString(TEXT("생성 불가: 함종과 올바른 숫자를 입력하세요.")));return;}
 S->SpawnShips(SelectedId,Side,Count,FVector(PX,PY,PZ),Gap,Aircraft,People,MoveAuto->IsChecked(),FireAuto->IsChecked());Feedback->SetText(FText::FromString(S->LastMessage));
}
void UOVDevelopmentWidget::Delete(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){S->DeleteShip(PC->Selected);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::Equip(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=MountChoice->GetSelectedIndex();S->EquipWeapon(WeaponSubject.Get(),MountIds.IsValidIndex(I)?MountIds[I]:NAME_None,SelectedId);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::Target(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=TargetChoice->GetSelectedIndex();S->SetShipTarget(WeaponSubject.Get(),Targets.IsValidIndex(I)?Targets[I].Get():nullptr);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::Fire(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetWorld()?GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>():nullptr;if(S&&PC){const int32 I=MountChoice->GetSelectedIndex();S->FireWeapon(WeaponSubject.Get(),MountIds.IsValidIndex(I)?MountIds[I]:NAME_None);Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVDevelopmentWidget::SpawnHero(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(PC&&PC->Interior){PC->Interior->SpawnRosterHero(PC->Selected,HeroId(),NAME_None);Feedback->SetText(FText::FromString(PC->Interior->LastMessage));}else if(Feedback)Feedback->SetText(FText::FromString(TEXT("실제 배치 불가: 선내 시스템을 사용할 수 없습니다.")));}

void UOVDevelopmentWidget::CreateFour(){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){int32 Created=0;const TCHAR* Names[]={TEXT("근거리 시험 영웅"),TEXT("원거리 시험 영웅"),TEXT("지원 시험 영웅"),TEXT("지휘 시험 영웅")};for(int32 I=0;I<4;++I)if(S->CreateDevHero(0,static_cast<EOVInteriorRole>(I+1),Names[I]).IsValid())++Created;const FString Message=FString::Printf(TEXT("%d/4명 생성 · %s"),Created,*S->LastMessage);Rebuild();Feedback->SetText(FText::FromString(Message));}}
void UOVDevelopmentWidget::GrantXP(){auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();if(!S)return;int32 Amount=0;if(!LexTryParseString(Amount,*XPAmount->GetText().ToString())){Feedback->SetText(FText::FromString(TEXT("시험 XP는 정수로 입력하세요.")));return;}S->GrantDevXP(HeroId(),Amount);Feedback->SetText(FText::FromString(S->LastMessage));}
