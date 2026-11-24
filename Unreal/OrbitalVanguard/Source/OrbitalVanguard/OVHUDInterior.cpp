#include "OVHUD.h"
#include "OVController.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVTransportSubsystem.h"
#include "OVInteriorSubsystem.h"
#include "OVShip.h"
#include "OVShipSystemsComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "OVHeroSubsystem.h"
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
int32 FormationTeam(const UOVPlayScenarioSubsystem* Play,const FOVInteriorUnitRecord& Unit)
{
 if(!Play || Play->ActiveId!=TEXT("HeroArena") || Unit.FactionId!=0)return INDEX_NONE;
 const int32 Key=Unit.SquadId>0?Unit.SquadId:Unit.UnitId;
 for(int32 I=0;I<3;++I)if(Play->ArenaGroups[I].Contains(Key))return I;
 return INDEX_NONE;
}

FString CardName(const FOVInteriorUnitRecord& Unit)
{
 return Unit.Role==EOVInteriorRole::Trooper?FString::Printf(TEXT("일반병 %d"),Unit.UnitId):Unit.Name;
}

}

FBox2D AOVHUD::InteriorBounds() const
{
 const double Width=ViewportPixels.X/UIScale,Height=ViewportPixels.Y/UIScale;
 const FVector2D Size(FMath::Clamp(InteriorWindowSize.X,620.,FMath::Max(620.,Width-32)),FMath::Clamp(InteriorWindowSize.Y,500.,FMath::Max(500.,Height-100)));
 const FVector2D Desired=InteriorWindowPosition.X<0?FVector2D(Width-Size.X-16,90):InteriorWindowPosition;
 const FVector2D Position(FMath::Clamp(Desired.X,0.,FMath::Max(0.,Width-Size.X)),FMath::Clamp(Desired.Y,62.,FMath::Max(62.,Height-Size.Y)));
 return FBox2D(Position,Position+FVector2D(Size.X,bInteriorMinimized?32:Size.Y));
}

bool AOVHUD::IsOverInteriorPanel(float X,float Y) const
{
 const auto* PC=Cast<AOVController>(PlayerOwner);
 return bInteriorOpen && PC && PC->Interior && PC->Interior->bEnabled && InteriorBounds().IsInsideOrOn(FVector2D(X/UIScale,Y/UIScale));
}

bool AOVHUD::HandleInteriorAction(FName Action)
{
 if(Action.ToString().StartsWith(TEXT("InteriorMarker_")))Action=FName(*(TEXT("InteriorUnit_")+Action.ToString().RightChop(15)));
 const FString Name=Action.ToString();
 if(!Name.StartsWith(TEXT("Interior"))) return false;
 auto* PC=Cast<AOVController>(PlayerOwner);
 if(!PC||!PC->Interior||!PC->Interior->bEnabled) { bInteriorOpen=false;return true; }
 auto* Interior=PC->Interior.Get();
 auto ViewShip=[&](AOVShip* Ship) { InteriorShip=Ship;InteriorDeck=1;InteriorTargetSlot=NAME_None;InteriorCardColumn=0;InteriorTeamFilter=INDEX_NONE;InteriorSelectedUnits.Reset(); };
 if(Action==TEXT("InteriorEntryA") || Action==TEXT("InteriorEntryB")){
  const auto* Play=PC->GetPlayScenario();
  if(!bInteriorOpen || !Play || Play->ActiveId!=TEXT("HeroArena"))return true;
  TArray<int32> Decks;
  for(const auto& E:PC->Transport->EntryEvents){
   if(E.Target!=InteriorShip || E.FactionId!=0)continue;
   const auto Person=PC->Transport->GetPerson(E.PersonId);
   if(Play->ArenaTeamForUnit(Person.RuntimeUnitId)==(Action==TEXT("InteriorEntryA")?0:1))Decks.AddUnique(E.Deck);
  }
  if(!Decks.IsEmpty()){
   Decks.Sort();
   InteriorDeck=Decks[(Decks.IndexOfByKey(InteriorDeck)+1)%Decks.Num()];
   InteriorTargetSlot=NAME_None;
  }
  return true;
 }
 if(Action==TEXT("InteriorToggle")||Action==TEXT("InteriorMissionToggle")) { bInteriorOpen=!bInteriorOpen;if(bInteriorOpen)ViewShip(PC->Selected);else CancelPointerDrag();return true; }
 if(Action==TEXT("InteriorZoomIn") || Action==TEXT("InteriorZoomOut")){InteriorMapScale=FMath::Clamp(InteriorMapScale+(Action==TEXT("InteriorZoomIn")?.2f:-.2f),.6f,1.f);return true;}
 if(Action==TEXT("InteriorClose")) { bInteriorOpen=false;CancelPointerDrag();return true; }
 if(Action==TEXT("InteriorMin")) { bInteriorMinimized=!bInteriorMinimized;CancelPointerDrag();return true; }
 if(Action==TEXT("InteriorMax")) {
  if(bInteriorMaximized){InteriorWindowPosition=InteriorRestorePosition;InteriorWindowSize=InteriorRestoreSize;bInteriorMaximized=false;}
  else {InteriorRestorePosition=InteriorBounds().Min;InteriorRestoreSize=InteriorWindowSize;InteriorWindowPosition=FVector2D(16,70);InteriorWindowSize=FVector2D(ViewportPixels.X/UIScale-32,ViewportPixels.Y/UIScale-100);bInteriorMaximized=true;}
  bInteriorMinimized=false;CancelPointerDrag();return true;
 }
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
 if(Name.StartsWith(TEXT("InteriorFilter_"))){
  InteriorTeamFilter=FMath::Clamp(FCString::Atoi(*Name.Mid(15))-1,INDEX_NONE,2);
  if(InteriorTeamFilter!=INDEX_NONE){
   const auto* Play=PC->GetPlayScenario();
   InteriorSelectedUnits.RemoveAll([&](int32 Id){
    return OVInteriorUI::FormationTeam(Play,Interior->GetUnit(Id))!=InteriorTeamFilter;
   });
  }
  InteriorCardColumn=0;
  return true;
 }
 if(Action==TEXT("InteriorPrev") || Action==TEXT("InteriorNext")){
  InteriorCardColumn=FMath::Clamp(InteriorCardColumn+(Action==TEXT("InteriorNext")?1:-1),0,InteriorLastColumn);
  return true;
 }
 if(Name.StartsWith(TEXT("InteriorSquad_"))){
  const auto Leader=Interior->GetUnit(FCString::Atoi(*Name.Mid(14)));
  if(Leader.Ship!=Ship || Leader.FactionId!=0 || Leader.SquadId<=0)return true;
  InteriorSelectedUnits.Reset();
  for(const auto& Member:Interior->Units)
   if(Member.Ship==Ship && Member.FactionId==0 && Member.SquadId==Leader.SquadId && Member.NodeId==Leader.NodeId && OVInteriorUI::Living(Member))
    InteriorSelectedUnits.Add(Member.UnitId);
  return true;
 }
 if(Name.StartsWith(TEXT("InteriorCard_"))){
  const auto Unit=Interior->GetUnit(FCString::Atoi(*Name.Mid(13)));
  if(Unit.Ship!=Ship || Unit.FactionId!=0)return true;
  const bool Extend=PC->IsInputKeyDown(EKeys::LeftShift)||PC->IsInputKeyDown(EKeys::RightShift);
  if(!Extend)InteriorSelectedUnits.Reset();
  if(Extend && InteriorSelectedUnits.Contains(Unit.UnitId))InteriorSelectedUnits.Remove(Unit.UnitId);
  else InteriorSelectedUnits.AddUnique(Unit.UnitId);
  const auto* Layout=Ship->Systems->Layout.Get();
  if(const int32 Node=Layout->GetSlotIndex(Unit.NodeId);Layout->Slots.IsValidIndex(Node))InteriorDeck=Layout->Slots[Node].DeckId;
  return true;
 }
 if(Action==TEXT("InteriorClear")) { InteriorSelectedUnits.Reset();return true; }
 if(Action==TEXT("InteriorAll"))
 {
  InteriorSelectedUnits.Reset();
  for(const auto& U:Interior->Units) if(U.Ship==Ship&&U.FactionId==0&&OVInteriorUI::Living(U) && (InteriorTeamFilter==INDEX_NONE || OVInteriorUI::FormationTeam(PC->GetPlayScenario(),U)==InteriorTeamFilter))InteriorSelectedUnits.Add(U.UnitId);
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
 InteriorScrollTrack.Init();
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
 if(!bInteriorMinimized)DrawLine(X+W-14,Y+H-2,X+W-2,Y+H-14,Accent,2);
 AOVShip* Ship=InteriorShip.Get();
 DrawText(TEXT("선내 작전 · ")+(Ship?Ship->ShipName:TEXT("함선 선택")),Accent,X+12,Y+10,OVTypography::Font(EOVTextRole::Heading));
 Button(TEXT("InteriorClose"),TEXT("X"),X+W-30,Y+2,26);
 Button(TEXT("InteriorMin"),bInteriorMinimized?TEXT("+"):TEXT("_"),X+W-60,Y+2,26);
 Button(TEXT("InteriorMax"),TEXT("[]"),X+W-90,Y+2,26);
 Button(TEXT("InteriorHeroPanel"),TEXT("영웅"),X+W-158,Y+2,60);
 if(bInteriorMinimized)return;
 Button(TEXT("InteriorSelectedShip"),TEXT("선택 함선 보기"),X+12,Y+37,140);
 Button(TEXT("InteriorZoomOut"),TEXT("−"),X+W-74,Y+37,30);
 Button(TEXT("InteriorZoomIn"),TEXT("+"),X+W-40,Y+37,30);
 Button(TEXT("InteriorShipNext"),TEXT("다음 알려진 함선"),X+160,Y+37,150);
 if(!IsValid(Ship)||!Ship->Systems||!Ship->Systems->Layout||!Interior->IsStructureKnown(0,Ship))
 {
  DrawText(TEXT("내부 구조 미관측 · 아군의 실제 진입 후 표시됩니다."),Ink,X+24,Y+110,OVTypography::Font(EOVTextRole::Body));
  return;
 }
 const auto* L=Ship->Systems->Layout.Get();InteriorDeck=FMath::Clamp(InteriorDeck,1,L->DeckCount);
 const auto* Play=PC->GetPlayScenario();
 const bool Arena=Play && Play->ActiveId==TEXT("HeroArena");
 if(!Arena)InteriorTeamFilter=INDEX_NONE;
 const float Footer=Y+H-208, CardsX=X+174, Right=X+W-12, CardsWidth=Right-CardsX;
 const FLinearColor Surface(.025f,.052f,.084f), Soft(.43f,.61f,.7f);
 for(int32 D=1;D<=L->DeckCount;++D)Button(FName(*FString::Printf(TEXT("InteriorDeck_%d"),D)),FString::Printf(TEXT("%d층"),D),X+12+(D-1)*64,Y+70,58,InteriorDeck==D);
 DrawRect(FLinearColor(.014f,.034f,.053f),X+10,Y+100,W-20,Footer-Y-103);
 DrawLine(X+10,Footer-2,Right,Footer-2,Accent,1);
 DrawRect(Surface,X+10,Footer+3,154,194);

 TArray<const FOVInteriorUnitRecord*> Own,VisibleCards;
 const FOVInteriorUnitRecord* Focus=nullptr;
 int32 Fighting=0,Injured=0;
 for(const auto& U:Interior->Units){
  if(U.Ship!=Ship || U.FactionId!=0)continue;
  Own.Add(&U);
  if(InteriorTeamFilter==INDEX_NONE || FormationTeam(Play,U)==InteriorTeamFilter)VisibleCards.Add(&U);
  if(!InteriorSelectedUnits.Contains(U.UnitId))continue;
  if(!Focus)Focus=&U;
  Fighting+=U.State==EOVInteriorState::Fighting;
  Injured+=Living(U) && U.HP<U.MaxHP;
 }
 // Keep actual heroes at the front of the strip; maintain stable personal IDs within each role.
 VisibleCards.StableSort([](const FOVInteriorUnitRecord& A,const FOVInteriorUnitRecord& B){
  const bool HeroA=A.Role!=EOVInteriorRole::Trooper,HeroB=B.Role!=EOVInteriorRole::Trooper;
  return HeroA!=HeroB?HeroA:A.UnitId<B.UnitId;
 });
 auto Text=[&](const FString& Value,FLinearColor Color,float TX,float TY,EOVTextRole TextRole=EOVTextRole::Body){DrawText(Value,Color,TX,TY,OVTypography::Font(TextRole));};
 Text(FString::Printf(TEXT("선택  %d명"),InteriorSelectedUnits.Num()),Soft,X+18,Footer+10);
 if(Focus && InteriorSelectedUnits.Num()==1){
  const auto* Definition=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definition(Focus->DefinitionId);
  if(Definition && Definition->Portrait)DrawTexture(Definition->Portrait,X+119,Footer+31,34,34,0,0,1,1);
  Text(CardName(*Focus),Accent,X+18,Footer+34,EOVTextRole::HeroName);
  Text(FString::Printf(TEXT("HP %.0f / %.0f"),Focus->HP,Focus->MaxHP),Ink,X+18,Footer+60,EOVTextRole::Numeric);
  Text(OVInteriorUI::Role(Focus->Role)+TEXT(" · ")+OVInteriorUI::State(Focus->State),Ink,X+18,Footer+80);
  if(const int32 Node=L->GetSlotIndex(Focus->NodeId);L->Slots.IsValidIndex(Node)){
   const auto& Slot=L->Slots[Node];
   Text(FString::Printf(TEXT("%d층 · %s"),Slot.DeckId,*Facility(Slot.Facility)),Ink,X+18,Footer+100);
  }
  if(Focus->State==EOVInteriorState::Operating){
   DrawRect(Soft,X+18,Footer+121,130,4);
   DrawRect(Accent,X+18,Footer+121,130*FMath::Clamp(Focus->Progress,0.f,1.f),4);
  }
 }else if(Focus){
  Text(FString::Printf(TEXT("교전 %d · 부상 %d"),Fighting,Injured),Ink,X+18,Footer+43);
 }else Text(TEXT("아래 카드를 선택하세요"),Ink,X+18,Footer+43);
 const int32 FilterCount=Arena?4:1;
 const TCHAR* FilterLabels[]={TEXT("전체"),TEXT("A팀"),TEXT("B팀"),TEXT("예비")};
 for(int32 Filter=0;Filter<FilterCount;++Filter)
  Button(FName(*FString::Printf(TEXT("InteriorFilter_%d"),Filter)),FilterLabels[Filter],CardsX+Filter*48,Footer+36,44,InteriorTeamFilter==Filter-1);
 Button(TEXT("InteriorAll"),TEXT("모두 선택"),Right-148,Footer+36,74);
 Button(TEXT("InteriorClear"),TEXT("선택 해제"),Right-70,Footer+36,70);
 const int32 Columns=FMath::Max(1,FMath::FloorToInt(CardsWidth/112.f));
 const int32 TotalColumns=FMath::DivideAndRoundUp(VisibleCards.Num(),2);
 InteriorLastColumn=FMath::Max(0,TotalColumns-Columns);
 InteriorCardColumn=FMath::Clamp(InteriorCardColumn,0,InteriorLastColumn);
 const float Pitch=CardsWidth/Columns;
 const auto* Roster=GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
 for(int32 Column=0;Column<Columns;++Column)for(int32 Row=0;Row<2;++Row){
  const int32 Index=2*(Column+InteriorCardColumn)+Row;
  if(!VisibleCards.IsValidIndex(Index))continue;
  const auto& U=*VisibleCards[Index];
  const float CX=CardsX+Column*Pitch,CY=Footer+68+Row*49,CW=Pitch-4;
  const bool Selected=InteriorSelectedUnits.Contains(U.UnitId);
  DrawRect(Selected?FLinearColor(.035f,.21f,.26f):Surface,CX,CY,CW,45);
  if(Selected)DrawRect(Accent,CX,CY,3,45);
  const auto* Def=U.DefinitionId.IsNone()?nullptr:Roster->Definition(U.DefinitionId);
  if(Def && Def->Portrait)DrawTexture(Def->Portrait,CX+5,CY+4,32,32,0,0,1,1);
  else {
   const bool Soldier=U.Role==EOVInteriorRole::Trooper;
   const FLinearColor Badge=Soldier?Soft:FLinearColor(.93f,.68f,.3f);
   DrawRect(FLinearColor(.045f,.082f,.11f),CX+5,CY+4,32,32);
   // Helmet and shoulder silhouette doubles as the fallback portrait.
   DrawRect(Badge,CX+16,CY+8,10,10);
   DrawRect(Panel,CX+18,CY+12,8,2);
   DrawLine(CX+10,CY+27,CX+16,CY+21,Badge,3);
   DrawRect(Badge,CX+16,CY+20,10,10);
   DrawLine(CX+26,CY+21,CX+32,CY+27,Badge,3);
  }
  const float Bar=CW-46;
  const auto NameRole=U.Role==EOVInteriorRole::Trooper?EOVTextRole::Body:EOVTextRole::HeroName;
  const FString Label=FitLabel(OVTypography::Font(NameRole),CardName(U),Bar);
  Text(Label,Ink,CX+40,CY+2,NameRole);
  Text(OVInteriorUI::State(U.State),Soft,CX+40,CY+18);
  const float HP=FMath::Clamp(U.HP/FMath::Max(1.f,U.MaxHP),0.f,1.f);
  DrawRect(Panel,CX+40,CY+36,Bar,4);
  DrawRect(HP<.35f?Enemy:Accent,CX+40,CY+36,Bar*HP,4);
  const FName Action(*FString::Printf(TEXT("InteriorCard_%d"),U.UnitId));
  AddHitBox(FVector2D(CX,CY)*UIScale,FVector2D(CW,45)*UIScale,Action,true,211);
  ActionCenters.Add(Action,FVector2D(CX+CW*.5,CY+22));
 }
 if(VisibleCards.IsEmpty())Text(TEXT("이 함선에 해당 인원이 없습니다"),Soft,CardsX+8,Footer+83);
 const float TrackY=Footer+174;
 InteriorScrollTrack=FBox2D(FVector2D(CardsX+34,TrackY),FVector2D(Right-34,TrackY+19));
 const float TrackWidth=InteriorScrollTrack.GetSize().X;
 InteriorThumbWidth=TrackWidth*Columns/FMath::Max(Columns,TotalColumns);
 const float ThumbLeft=InteriorScrollTrack.Min.X+(InteriorLastColumn?(TrackWidth-InteriorThumbWidth)*InteriorCardColumn/InteriorLastColumn:0);
 DrawRect(Surface,InteriorScrollTrack.Min.X,TrackY+5,TrackWidth,9);
 DrawRect(InteriorLastColumn?Accent:Soft,ThumbLeft,TrackY+5,InteriorThumbWidth,9);
 if(InteriorLastColumn){
  Button(TEXT("InteriorPrev"),TEXT("◀"),CardsX,TrackY-3,29);
  Button(TEXT("InteriorNext"),TEXT("▶"),Right-29,TrackY-3,29);
 }
 const float MapTop=Y+104,MapBottom=Footer-5;
 // Fit this deck's room centers to the available map, leaving room for labels and occupants.
 FBox2D Rooms(ForceInit);
 for(const auto& Slot:L->Slots)if(Slot.DeckId==InteriorDeck)Rooms+=FVector2D(Slot.LocalCenter.X,Slot.LocalCenter.Y);
 if(!Rooms.bIsValid)return;
 const FVector2D Mid=Rooms.GetCenter(),RoomExtent=Rooms.GetExtent();
 auto Project=[&](FVector Local){
  const double NX=FMath::Clamp((Local.X-Mid.X)/FMath::Max(1.,RoomExtent.X),-1.12,1.12);
  const double NY=FMath::Clamp((Local.Y-Mid.Y)/FMath::Max(1.,RoomExtent.Y),-1.12,1.12);
  return FVector2D(X+W*.5+NX*(W*.5-78)*InteriorMapScale,
      (MapTop+MapBottom)*.5-18+NY*FMath::Max(10.f,(MapBottom-MapTop)*.5f-46)*InteriorMapScale);
 };
 for(const auto& Link:L->Connections)
 {
  const int32 AI=L->GetSlotIndex(Link.FromSlot),BI=L->GetSlotIndex(Link.ToSlot);
  if(!L->Slots.IsValidIndex(AI)||!L->Slots.IsValidIndex(BI))continue;
  const auto& A=L->Slots[AI];const auto& B=L->Slots[BI];
  if(A.DeckId!=InteriorDeck&&B.DeckId!=InteriorDeck)continue;
  const FVector2D P=Project(A.LocalCenter),Q=Project(B.LocalCenter);
  if(A.DeckId==B.DeckId)DrawLine(P.X,P.Y,Q.X,Q.Y,FLinearColor(.25f,.4f,.5f),2);
  else DrawText(FString::Printf(TEXT("↕%d층"),A.DeckId==InteriorDeck?B.DeckId:A.DeckId),Accent,P.X+15,P.Y-18,OVTypography::Font(EOVTextRole::Numeric));
 }
 if(Focus)for(int32 I=1;I<Focus->Path.Num();++I)
 {
  const int32 A=L->GetSlotIndex(Focus->Path[I-1]),B=L->GetSlotIndex(Focus->Path[I]);
  if(!L->Slots.IsValidIndex(A)||!L->Slots.IsValidIndex(B)||L->Slots[A].DeckId!=InteriorDeck||L->Slots[B].DeckId!=InteriorDeck)continue;
  const FVector2D P=Project(L->Slots[A].LocalCenter),Q=Project(L->Slots[B].LocalCenter);
  DrawLine(P.X,P.Y,Q.X,Q.Y,FLinearColor(.3f,1,.45f),3);
 }
 TArray<FBox2D> Markers;
 for(const auto& Slot:L->Slots)if(Slot.DeckId==InteriorDeck)
 {
  const FVector2D P=Project(Slot.LocalCenter);
  Markers.Emplace(P-FVector2D(35,13),P+FVector2D(35,14));
  Button(FName(*(TEXT("InteriorSlot_")+Slot.SlotId.ToString())),Facility(Slot.Facility),P.X-35,P.Y-13,70,Slot.SlotId==InteriorTargetSlot);
 }
 // Squads are split by room, allegiance and visibility. A separated survivor keeps a marker.
 TMap<TTuple<FName,int32,int32>,TArray<int32>> RoomSquads;
 for(const auto& U:Interior->Units){
  if(U.Ship==Ship && U.Role==EOVInteriorRole::Trooper && U.SquadId>0 && Living(U) && Interior->CanObserve(0,U.UnitId))
   RoomSquads.FindOrAdd(MakeTuple(U.NodeId,U.FactionId,U.SquadId)).Add(U.UnitId);
 }
 TMap<FName,int32> RoomOccupants;
 for(const auto& U:Interior->Units)if(U.Ship==Ship&&Interior->CanObserve(0,U.UnitId))
 {
  const int32 Node=L->GetSlotIndex(U.NodeId);if(!L->Slots.IsValidIndex(Node)||L->Slots[Node].DeckId!=InteriorDeck)continue;
  const bool Hero=U.Role!=EOVInteriorRole::Trooper,Selected=InteriorSelectedUnits.Contains(U.UnitId);
  const auto* Squad=!Hero && Living(U)?RoomSquads.Find(MakeTuple(U.NodeId,U.FactionId,U.SquadId)):nullptr;
  if(Squad && (*Squad)[0]!=U.UnitId)continue;
  if(!Hero && !Living(U))continue; // Individual casualty cards and notices remain available.
  const bool Moving=U.State==EOVInteriorState::Moving||U.State==EOVInteriorState::Retreating;
  const float Step=Moving?FMath::Sin(Interior->GameTime*10+U.UnitId)*2.f:0.f;
  const int32 Place=RoomOccupants.FindOrAdd(U.NodeId)++;
  FVector2D P=Project(U.LocalPosition)+FVector2D((Place%4-1.5f)*33,24+(Place/4)*24);
  P.X=FMath::Clamp(P.X,X+28.,X+W-29.);
  P.Y=FMath::Clamp(P.Y,MapTop+17.,MapBottom-17.);
  const FVector2D Anchor=Project(U.LocalPosition);
  const FVector2D HalfSize=Squad?FVector2D(17,9):FVector2D(13,17);
  auto FreePosition=[&](FVector2D Center){
   const FBox2D Box(Center-HalfSize,Center+HalfSize);
   return !Markers.ContainsByPredicate([&](const auto& Occupied){return Occupied.Intersect(Box);});
  };
  if(!FreePosition(P)){
   // Use the nearest clear space around this unit, independently of mouse selection.
   const FVector2D Directions[]={{0,1},{1,0},{-1,0},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
   bool Found=false;
   for(int32 Ring=1;Ring<=3 && !Found;++Ring){
    for(const auto& Direction:Directions){
     const FVector2D Candidate=P+Direction*FVector2D(20,18)*Ring;
     if(Candidate.X<X+28 || Candidate.X>X+W-29 || Candidate.Y<MapTop+17 || Candidate.Y>MapBottom-17 || !FreePosition(Candidate))continue;
     P=Candidate;Found=true;break;
    }
   }
  }
  Markers.Emplace(P-HalfSize,P+HalfSize);
  if(FVector2D::DistSquared(P,Anchor)>FMath::Square(40.f))
   DrawLine(Anchor.X,Anchor.Y,P.X,P.Y,Soft.CopyWithNewOpacity(.45f),1);
  if(Squad){
   const bool Friendly=U.FactionId==0;
   const FLinearColor Tint=Friendly?Accent:Enemy;
   const bool AnySelected=Squad->ContainsByPredicate([&](int32 Id){return InteriorSelectedUnits.Contains(Id);});
   DrawRect(AnySelected?FLinearColor(.035f,.21f,.26f):Surface,P.X-16,P.Y-7,32,15);
   DrawRect(Tint,P.X-16,P.Y-7,2,15);
   const int32 Team=FormationTeam(Play,U);
   const TCHAR* Symbol=Team==0?TEXT("A"):Team==1?TEXT("B"):Friendly?TEXT("◆"):TEXT("◇");
   Text(FString::Printf(TEXT("%s×%d"),Symbol,Squad->Num()),Tint,P.X-12,P.Y-8);
   if(Friendly){
    const FName Marker(*FString::Printf(TEXT("InteriorSquad_%d"),U.UnitId));
    AddHitBox(FVector2D(P.X-16,P.Y-8)*UIScale,FVector2D(32,18)*UIScale,Marker,true,215);
    ActionCenters.Add(Marker,P);
   }
   continue;
  }
  const FLinearColor RoleColors[]={Ink,FLinearColor(.2f,.75f,1),FLinearColor(1,.65f,.2f),FLinearColor(.25f,1,.6f),FLinearColor(.75f,.55f,1)};
  FLinearColor C=U.FactionId==0?RoleColors[FMath::Clamp(int32(U.Role),0,4)]:Enemy;
  if(U.State==EOVInteriorState::Dead)C=FLinearColor(.45f,.45f,.5f);
  if(Interior->GameTime-U.LastDamagedAt<.18)C=FLinearColor(1,.2f,.18f);
  const auto* Art=Hero && !U.DefinitionId.IsNone()?GetWorld()->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>()->Definition(U.DefinitionId):nullptr;
  const float Body=Hero?8.f:5.f;
  if(Selected)
  {
   DrawLine(P.X-10,P.Y-12,P.X+11,P.Y-12,Accent,2);
   DrawLine(P.X-10,P.Y+16,P.X+11,P.Y+16,Accent,2);
   DrawLine(P.X-10,P.Y-12,P.X-10,P.Y+16,Accent,2);
   DrawLine(P.X+11,P.Y-12,P.X+11,P.Y+16,Accent,2);
  }
  const bool Dead=U.State==EOVInteriorState::Dead;
  const bool ArtVisible=Art && Art->SD && InteriorMapScale>=.79f;
  if(Dead && !(ArtVisible && Art->Poses.Contains(TEXT("death"))))
  {
   DrawLine(P.X-6,P.Y-4,P.X+6,P.Y+7,C,3);DrawLine(P.X-6,P.Y+7,P.X+6,P.Y-4,C,3);
  }
  else if(ArtVisible)
  {
   const float Height=31.f*Art->Scale*InteriorMapScale;
   const bool Hit=Interior->GameTime-U.LastDamagedAt<.18;
   const bool Skill=Interior->GameTime-U.LastPhaseArcAt<.35 || Interior->GameTime<U.SkillActiveUntil;
   const FName Pose=Dead?TEXT("death"):Hit?TEXT("hit"):Skill?TEXT("skill"):Moving?TEXT("move"):
       U.State==EOVInteriorState::Fighting?TEXT("attack"):U.State==EOVInteriorState::Operating?TEXT("operate"):TEXT("idle");
   UTexture2D* Image=Art->SD;
   if(const auto* Sequence=Art->Poses.Find(Pose);Sequence && !Sequence->Frames.IsEmpty()){
    const double Time=(Dead || Hit)?FMath::Max(0.,Interior->GameTime-U.LastDamagedAt):Interior->GameTime;
    const int32 Frame=FMath::FloorToInt(Time*Sequence->FramesPerSecond);
    Image=Sequence->Frames[Dead?FMath::Min(Frame,Sequence->Frames.Num()-1):Frame%Sequence->Frames.Num()];
   }
   FVector2D Direction=FVector2D::ZeroVector;
   if(U.EnemyId>0){
    const auto* Target=Interior->Units.FindByPredicate([&](const auto& Other){return Other.UnitId==U.EnemyId;});
    if(Target && Interior->CanObserve(0,Target->UnitId))Direction=(Project(Target->LocalPosition)-Project(U.LocalPosition)).GetSafeNormal();
   }else if(const int32 Goal=L->GetSlotIndex(U.TargetSlot);Moving && Goal!=INDEX_NONE)
    Direction=(Project(L->Slots[Goal].LocalCenter)-Project(U.LocalPosition)).GetSafeNormal();
   const bool Flip=Art->bAllowMirror && Direction.X<0;
   const float Width=Height*Image->GetSizeX()/FMath::Max(1,Image->GetSizeY());
   DrawTexture(Image,P.X-Width*(Flip?1-Art->Foot.X:Art->Foot.X),P.Y+11-Height*Art->Foot.Y+Step,Width,Height,
       Flip?1:0,0,Flip?-1:1,1,Hit?FLinearColor(1,.55f,.55f):FLinearColor::White);
   if(Selected && !Direction.IsNearlyZero()){
    const FVector2D End=P+Direction*17;
    DrawLine(P.X+Direction.X*8,P.Y+Direction.Y*8,End.X,End.Y,Accent,2);
   }
   DrawLine(P.X-7,P.Y+13,P.X+7,P.Y+13,Selected?Accent:C,2);
  }
  else
  {
   DrawRect(FLinearColor(.75f,.61f,.52f),P.X-2.5f,P.Y-8+Step,5,5);
   DrawRect(C,P.X-Body*.5f,P.Y-2+Step,Body,8);
   DrawLine(P.X-2,P.Y+5+Step,P.X-4-Step,P.Y+11,C,2);
   DrawLine(P.X+2,P.Y+5+Step,P.X+4+Step,P.Y+11,C,2);
   if(Hero)
   {
    // Role equipment is readable independently of faction color.
    if(U.Role==EOVInteriorRole::Melee)DrawRect(C*.7f,P.X-9,P.Y-1+Step,4,11);
    else if(U.Role==EOVInteriorRole::Ranged)DrawLine(P.X+4,P.Y+1+Step,P.X+13,P.Y-2+Step,C,3);
    else if(U.Role==EOVInteriorRole::Support){DrawLine(P.X+7,P.Y-4+Step,P.X+7,P.Y+4+Step,C,2);DrawLine(P.X+3,P.Y+Step,P.X+11,P.Y+Step,C,2);}
    else {DrawLine(P.X-7,P.Y-11+Step,P.X+7,P.Y-11+Step,C,2);DrawLine(P.X-7,P.Y-11+Step,P.X-4,P.Y-15+Step,C,2);DrawLine(P.X+7,P.Y-11+Step,P.X+4,P.Y-15+Step,C,2);}
   }
  }
  if(U.State!=EOVInteriorState::Dead){
   const float ArcAge=static_cast<float>(Interior->GameTime-U.LastPhaseArcAt);
   if(ArcAge>=0 && ArcAge<.35f){
    const float Radius=12.f+ArcAge*32.f;
    const FLinearColor Phase(1,.28f,.68f,1-ArcAge/.35f);
    const FVector Extent=Ship->GetActorScale3D();
    FVector2D Heading=(Project(U.LocalPosition+U.FieldFacing/Extent)-Project(U.LocalPosition)).GetSafeNormal();
    if(Heading.IsNearlyZero())Heading=FVector2D(1,0);
    const float Bearing=FMath::Atan2(Heading.Y,Heading.X);
    FVector2D Prev=P+Heading.GetRotated(-60.f)*Radius;
    for(int32 I=1;I<=8;++I){
     const float Angle=Bearing+FMath::DegreesToRadians(-60.f+15.f*I);
     const FVector2D Next=P+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius;
     DrawLine(Prev.X,Prev.Y,Next.X,Next.Y,Phase,2);Prev=Next;
    }
   }
   const double SinceShot=Interior->GameTime-(U.AttackReadyAt-U.Period);
   if(U.Shots>0&&U.State==EOVInteriorState::Fighting&&SinceShot>=0&&SinceShot<.16)
   {
    DrawLine(P.X+8,P.Y,P.X+17,P.Y-4,FLinearColor(1,.9f,.4f),2);
    DrawLine(P.X+13,P.Y-7,P.X+13,P.Y+1,FLinearColor(1,.9f,.4f),2);
   }
   if(U.State==EOVInteriorState::Operating)
   {
    const float Pulse=.55f+.45f*FMath::Sin(Interior->GameTime*5);
    DrawRect(FLinearColor(.3f,1,.65f)*Pulse,P.X-9,P.Y+18,18*FMath::Clamp(U.Progress,0.f,1.f),3);
   }
  }
  if(Hero||Selected||U.HP<U.MaxHP)
  {
   DrawRect(FLinearColor(.06f,.1f,.14f),P.X-9,P.Y+14,18,2);
   DrawRect(C,P.X-9,P.Y+14,18*FMath::Clamp(U.HP/FMath::Max(1.f,U.MaxHP),0.f,1.f),2);
  }
  if(U.FactionId==0) {
   const FName Marker(*FString::Printf(TEXT("InteriorMarker_%d"),U.UnitId));
   AddHitBox(FVector2D(P.X-11,P.Y-16)*UIScale,FVector2D(24,34)*UIScale,Marker,true,215);
   ActionCenters.Add(Marker,P);
  }
 }
 if(Arena){
  for(int32 I=0;I<2;++I){
   bool HasEntry=false;
   TSet<FName> Labelled;
   const FLinearColor Color=I==0?FLinearColor(.15f,1.f,.85f):FLinearColor(1.f,.7f,.18f);
   for(const auto& Entry:PC->Transport->EntryEvents){
    if(Entry.Target!=Ship || Entry.FactionId!=0)continue;
    const auto Person=PC->Transport->GetPerson(Entry.PersonId);
    if(Play->ArenaTeamForUnit(Person.RuntimeUnitId)!=I)continue;
    HasEntry=true;
    if(Entry.Deck!=InteriorDeck)continue;
    FVector2D Point=Project(Entry.LocalPoint);
    Point.X=FMath::Clamp(Point.X,X+16.,X+W-16.);
    Point.Y=FMath::Clamp(Point.Y,MapTop+5.,MapBottom-5.);
    const FVector2D Label(FMath::Clamp(Point.X+9.,X+12.,X+W-72.),
        FMath::Clamp(Point.Y+(I==0?-29:17),MapTop+2.,MapBottom-18.));
    DrawLine(Point.X-4,Point.Y,Point.X+4,Point.Y,Color,2);
    DrawLine(Point.X,Point.Y-4,Point.X,Point.Y+4,Color,2);
    if(Labelled.Contains(Entry.SlotId))continue;
    Labelled.Add(Entry.SlotId);
    DrawLine(Point.X,Point.Y,Label.X,Label.Y+7,Color,1);
    DrawRect(Panel,Label.X,Label.Y,61,17);
    DrawText(I==0?TEXT("A팀 진입점"):TEXT("B팀 진입점"),Color,Label.X+2,Label.Y,OVTypography::Font(EOVTextRole::Body));
   }
   if(HasEntry)Button(I==0?TEXT("InteriorEntryA"):TEXT("InteriorEntryB"),I==0?TEXT("A 진입층"):TEXT("B 진입층"),X+318+I*84,Y+37,80);
  }
 }
 if(PC->bDeveloperMode)Text(L->LayoutVersion.ToString(),Soft,X+W-165,Y+77);
 const int32 SlotIndex=L->GetSlotIndex(InteriorTargetSlot);
 if(L->Slots.IsValidIndex(SlotIndex)&&Ship->Systems->Slots.IsValidIndex(SlotIndex))
 {
  const auto& Slot=L->Slots[SlotIndex];const auto& S=Ship->Systems->Slots[SlotIndex];
  Text(Facility(Slot.Facility)+FString::Printf(TEXT(" · %d층"),Slot.DeckId),Accent,X+18,Footer+131);
  const bool Observed=Ship->Systems->FactionId==0||Own.ContainsByPredicate([&](const auto* U){return Living(*U)&&U->State!=EOVInteriorState::InTransit&&U->State!=EOVInteriorState::RescueWaiting&&U->NodeId==Slot.SlotId;});
  if(Observed){
   Text(FString::Printf(TEXT("시설 HP %.0f/%.0f"),S.HP,Slot.MaxFacilityHP),Ink,X+18,Footer+150,EOVTextRole::Numeric);
   Text(FString::Printf(TEXT("운영 %.0f/%.0f · %s"),S.Crew,Slot.RequiredCrew,S.OccupyingFaction<0?TEXT("미점령"):S.OccupyingFaction==0?TEXT("아군"):TEXT("적군")),Ink,X+18,Footer+169);
  }else Text(TEXT("시설 상태 미관측"),Soft,X+18,Footer+151);
 }
 const FName Commands[]={TEXT("InteriorCapture"),TEXT("InteriorSabotage"),TEXT("InteriorDefend"),TEXT("InteriorRetreat"),TEXT("InteriorCancel")};
 const TCHAR* Labels[]={TEXT("점령"),TEXT("공작"),TEXT("수호"),TEXT("철수"),TEXT("취소")};
 const float CommandWidth=CardsWidth/5;
 for(int32 I=0;I<5;++I)Button(Commands[I],Labels[I],CardsX+I*CommandWidth,Footer+3,CommandWidth-5);
 const FString Feedback=Focus&&InteriorSelectedUnits.Num()==1&&!Focus->Reason.IsEmpty()?Focus->Reason:InteriorFeedback;
 if(!Feedback.IsEmpty())Text(Feedback.Left(56),FLinearColor(1,.8f,.25f),CardsX,Y+H-15);
}
