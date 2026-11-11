#include "OVHUD.h"
#include "OVController.h"
#include "OVShip.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
void AOVHUD::Button(FName Name,const FString& Label,float X,float Y,float W,bool Active)
{
 DrawRect(Active?FLinearColor(0.03f,0.4f,0.55f,0.95f):FLinearColor(0.035f,0.07f,0.12f,0.95f),X,Y,W,32);
 DrawText(Label,Active?FLinearColor::White:FLinearColor(0.6f,0.8f,0.9f),X+9,Y+7,GEngine->GetSmallFont(),1);
 AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(W,32)*UIScale,Name,true,10);
}
bool AOVHUD::IsOverPanel(float X,float Y) const { return (X>=12*UIScale && X<=382*UIScale && Y>=12*UIScale && Y<=480*UIScale) || (Canvas && Y>=Canvas->SizeY-95*UIScale); }
void AOVHUD::NotifyHitBoxClick(FName Name) { if(AOVController* PC=Cast<AOVController>(PlayerOwner)) PC->Action(Name); }
void AOVHUD::DrawHUD()
{
 Super::DrawHUD(); AOVController* PC=Cast<AOVController>(PlayerOwner); if(!PC||!Canvas) return;
 UIScale=FMath::Max(0.25f,FMath::Min(1.f,FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f)));
 Canvas->Canvas->PushRelativeTransform(FScaleMatrix(FVector(UIScale,UIScale,1)));
 const FLinearColor Text(0.72f,0.85f,0.93f); const FLinearColor Accent(0.2f,0.85f,1);
 DrawRect(FLinearColor(0.006f,0.014f,0.03f,0.94f),12,12,370,468);
 DrawText(TEXT("ORBITAL / VANGUARD"),FLinearColor::White,26,22,GEngine->GetMediumFont(),1.1f);
 DrawText(TEXT("01   /   3D CONTROL RANGE"),Accent,26,55,GEngine->GetSmallFont());
 Button(TEXT("Ship1"),TEXT("F1  KESTREL"),26,84,160,PC->Selected==PC->Ships[0]);
 Button(TEXT("Ship2"),TEXT("F2  ATLAS"),196,84,160,PC->Selected==PC->Ships[1]);
 if(PC->Selected) {
  AOVShip* Ship=PC->Selected; FVector P=Ship->GetActorLocation();
  DrawText(Ship->ShipName,Text,26,128,GEngine->GetSmallFont());
  DrawText(FString::Printf(TEXT("XYZ  %6.0f  %6.0f  %6.0f"),P.X,P.Y,P.Z),Text,26,149,GEngine->GetSmallFont());
  DrawText(FString::Printf(TEXT("Speed %.0f u/s   Turn %.0f deg/s"),Ship->MoveSpeed,Ship->TurnRate),Text,26,170,GEngine->GetSmallFont());
  DrawText(Ship->bFriendly?(Ship->bManual?TEXT("MANUAL / persistent orders"):TEXT("AUTO / target heading only")):TEXT("ENEMY / stationary test target"),Accent,26,191,GEngine->GetSmallFont());
  Button(TEXT("Manual"),TEXT("MANUAL"),26,217,160,Ship->bFriendly&&Ship->bManual);
  Button(TEXT("Auto"),TEXT("AUTO"),196,217,160,Ship->bFriendly&&!Ship->bManual);
 }
 Button(TEXT("Pause"),PC->bSimulationPaused?TEXT("RESUME"):TEXT("PAUSE"),26,261,100,PC->bSimulationPaused);
 Button(TEXT("Speed1"),TEXT("1x"),136,261,65,PC->SimulationSpeed==1);
 Button(TEXT("Speed2"),TEXT("2x"),211,261,65,PC->SimulationSpeed==2);
 Button(TEXT("Speed3"),TEXT("3x"),286,261,70,PC->SimulationSpeed==3);
 DrawText(FString::Printf(TEXT("SIM %7.1fs  |  %s / %.0fx"),PC->SimulationSeconds,PC->bSimulationPaused?TEXT("PAUSED"):TEXT("RUNNING"),PC->SimulationSpeed),Text,26,305,GEngine->GetSmallFont());
 Button(TEXT("Move"),TEXT("R  MOVE XYZ"),26,334,160,PC->bPlacing);
 Button(TEXT("Focus"),TEXT("F  FOCUS"),196,334,160);
 if(PC->bPlacing) {
  DrawText(FString::Printf(TEXT("DEST  X %.0f  Y %.0f  Z %.0f"),PC->Preview.X,PC->Preview.Y,PC->Preview.Z),FLinearColor::Yellow,26,377,GEngine->GetSmallFont());
  Button(TEXT("Down"),TEXT("Z -250"),26,405,76); Button(TEXT("Up"),TEXT("Z +250"),108,405,76);
  Button(TEXT("Confirm"),TEXT("CONFIRM"),190,405,85); Button(TEXT("Cancel"),TEXT("CANCEL"),281,405,75);
 } else DrawText(TEXT("RMB: start XYZ order / LMB: select"),Text,26,385,GEngine->GetSmallFont());
 DrawText(TEXT("Prototype units / no combat in this phase"),FLinearColor(0.4f,0.55f,0.65f),26,455,GEngine->GetSmallFont());
 const float Y=Canvas->SizeY/UIScale-95;
 DrawRect(FLinearColor(0.006f,0.014f,0.03f,0.85f),12,Y,Canvas->SizeX/UIScale-24,83);
 DrawText(TEXT("WASD/Arrows pan | Q/E camera height | MMB/Z/C orbit | T/G tilt | Wheel zoom | F focus | Home reset"),Text,26,Y+10,GEngine->GetSmallFont());
 DrawText(TEXT("Order: RMB/R > cursor XY > wheel Z > LMB/Enter confirm | RMB/Backspace cancel | Space pause | 1/2/3 speed"),Text,26,Y+32,GEngine->GetSmallFont());
 DrawText(PC->LastAction,Accent,26,Y+54,GEngine->GetSmallFont());
 for(AOVShip* Ship:PC->Ships) {
  FVector2D Screen;
  if(PC->ProjectWorldLocationToScreen(Ship->GetActorLocation()+FVector(0,0,220),Screen) && Screen.X>390*UIScale) {
   float TextWidth=0,TextHeight=0; GetTextSize(Ship->ShipName,TextWidth,TextHeight,GEngine->GetSmallFont());
   const float LabelX=FMath::Clamp(static_cast<float>(Screen.X/UIScale),390.f,FMath::Max(390.f,Canvas->SizeX/UIScale-TextWidth-12));
   DrawText(Ship->ShipName,Ship->bFriendly?Accent:FLinearColor(1,0.4f,0.3f),LabelX,Screen.Y/UIScale,GEngine->GetSmallFont());
  }
 }
 Canvas->Canvas->PopTransform();
}
