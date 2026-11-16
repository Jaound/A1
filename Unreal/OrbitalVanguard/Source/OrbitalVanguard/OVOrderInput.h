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
 void Sample(FVector& Preview,const FVector& RawXY,float MouseY,bool Altitude,bool Blocked,float UnitsPerPixel=10.f,
             double XYLimit=6000, double MinZ=-1500, double MaxZ=4000)
 {
  if(Blocked) { bBlocked=true; return; }
  if(bBlocked) { bReanchor=true; bBlocked=false; AnchorY=MouseY; AnchorZ=Preview.Z; }
  if(Altitude) {
   if(!bAltitude || bReanchor) { AnchorY=MouseY; AnchorZ=Preview.Z; bReanchor=false; }
   const float Pixels=AnchorY-MouseY;
   if(FMath::Abs(Pixels)>=8 || bShowAltitude) { bShowAltitude=true; Preview.Z=FMath::Clamp(AnchorZ+Pixels*UnitsPerPixel,MinZ,MaxZ); }
  } else {
   if(bAltitude || bReanchor) { XYOffset=Preview-RawXY; XYOffset.Z=0; bReanchor=false; }
   Preview.X=FMath::Clamp(RawXY.X+XYOffset.X,-XYLimit,XYLimit);
   Preview.Y=FMath::Clamp(RawXY.Y+XYOffset.Y,-XYLimit,XYLimit);
  }
  bAltitude=Altitude;
 }
 static bool Project(const FVector& Origin,const FVector& Direction,double PlaneZ,FVector& Point,double MinDistance=350.0, double MaxDistance=100000.0)
 {
  if(Origin.ContainsNaN() || Direction.ContainsNaN() || FMath::Abs(Direction.Z)<0.15) return false;
  const double T=(PlaneZ-Origin.Z)/Direction.Z;
  if(T<MinDistance || T>MaxDistance) return false;
  Point=Origin+Direction*T; return !Point.ContainsNaN();
 }
};
