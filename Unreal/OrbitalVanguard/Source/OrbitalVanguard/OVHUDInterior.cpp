#include "OVHUD.h"
#include "OVController.h"
#include "OVInteriorSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace OVInteriorUI
{
const FLinearColor Ink(.8f,.9f,1), Accent(.2f,.9f,1), Enemy(1,.3f,.15f), Panel(.008f,.02f,.04f,.985f);
FString Facility(EOVFacilityType Type)
{
 const TCHAR* Names[]={TEXT("빈 방"),TEXT("코어"),TEXT("함교"),TEXT("생명 유지"),TEXT("엔진"),TEXT("무기 제어"),TEXT("탄약고"),TEXT("축전실"),TEXT("미사일"),TEXT("쉴드 제어"),TEXT("레이더"),TEXT("요격 제어"),TEXT("포드 발사"),TEXT("격납고"),TEXT("기체 관제"),TEXT("저장소"),TEXT("보안실")};
 return Names[FMath::Clamp(int32(Type),0,16)];
}
FString Role(EOVInteriorRole Type)
{
 const TCHAR* Names[]={TEXT("병력"),TEXT("근거리"),TEXT("원거리"),TEXT("지원"),TEXT("지휘")};
 return Names[FMath::Clamp(int32(Type),0,4)];
}
FString State(EOVInteriorState Value)
{
 const TCHAR* Names[]={TEXT("탑승"),TEXT("이동"),TEXT("교전"),TEXT("작업"),TEXT("수호"),TEXT("철수"),TEXT("운송"),TEXT("회수 대기"),TEXT("구조 대기"),TEXT("사망"),TEXT("이동 불가")};
 return Names[FMath::Clamp(int32(Value),0,10)];
}
bool Living(const FOVInteriorUnitRecord& U) { return U.HP>0 && U.State!=EOVInteriorState::Dead; }
}

FBox2D AOVHUD::InteriorBounds() const
{
 const double Width=ViewportPixels.X/UIScale,Height=ViewportPixels.Y/UIScale;
 const FVector2D Size(FMath::Min(760.,Width-32),FMath::Min(590.,Height-240));
 const FVector2D Position(Width-Size.X-16,104);
 return FBox2D(Position,Position+Size);
}

bool AOVHUD::IsOverInteriorPanel(float X,float Y) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 return bInteriorOpen && PC && PC->Interior && PC->Interior->bEnabled && InteriorBounds().IsInsideOrOn(FVector2D(X/UIScale,Y/UIScale));
}

bool AOVHUD::HandleInteriorAction(FName Action)
{
 const FString Name=Action.ToString();
 if(!Name.StartsWith(TEXT("Interior"))) return false;
 auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!PC->Interior||!PC->Interior->bEnabled) { bInteriorOpen=false;return true; }
 auto* Interior=PC->Interior.Get();
 auto ViewShip=[&](AOVShip* Ship) { InteriorShip=Ship;InteriorDeck=1;InteriorTargetSlot=NAME_None;InteriorUnitPage=0;InteriorSelectedUnits.Reset(); };
 if(Action==TEXT("InteriorToggle")||Action==TEXT("InteriorMissionToggle")) { bInteriorOpen=!bInteriorOpen;if(bInteriorOpen)ViewShip(PC->Selected);return true; }
 if(Action==TEXT("InteriorClose")) { bInteriorOpen=false;return true; }
 if(Action==TEXT("InteriorBackground"))return true;
 if(!bInteriorOpen)return true;
 if(Action==TEXT("InteriorSelectedShip")) { ViewShip(PC->Selected);return true; }
 if(Action==TEXT("InteriorShipNext"))
 {
  const int32 Start=PC->Ships.IndexOfByPredicate([&](const auto& S){return S.Get()==InteriorShip.Get();});
  for(int32 Offset=1;Offset<=PC->Ships.Num();++Offset)
  {
   auto* Ship=PC->Ships[(Start+Offset+PC->Ships.Num())%PC->Ships.Num()].Get();
   if(Interior->IsStructureKnown(0,Ship)) { ViewShip(Ship);break; }
  }
  return true;
 }
 AOVShip* Ship=InteriorShip.Get();
 if(!IsValid(Ship)||!Ship->Systems||!Ship->Systems->Layout||!Interior->IsStructureKnown(0,Ship))return true;
 if(Name.StartsWith(TEXT("InteriorDeck_"))) { InteriorDeck=FMath::Clamp(FCString::Atoi(*Name.RightChop(13)),1,Ship->Systems->Layout->DeckCount);return true; }
 if(Action==TEXT("InteriorPrev")) { InteriorUnitPage=FMath::Max(0,InteriorUnitPage-1);return true; }
 if(Action==TEXT("InteriorNext")) { ++InteriorUnitPage;return true; }
 if(Action==TEXT("InteriorClear")) { InteriorSelectedUnits.Reset();return true; }
 if(Action==TEXT("InteriorAll"))
 {
  InteriorSelectedUnits.Reset();
  for(const auto& U:Interior->Units) if(U.Ship==Ship&&U.FactionId==0&&OVInteriorUI::Living(U))InteriorSelectedUnits.Add(U.UnitId);
  return true;
 }
 if(Name.StartsWith(TEXT("InteriorUnit_")))
 {
  const int32 Id=FCString::Atoi(*Name.RightChop(13));const auto U=Interior->GetUnit(Id);
  if(U.UnitId==Id&&U.Ship==Ship&&U.FactionId==0)
  { if(InteriorSelectedUnits.Contains(Id))InteriorSelectedUnits.Remove(Id);else InteriorSelectedUnits.Add(Id); }
  return true;
 }
 if(Name.StartsWith(TEXT("InteriorSlot_")))
 {
  const FName Slot(*Name.RightChop(13));
  if(Ship->Systems->Layout->GetSlotIndex(Slot)!=INDEX_NONE)InteriorTargetSlot=Slot;
  return true;
 }
 EOVInteriorOrder Order=EOVInteriorOrder::None;
 if(Action==TEXT("InteriorCapture"))Order=EOVInteriorOrder::Capture;
 else if(Action==TEXT("InteriorSabotage"))Order=EOVInteriorOrder::Sabotage;
 else if(Action==TEXT("InteriorDefend"))Order=EOVInteriorOrder::Defend;
 else if(Action==TEXT("InteriorRetreat"))Order=EOVInteriorOrder::Retreat;
 else if(Action!=TEXT("InteriorCancel"))return true;
 TArray<int32> Authorized;
 for(int32 Id:InteriorSelectedUnits)
 {
  const auto U=Interior->GetUnit(Id);
  if(U.UnitId==Id&&U.FactionId==0&&U.Ship==Ship&&OVInteriorUI::Living(U))Authorized.Add(Id);
 }
 if(Authorized.IsEmpty()) { InteriorFeedback=TEXT("이 함선의 살아 있는 아군을 선택하세요");return true; }
 if(Order!=EOVInteriorOrder::None&&Order!=EOVInteriorOrder::Retreat&&InteriorTargetSlot.IsNone())
 { InteriorFeedback=TEXT("목표 시설이나 방을 선택하세요");return true; }
 Interior->IssueOrder(Authorized,Order,InteriorTargetSlot);
 InteriorFeedback=Interior->LastMessage;
 PC->LastAction=InteriorFeedback;
 return true;
}

void AOVHUD::DrawInterior()
{
 using namespace OVInteriorUI;
 auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!PC->Interior||!PC->Interior->bEnabled) { bInteriorOpen=false;InteriorSelectedUnits.Reset();InteriorShip.Reset();return; }
 if(PC->bDeveloperMode)Button(TEXT("InteriorToggle"),TEXT("선내 작전"),1040,55,120,bInteriorOpen);
 if(!bInteriorOpen)return;
 auto* Interior=PC->Interior.Get();
 if(!InteriorShip.IsValid())InteriorShip=PC->Selected;
 const FBox2D Bounds=InteriorBounds();const float X=Bounds.Min.X,Y=Bounds.Min.Y,W=Bounds.GetSize().X,H=Bounds.GetSize().Y;
 TGuardValue<bool> Layer(bDrawingInterior,true);
 AddHitBox(Bounds.Min*UIScale,Bounds.GetSize()*UIScale,TEXT("InteriorBackground"),true,200);
 DrawRect(Panel,X,Y,W,H);
 AOVShip* Ship=InteriorShip.Get();
 DrawText(TEXT("선내 작전 · ")+(Ship?Ship->ShipName:TEXT("함선 선택")),Accent,X+12,Y+10,GEngine->GetSmallFont());
 Button(TEXT("InteriorClose"),TEXT("닫기"),X+W-66,Y+5,56);
 Button(TEXT("InteriorSelectedShip"),TEXT("선택 함선 보기"),X+12,Y+37,140);
 Button(TEXT("InteriorShipNext"),TEXT("다음 알려진 함선"),X+160,Y+37,150);
 DrawText(Interior->GetSummary(0),Ink,X+320,Y+44,GEngine->GetSmallFont());
 if(!IsValid(Ship)||!Ship->Systems||!Ship->Systems->Layout||!Interior->IsStructureKnown(0,Ship))
 {
  DrawText(TEXT("내부 구조 미관측 · 아군의 실제 진입 후 표시됩니다."),Ink,X+24,Y+110,GEngine->GetSmallFont());
  return;
 }
 const auto* L=Ship->Systems->Layout.Get();InteriorDeck=FMath::Clamp(InteriorDeck,1,L->DeckCount);
 for(int32 D=1;D<=L->DeckCount;++D)Button(FName(*FString::Printf(TEXT("InteriorDeck_%d"),D)),FString::Printf(TEXT("%d층"),D),X+234+(D-1)*64,Y+76,58,InteriorDeck==D);
 Button(TEXT("InteriorAll"),TEXT("아군 전체"),X+12,Y+76,100);
 Button(TEXT("InteriorClear"),TEXT("선택 해제"),X+120,Y+76,100);
 TArray<const FOVInteriorUnitRecord*> Own;
 for(const auto& U:Interior->Units)if(U.Ship==Ship&&U.FactionId==0)Own.Add(&U);
 InteriorUnitPage=FMath::Clamp(InteriorUnitPage,0,FMath::Max(0,(Own.Num()-1)/6));
 for(int32 Row=0;Row<6;++Row)
 {
  const int32 Index=InteriorUnitPage*6+Row;if(!Own.IsValidIndex(Index))break;
  const auto& U=*Own[Index];const float RY=Y+114+Row*38;
  Button(FName(*FString::Printf(TEXT("InteriorUnit_%d"),U.UnitId)),FString::Printf(TEXT("%s %d · %s%s"),*OVInteriorUI::Role(U.Role),U.UnitId,*OVInteriorUI::State(U.State),U.bWounded?TEXT(" · 부상"):TEXT("")),X+12,RY,208,InteriorSelectedUnits.Contains(U.UnitId));
  DrawRect(FLinearColor(.1f,.13f,.17f),X+14,RY+29,202,4);
  DrawRect(Living(U)?Accent:FLinearColor::Gray,X+14,RY+29,202*FMath::Clamp(U.HP/FMath::Max(1.f,U.MaxHP),0.f,1.f),4);
 }
 Button(TEXT("InteriorPrev"),TEXT("이전"),X+12,Y+346,62);
 Button(TEXT("InteriorNext"),TEXT("다음"),X+158,Y+346,62);
 DrawText(FString::Printf(TEXT("%d / %d"),InteriorUnitPage+1,FMath::Max(1,(Own.Num()+5)/6)),Ink,X+86,Y+353,GEngine->GetSmallFont());
 DrawText(FString::Printf(TEXT("선택 %d명 · 인원 클릭으로 추가/해제"),InteriorSelectedUnits.Num()),Ink,X+12,Y+386,GEngine->GetSmallFont());
 const auto* FocusEntry=Own.FindByPredicate([&](const auto* U){return InteriorSelectedUnits.Contains(U->UnitId);});
 const FOVInteriorUnitRecord* Focus=FocusEntry?*FocusEntry:nullptr;
 if(Focus)
 {
  DrawText(Focus->Name,Accent,X+12,Y+408,GEngine->GetSmallFont());
  DrawText(FString::Printf(TEXT("HP %.0f/%.0f · 진행 %.0f%%"),Focus->HP,Focus->MaxHP,Focus->Progress*100),Ink,X+12,Y+428,GEngine->GetSmallFont());
 }
 // Static layout is known; enemy units and room runtime state still need observation.
 auto Project=[&](FVector Local)
 {
  return FVector2D(X+276+(Local.X/L->LocalHalfExtent.X+1)*(W-330)*.5,Y+130+(Local.Y/L->LocalHalfExtent.Y+1)*66);
 };
 for(const auto& Link:L->Connections)
 {
  const int32 AI=L->GetSlotIndex(Link.FromSlot),BI=L->GetSlotIndex(Link.ToSlot);
  if(!L->Slots.IsValidIndex(AI)||!L->Slots.IsValidIndex(BI))continue;
  const auto& A=L->Slots[AI];const auto& B=L->Slots[BI];
  if(A.DeckId!=InteriorDeck&&B.DeckId!=InteriorDeck)continue;
  const FVector2D P=Project(A.LocalCenter),Q=Project(B.LocalCenter);
  if(A.DeckId==B.DeckId)DrawLine(P.X,P.Y,Q.X,Q.Y,FLinearColor(.25f,.4f,.5f),2);
  else DrawText(FString::Printf(TEXT("↕%d층"),A.DeckId==InteriorDeck?B.DeckId:A.DeckId),Accent,P.X+15,P.Y-18,GEngine->GetSmallFont());
 }
 if(Focus)for(int32 I=1;I<Focus->Path.Num();++I)
 {
  const int32 A=L->GetSlotIndex(Focus->Path[I-1]),B=L->GetSlotIndex(Focus->Path[I]);
  if(!L->Slots.IsValidIndex(A)||!L->Slots.IsValidIndex(B)||L->Slots[A].DeckId!=InteriorDeck||L->Slots[B].DeckId!=InteriorDeck)continue;
  const FVector2D P=Project(L->Slots[A].LocalCenter),Q=Project(L->Slots[B].LocalCenter);
  DrawLine(P.X,P.Y,Q.X,Q.Y,FLinearColor(.3f,1,.45f),3);
 }
 for(const auto& Slot:L->Slots)if(Slot.DeckId==InteriorDeck)
 {
  const FVector2D P=Project(Slot.LocalCenter);
  Button(FName(*(TEXT("InteriorSlot_")+Slot.SlotId.ToString())),Facility(Slot.Facility),P.X-35,P.Y-13,70,Slot.SlotId==InteriorTargetSlot);
 }
 for(const auto& U:Interior->Units)if(U.Ship==Ship&&Interior->CanObserve(0,U.UnitId))
 {
  const int32 Node=L->GetSlotIndex(U.NodeId);if(!L->Slots.IsValidIndex(Node)||L->Slots[Node].DeckId!=InteriorDeck)continue;
  const FVector2D P=Project(U.LocalPosition);const FLinearColor C=U.State==EOVInteriorState::Dead?FLinearColor::Gray:U.FactionId==0?Accent:Enemy;
  const float Offset=(U.UnitId%4)*5;
  DrawRect(C,P.X-7+Offset,P.Y+17+(U.UnitId%2)*5,4,4);
  if(U.FactionId!=0)DrawText(TEXT(" 적"),C,P.X+12,P.Y+12,GEngine->GetSmallFont());
 }
 const int32 SlotIndex=L->GetSlotIndex(InteriorTargetSlot);
 if(L->Slots.IsValidIndex(SlotIndex)&&Ship->Systems->Slots.IsValidIndex(SlotIndex))
 {
  const auto& Slot=L->Slots[SlotIndex];const auto& S=Ship->Systems->Slots[SlotIndex];
  DrawText(TEXT("목표: ")+Facility(Slot.Facility)+FString::Printf(TEXT(" · %d층"),Slot.DeckId),Accent,X+234,Y+288,GEngine->GetSmallFont());
  const bool Observed=Ship->Systems->FactionId==0||Own.ContainsByPredicate([&](const auto* U){return Living(*U)&&U->State!=EOVInteriorState::InTransit&&U->State!=EOVInteriorState::RescueWaiting&&U->NodeId==Slot.SlotId;});
  if(Observed)
  {
   DrawText(FString::Printf(TEXT("물리 HP %.0f/%.0f · 운영 인원 %.0f/%.0f"),S.HP,Slot.MaxFacilityHP,S.Crew,Slot.RequiredCrew),Ink,X+234,Y+308,GEngine->GetSmallFont());
   DrawText(S.OccupyingFaction<0?TEXT("점령: 없음 · 운영 인원은 전투 병력과 별도"):S.OccupyingFaction==0?TEXT("점령: 아군 · 파괴/인원 상태는 별도"):TEXT("점령: 적 · 파괴/인원 상태는 별도"),Ink,X+234,Y+328,GEngine->GetSmallFont());
  }
  else DrawText(TEXT("시설 상태 미관측 · 아군 도착 시 확인"),Ink,X+234,Y+312,GEngine->GetSmallFont());
 }
 else DrawText(TEXT("층과 목표 시설을 선택하세요."),Ink,X+234,Y+300,GEngine->GetSmallFont());
 const FName Commands[]={TEXT("InteriorCapture"),TEXT("InteriorSabotage"),TEXT("InteriorDefend"),TEXT("InteriorRetreat"),TEXT("InteriorCancel")};
 const TCHAR* Labels[]={TEXT("점령"),TEXT("공작/파괴"),TEXT("수호"),TEXT("실제 철수"),TEXT("명령 취소")};
 for(int32 I=0;I<5;++I)Button(Commands[I],Labels[I],X+234+I*99,Y+364,92);
 DrawText(TEXT("철수는 원 진입점까지 이동 후 원 포드/강습선으로 회수합니다."),Ink,X+234,Y+401,GEngine->GetSmallFont());
 DrawText(TEXT("쉴드 재가동·수송선 상실은 귀환을 위험하게 할 수 있습니다."),Ink,X+234,Y+422,GEngine->GetSmallFont());
 const FString Feedback=Focus&&!Focus->Reason.IsEmpty()?Focus->Reason:InteriorFeedback;
 DrawText(Feedback.Left(92),FLinearColor(1,.8f,.25f),X+12,Y+H-33,GEngine->GetSmallFont());
 if(PC->bDeveloperMode)DrawText(TEXT("배치 ")+L->LayoutVersion.ToString(),Ink,X+234,Y+H-53,GEngine->GetSmallFont());
}
