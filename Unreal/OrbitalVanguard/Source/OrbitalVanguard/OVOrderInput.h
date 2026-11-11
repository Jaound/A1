#pragma once
#include "CoreMinimal.h"

/** Cursor projection is independent of simulation time. UI/focus transitions re-anchor. */
struct FOVOrderInput
{
 bool bAltitude=false, bBlocked=true, bReanchor=false, bShowAltitude=false;
 float AnchorY=0;
 double AnchorZ=0;
 FVector XYOffset=FVector::ZeroVector;
 void Reset() { *this=FOVOrderInput(); bBlocked=false; }
 void Sample(FVector& Preview,const FVector& RawXY,float MouseY,bool Altitude,bool Blocked,float UnitsPerPixel=10.f)
 {
  if(Blocked) { bBlocked=true; return; }
  if(bBlocked) { bReanchor=true; bBlocked=false; AnchorY=MouseY; AnchorZ=Preview.Z; }
  if(Altitude) {
   if(!bAltitude || bReanchor) { AnchorY=MouseY; AnchorZ=Preview.Z; bReanchor=false; }
   const float Pixels=AnchorY-MouseY;
   if(FMath::Abs(Pixels)>=8 || bShowAltitude) { bShowAltitude=true; Preview.Z=FMath::Clamp(AnchorZ+Pixels*UnitsPerPixel,-1500.0,4000.0); }
  } else {
   if(bAltitude || bReanchor) { XYOffset=Preview-RawXY; XYOffset.Z=0; bReanchor=false; }
   Preview.X=FMath::Clamp(RawXY.X+XYOffset.X,-6000.0,6000.0);
   Preview.Y=FMath::Clamp(RawXY.Y+XYOffset.Y,-6000.0,6000.0);
  }
  bAltitude=Altitude;
 }
 static bool Project(const FVector& Origin,const FVector& Direction,double PlaneZ,FVector& Point,double MinDistance=350.0)
 {
  if(Origin.ContainsNaN() || Direction.ContainsNaN() || FMath::Abs(Direction.Z)<0.15) return false;
  const double T=(PlaneZ-Origin.Z)/Direction.Z;
  if(T<MinDistance || T>100000.0) return false;
  Point=Origin+Direction*T; return !Point.ContainsNaN();
 }
};
