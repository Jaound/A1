#include "OVHeroWidget.h"
#include "OVHeroSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVController.h"
#include "OVHUD.h"
#include "OVDevelopmentSubsystem.h"
#include "OVMissionSubsystem.h"
#include "OVShipSystemsComponent.h"
#include "OVShipLayout.h"
#include "OVShip.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace {
UTextBlock* Text(UWidgetTree* Tree, const FString& Value) { auto* T=Tree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Value)); T->SetAutoWrapText(true); return T; }
UButton* Button(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Label) { auto* B=Tree->ConstructWidget<UButton>(); B->AddChild(Text(Tree,Label)); Parent->AddChildToVerticalBox(B); return B; }
FString Role(EOVInteriorRole R) { const TCHAR* V[]={TEXT("병력"),TEXT("근거리"),TEXT("원거리"),TEXT("지원"),TEXT("지휘")};return V[FMath::Clamp(int32(R),0,4)]; }
}
void UOVHeroWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 auto* Border=WidgetTree->ConstructWidget<UBorder>(); Border->SetBrushColor(FLinearColor(.008f,.02f,.04f,.96f)); WidgetTree->RootWidget=Border;
 auto* Outer=WidgetTree->ConstructWidget<UScrollBox>();Border->AddChild(Outer);auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Outer->AddChild(Box);
 Box->AddChildToVerticalBox(Text(WidgetTree,TEXT("아군 영웅 / 병력")));
 Choice=WidgetTree->ConstructWidget<UComboBoxString>(); Box->AddChildToVerticalBox(Choice); Choice->OnSelectionChanged.AddDynamic(this,&UOVHeroWidget::SelectionChanged);
 Roster=WidgetTree->ConstructWidget<UScrollBox>(); auto* RosterSize=WidgetTree->ConstructWidget<USizeBox>();RosterSize->SetHeightOverride(105);RosterSize->AddChild(Roster);Box->AddChildToVerticalBox(RosterSize);
 Detail=Text(WidgetTree,TEXT("영웅을 선택하세요.")); Box->AddChildToVerticalBox(Detail);
 auto* DeployButton=Button(WidgetTree,Box,TEXT("편성 예약"));DeployButton->OnClicked.AddDynamic(this,&UOVHeroWidget::Deploy);
 auto* ReleaseButton=Button(WidgetTree,Box,TEXT("편성 해제"));ReleaseButton->OnClicked.AddDynamic(this,&UOVHeroWidget::Release);
 auto* SpawnButton=Button(WidgetTree,Box,TEXT("선택 함선에 배치"));SpawnButton->OnClicked.AddDynamic(this,&UOVHeroWidget::SpawnHero);
 DutyChoice=WidgetTree->ConstructWidget<UComboBoxString>();Box->AddChildToVerticalBox(DutyChoice);
 auto* DutyOnButton=Button(WidgetTree,Box,TEXT("선택 시설에 보직"));DutyOnButton->OnClicked.AddDynamic(this,&UOVHeroWidget::DutyOn);
 auto* DutyOffButton=Button(WidgetTree,Box,TEXT("보직 해제"));DutyOffButton->OnClicked.AddDynamic(this,&UOVHeroWidget::DutyOff);
 auto* Skills=WidgetTree->ConstructWidget<UHorizontalBox>();Box->AddChildToVerticalBox(Skills);
 auto MakeSkill=[&](const TCHAR* Title){auto* B=WidgetTree->ConstructWidget<UButton>();B->AddChild(Text(WidgetTree,Title));Skills->AddChild(B);return B;};
 MakeSkill(TEXT("스킬 1"))->OnClicked.AddDynamic(this,&UOVHeroWidget::Skill0);MakeSkill(TEXT("스킬 2"))->OnClicked.AddDynamic(this,&UOVHeroWidget::Skill1);MakeSkill(TEXT("스킬 3"))->OnClicked.AddDynamic(this,&UOVHeroWidget::Skill2);MakeSkill(TEXT("스킬 4"))->OnClicked.AddDynamic(this,&UOVHeroWidget::Skill3);
 auto* RoomButton=Button(WidgetTree,Box,TEXT("비전투 편성실"));RoomButton->OnClicked.AddDynamic(this,&UOVHeroWidget::RosterRoom);
 auto* MissionButton=Button(WidgetTree,Box,TEXT("보유 편제로 공격 임무 시작"));MissionButton->OnClicked.AddDynamic(this,&UOVHeroWidget::StartMission);
 auto* InteriorButton=Button(WidgetTree,Box,TEXT("선내 작전 열기"));InteriorButton->OnClicked.AddDynamic(this,&UOVHeroWidget::OpenInterior);
 Feedback=Text(WidgetTree,TEXT("선내 명령은 선내 작전에서 대상과 인원을 선택하세요."));Box->AddChildToVerticalBox(Feedback);
 Refresh();
}
FGuid UOVHeroWidget::SelectedId() const { const int32 I=Choice?Choice->GetSelectedIndex():INDEX_NONE;return ChoiceIds.IsValidIndex(I)?ChoiceIds[I]:FGuid(); }
void UOVHeroWidget::NativeTick(const FGeometry& Geometry,float DeltaSeconds) { Super::NativeTick(Geometry,DeltaSeconds);RefreshElapsed+=DeltaSeconds;if(RefreshElapsed>=1.f){RefreshElapsed=0;Refresh();} }
void UOVHeroWidget::Refresh()
{
 auto* PC=Cast<AOVController>(GetOwningPlayer());auto* Heroes=GetGameInstance()?GetGameInstance()->GetSubsystem<UOVHeroSubsystem>():nullptr;
 if(!PC||!Heroes||!Choice||!Roster)return;
 const auto List=Heroes->GetHeroes(0); FString Signature;for(const auto& H:List)Signature+=H.HeroId.ToString()+H.Name;
 if(Signature!=RosterSignature) { const FGuid Previous=SelectedId();RosterSignature=Signature;Choice->ClearOptions();ChoiceIds.Reset();for(const auto& H:List){ChoiceIds.Add(H.HeroId);Choice->AddOption(H.Name+TEXT(" · ")+Role(H.Role));}const int32 Index=ChoiceIds.IndexOfByKey(Previous);if(Index!=INDEX_NONE)Choice->SetSelectedIndex(Index);else if(ChoiceIds.Num())Choice->SetSelectedIndex(0); }
 TArray<const FOVInteriorUnitRecord*> VisibleUnits;
 if(PC->Interior)for(const auto& Unit:PC->Interior->Units)if(Unit.FactionId==0)VisibleUnits.Add(&Unit);
 const int32 RowCount=FMath::Max(1,VisibleUnits.Num());
 while(UnitRows.Num()<RowCount){auto* Row=Text(WidgetTree,TEXT(""));UnitRows.Add(Row);Roster->AddChild(Row);}
 for(int32 I=0;I<UnitRows.Num();++I){auto* Row=UnitRows[I].Get();Row->SetVisibility(I<RowCount?ESlateVisibility::Visible:ESlateVisibility::Collapsed);if(I>=RowCount)continue;if(!VisibleUnits.Num()){Row->SetText(FText::FromString(TEXT("선내 배치 인원 없음")));continue;}const auto& Unit=*VisibleUnits[I];const FString Location=Unit.Ship.IsValid()?Unit.Ship->ShipName+TEXT(" / ")+Unit.NodeId.ToString():TEXT("위치 미확인");Row->SetText(FText::FromString(FString::Printf(TEXT("%s %s · %s · HP %.0f/%.0f"),Unit.Role==EOVInteriorRole::Trooper?TEXT("■ 병력"):(List.ContainsByPredicate([&](const FOVHeroRecord& Hero){return Hero.RuntimeUnitId==Unit.UnitId;})?TEXT("● 영웅"):TEXT("● 예시 영웅")),*Unit.Name,*Location,Unit.HP,Unit.MaxHP)));Row->SetColorAndOpacity(FSlateColor(Unit.Role==EOVInteriorRole::Trooper?FLinearColor(.65f,.7f,.75f):FLinearColor(.2f,.9f,1)));}
 const FGuid Id=SelectedId();if(!Id.IsValid()){Detail->SetText(FText::FromString(TEXT("영웅 명단이 비어 있습니다.")));return;}
 const auto H=Heroes->GetHero(Id);FString Position=TEXT("현재 위치: 전장 연결 없음 / 미확인");
 FString Duty=TEXT("보직 없음 / 미연결"); AOVShip* UnitShip=nullptr;
 if(PC->Interior&&H.RuntimeUnitId>0){const auto U=PC->Interior->GetUnit(H.RuntimeUnitId);if(U.UnitId==H.RuntimeUnitId){UnitShip=U.Ship.Get();Position=TEXT("현재 위치: ")+(U.Ship.IsValid()?U.Ship->ShipName:TEXT("미확인"))+TEXT(" / ")+U.NodeId.ToString();Duty=U.bDutyActive?TEXT("수행 중: ")+U.DutySlot.ToString():U.bDutyRequested?TEXT("이동/인계 중: ")+U.DutySlot.ToString():TEXT("보직 없음");}}
 if(DutyHero!=Id||DutyShip.Get()!=UnitShip){DutyHero=Id;DutyShip=UnitShip;DutySlots.Reset();DutyChoice->ClearOptions();if(UnitShip&&UnitShip->Systems&&UnitShip->Systems->Layout)for(const auto& D:UnitShip->Systems->Layout->Slots){DutySlots.Add(D.SlotId);DutyChoice->AddOption(D.SlotId.ToString());}if(DutySlots.Num())DutyChoice->SetSelectedIndex(0);}
 FString Ranks;for(int32 Rank:H.SkillRanks)Ranks+=FString::Printf(TEXT(" %d"),Rank);
 const TCHAR* Status[]={TEXT("예비"),TEXT("편성 예약"),TEXT("구조 대기"),TEXT("포로"),TEXT("실종"),TEXT("사망")};
 const TCHAR* L1[]={TEXT("없음"),TEXT("역장 방어"),TEXT("초감각 조준"),TEXT("생체 재구성"),TEXT("신경 공명")};
 Detail->SetText(FText::FromString(FString::Printf(TEXT("%s · %s · %s · Lv%d / XP%d\n스킬 %s · 등급:%s / 잔여 %d\n%s\n보직: %s"),*H.Name,*Role(H.Role),Status[FMath::Clamp(int32(H.Status),0,5)],H.Level,H.XP,L1[FMath::Clamp(int32(H.Role),0,4)],*Ranks,H.SkillPoints,*Position,*Duty)));
}
void UOVHeroWidget::SelectionChanged(FString, ESelectInfo::Type) { Refresh(); }
void UOVHeroWidget::Deploy() { if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SetDeployment(SelectedId(),true);Feedback->SetText(FText::FromString(S->LastMessage));Refresh();} }
void UOVHeroWidget::Release() { if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SetDeployment(SelectedId(),false);Feedback->SetText(FText::FromString(S->LastMessage));Refresh();} }
void UOVHeroWidget::OpenInterior() { if(auto* PC=Cast<AOVController>(GetOwningPlayer()))if(auto* HUD=Cast<AOVHUD>(PC->GetHUD())){if(!PC->Interior||!PC->Interior->bEnabled){Feedback->SetText(FText::FromString(TEXT("현재 장면에서 선내 작전을 사용할 수 없습니다.")));return;}if(!HUD->bInteriorOpen)HUD->HandleInteriorAction(TEXT("InteriorToggle"));} }

void UOVHeroWidget::SpawnHero(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(PC&&PC->Interior){PC->Interior->SpawnRosterHero(PC->Selected,SelectedId(),NAME_None);Feedback->SetText(FText::FromString(PC->Interior->LastMessage));}else Feedback->SetText(FText::FromString(TEXT("선내 시스템을 사용할 수 없습니다.")));Refresh();}
void UOVHeroWidget::DutyOn(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();if(PC&&PC->Interior&&S){const int32 I=DutyChoice->GetSelectedIndex();PC->Interior->SetHeroDuty(S->GetHero(SelectedId()).RuntimeUnitId,true,DutySlots.IsValidIndex(I)?DutySlots[I]:NAME_None);Feedback->SetText(FText::FromString(PC->Interior->LastMessage));}Refresh();}
void UOVHeroWidget::DutyOff(){auto* PC=Cast<AOVController>(GetOwningPlayer());auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();if(PC&&PC->Interior&&S){PC->Interior->SetHeroDuty(S->GetHero(SelectedId()).RuntimeUnitId,false,NAME_None);Feedback->SetText(FText::FromString(PC->Interior->LastMessage));}Refresh();}
void UOVHeroWidget::Spend(int32 I){if(auto* S=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()){S->SpendSkillPoint(SelectedId(),I);Feedback->SetText(FText::FromString(S->LastMessage));}Refresh();}
void UOVHeroWidget::Skill0(){Spend(0);}void UOVHeroWidget::Skill1(){Spend(1);}void UOVHeroWidget::Skill2(){Spend(2);}void UOVHeroWidget::Skill3(){Spend(3);}
void UOVHeroWidget::RosterRoom(){if(auto* S=GetWorld()->GetSubsystem<UOVDevelopmentSubsystem>()){S->OpenRosterRoom();Feedback->SetText(FText::FromString(S->LastMessage));}}
void UOVHeroWidget::StartMission(){auto* PC=Cast<AOVController>(GetOwningPlayer());if(PC&&PC->Mission){PC->Mission->StartRosterMission(PC,false,260112,false);Feedback->SetText(FText::FromString(PC->Mission->Status));}}
