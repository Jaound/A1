#include "OVHUD.h"
#include "OVController.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVTitleScreenWidget.h"
#include "OVShip.h"
#include "OVCalibration.h"
#include "OVCombatSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "OVTypography.h"
#include "Misc/ConfigCacheIni.h"

void AOVHUD::DrawPractice()
{
 auto* pc=Cast<AOVController>(PlayerOwner);auto* play=pc?pc->GetPlayScenario():nullptr;
 if(!play||play->PracticeChapter<0)return;
 play->ObservePractice();
 const bool done=play->PracticeStep>=play->PracticeStepCount();
 TGuardValue<bool> layer(bDrawingPractice,true);
 const float width=FMath::Min(412.f,float(ViewportPixels.X/UIScale));
 const FLinearColor accent(.18,.82,.95),paper(.018,.038,.067,.96);
 const auto body=OVTypography::Font(EOVTextRole::Body);
 const FString tip=play->PracticeInput();
 if(tip!=LastPracticeTip || width!=LastPracticeWidth){
    LastPracticeTip=tip;LastPracticeWidth=width;PracticeTipLines.Reset();
    TArray<FString> paragraphs;tip.ParseIntoArrayLines(paragraphs,false);
    for(const FString& paragraph:paragraphs){
       FString rest=paragraph.TrimStartAndEnd();
       while(!rest.IsEmpty()){
          int32 lo=1,hi=rest.Len();
          while(lo<hi){
             const int32 mid=(lo+hi+1)/2;float w=0,h=0;GetTextSize(rest.Left(mid),w,h,body);
             if(w<=width-24)lo=mid;else hi=mid-1;
          }
          int32 cut=lo;
          if(lo<rest.Len()){
             int32 space=INDEX_NONE;
             if(rest.Left(lo+1).FindLastChar(' ',space) && space>0)cut=space;
          }
          PracticeTipLines.Add(rest.Left(cut));rest=rest.Mid(cut).TrimStart();
       }
    }
 }
 const float status=54.f+PracticeTipLines.Num()*17.f;
 const float footer=status+33.f;
 const bool more=play->PracticeChapter==4||(!done&&play->PracticeChapter==3);
 const float detailHeight=play->PracticeChapter==4?132.f:112.f;
 const float height=play->bPracticeHints?footer+34.f+(more?32.f:0.f)+(more&&play->bPracticeDetail?detailHeight:0.f):55.f;
 const float x=0;
 float y=63;
 const FName goal=play->PracticeGoalId();
 FName action=play->PracticeControl();
 auto firstVisible=[&](const TCHAR* prefix){
    FName best;int32 bestId=MAX_int32;
    for(const auto& pair:PracticeButtonWidths){
       const FString name=pair.Key.ToString();
       if(name.StartsWith(prefix)){
          const int id=FCString::Atoi(*name.Mid(FCString::Strlen(prefix)));
          if(id<bestId){best=pair.Key;bestId=id;}
       }
    }
    return best;
 };
 if(goal=="Launch"){
    if(!bDetailsOpen)action="Details";
    else if(!bHangarView)action="HangarView";
    else if(!bAircraftOnly)action=firstVisible(TEXT("OpsAircraftList_"));
    else if(!bAircraftGrid)action="HangarDisplayMode";
    else if(!SelectedAircraftId)action=firstVisible(TEXT("AircraftCard_"));
 }
 if(goal=="Hangar" && !bDetailsOpen)action="Details";
 if(goal=="Reject")action="PracticeEquip";
 FVector2D focus=GetActionCenter(action);
 FVector2D half(24,15);
 if(const float* w=PracticeButtonWidths.Find(action))half.X=*w*.5f+2;
 if(focus.X<0){
    if(auto* ship=play->PracticeFocus()){
       FVector2D p;if(pc->ProjectWorldLocationToScreen(ship->GetActorLocation(),p,true))focus=p/UIScale;
    }
 }
 // Leave the target visible when a movable window places its control under the lesson.
 if(!play->bPracticeReady && focus.X>=0 && focus.X-half.X<width && focus.Y+half.Y>y && focus.Y-half.Y<y+height){
    const float below=focus.Y+half.Y+5;
    if(below+height<ViewportPixels.Y/UIScale)y=below;
 }
 DrawRect(paper,x,y,width,height);
 DrawRect(accent,x,y,3,height);
 DrawText(FString::Printf(TEXT("%d장  %s    %d / %d"),play->PracticeChapter+1,*play->PracticeTitle(play->PracticeChapter),FMath::Min(play->PracticeStep+1,play->PracticeStepCount()),play->PracticeStepCount()),accent,x+10,y+3,body);
 DrawText(play->PracticeObjective(),FLinearColor::White,x+10,y+25,OVTypography::Font(EOVTextRole::Heading));
 Button("PracticeHints",play->bPracticeHints?TEXT("접기"):TEXT("힌트"),x+width-52,y+4,46);
 if(play->HasPracticeMarker()){
    FVector2D p;
    if(pc->ProjectWorldLocationToScreen(play->PracticeGoal,p,true)){
       p/=UIScale;
       DrawText(FString::Printf(TEXT("◇ 이동 목표 · 고도 %.0fm"),play->PracticeGoal.Z/100),accent,p.X-45,p.Y-10,body);
    }
 }
 if(!play->bPracticeHints)return;
 if(!play->bPracticeReady && !done && focus.X>=0 && focus.Y>=0){
    for(int side:{-1,1})for(int vertical:{-1,1}){
       const FVector2D corner=focus+FVector2D(side*half.X,vertical*half.Y);
       DrawLine(corner.X,corner.Y,corner.X-side*10,corner.Y,accent,2);
       DrawLine(corner.X,corner.Y,corner.X,corner.Y-vertical*8,accent,2);
    }
 }
 for(int32 i=0;i<PracticeTipLines.Num();++i)
    DrawText(PracticeTipLines[i],FLinearColor::White,x+10,y+50+i*17,body);
 if(!play->bPracticeReady && (goal=="Equip"||goal=="Reject"))
    Button("PracticeEquip",goal=="Equip"?TEXT("호환 장비 장착"):TEXT("상위 티어 시험"),x+10,y+status,145);
 else if(play->bPracticeReady||done){
    DrawLine(x+11,y+status+12,x+15,y+status+16,accent,2.f);
    DrawLine(x+15,y+status+16,x+22,y+status+7,accent,2.f);
    DrawText(done?TEXT("완료 기록됨"):TEXT("목표 달성"),accent,x+28,y+status+3,body);
 }else DrawText(pc->bSimulationPaused?TEXT("일시정지 · Space로 재개"):TEXT("실습 중"),accent,x+10,y+status+3,body);
 Button("PracticeIndex",TEXT("장 목록"),x+8,y+footer,68);
 Button(done?"PracticeReset":"PracticeRetry",done?TEXT("장 다시 실습"):play->PracticeChapter==8?TEXT("강습 다시 실습"):TEXT("단계 다시 시도"),x+80,y+footer,116);
 if(done&&play->PracticeChapter<8)Button("PracticeFollowing",TEXT("다음 장"),x+width-95,y+footer,88,true);
 else if(!done&&play->bPracticeReady)Button("PracticeAdvance",TEXT("다음"),x+width-95,y+footer,88,true);
 if(more){
   const float details=y+footer+33;
   Button("PracticeMore",play->bPracticeDetail?TEXT("상세 접기"):play->PracticeChapter==4?TEXT("함종 카드"):TEXT("무장 비교"),x+width-110,details,100);
   if(play->bPracticeDetail){
    DrawRect(paper,x,details+32,width,detailHeight);
    if(play->PracticeChapter==4){
     static const TArray<FName> hulls=OVCalibration::GetHullIds();
     if(!hulls.IsValidIndex(play->PracticeRoleCard))return;
     const FName hull=hulls[play->PracticeRoleCard];
     FOVHullMetadata info;OVCalibration::GetHullMetadata(hull,info);
     const TCHAR* sizes[]={TEXT(""),TEXT("소형"),TEXT("중형"),TEXT("대형"),TEXT("초대형")};
     if(auto* picture=HullCardImage(hull)){
        const float aspect=float(picture->SizeX)/FMath::Max(1,picture->SizeY);
        const float h=FMath::Min(64.f,112.f/aspect);
        DrawTexture(picture,x+8+(112-h*aspect)*.5f,details+36,h*aspect,h,0,0,1,1);
     }
     DrawText(OVCalibration::GetHullName(hull)+TEXT(" · ")+sizes[FMath::Clamp(info.SizeClass,0,4)],accent,x+125,details+38,body);
     const FString roles=FString::JoinBy(info.RoleTags,TEXT(" · "),[](const FName& tag){return tag.ToString();});
     DrawText(FitLabel(body,roles,width-133),FLinearColor::White,x+125,details+59,body);
     DrawText(FString::Printf(TEXT("공격 %d문 · 요격 %d문 · 격납고 최대 %d"),info.AttackMountCount,info.PointDefenseCount,info.MaxHangarInstallations),FLinearColor::White,x+12,details+101,body);
     Button("PracticeRole",TEXT("다음 함종"),x+8,details+131,90);
     Button("PracticePilot",TEXT("직접 조종"),x+width-108,details+131,100,true);
    }else{
     DrawText(TEXT("빔  즉시 판정     함포  직선 탄도     미사일  곡선 접근"),accent,x+8,details+38,body);
     auto* ship=pc->Selected.Get();if(ship){
      int blind=0,ready=0;for(const auto& m:pc->Combat->GetShipState(ship).Mounts){const auto r=pc->Combat->GetReadiness(ship,m.MountId);blind+=r.Reason==TEXT("MOUNT BLIND ARC");ready+=r.bReady;}
      DrawText(FString::Printf(TEXT("선택함   사격 가능 %d문   ·   장착면 사각 %d문"),ready,blind),FLinearColor::White,x+8,details+72,body);
     }DrawText(TEXT("표적의 고도를 바꾸면 상·하부 포탑의 사각이 달라집니다."),FLinearColor::White,x+8,details+98,body);
    }
   }
 }
}
bool AOVHUD::PracticeAction(FName action)
{
 if(!action.ToString().StartsWith(TEXT("Practice")))return false;
 auto* pc=Cast<AOVController>(PlayerOwner);auto* p=pc?pc->GetPlayScenario():nullptr;
 if(!p||p->PracticeChapter<0||bPauseMenu||bTitleScreenOpen||bStarMapOpen)return false;
 if(action=="PracticeIndex"){pc->LeavePlayScenario();OpenTitleScreen();if(TitleScreenWidget)TitleScreenWidget->OpenPractice();}
 else if(action=="PracticeRetry")p->RetryPracticeStep();
 else if(action=="PracticeReset")p->StartPractice(pc,p->PracticeChapter);
 else if(action=="PracticeFollowing"&&p->PracticeStep==p->PracticeStepCount())p->StartPractice(pc,p->PracticeChapter+1);
 else if(action=="PracticeAdvance")p->AdvancePractice();
 else if(action=="PracticeEquip")p->PracticeEquipment(p->PracticeGoalId()=="Reject");
 else if(action=="PracticeMore")p->bPracticeDetail=!p->bPracticeDetail;
 else if(action=="PracticePilot")p->PilotPracticeRole();
 else if(action=="PracticeRole")p->PracticeRoleCard=(p->PracticeRoleCard+1)%FMath::Max(1,OVCalibration::GetHullIds().Num());
 else if(action=="PracticeHints"){
    p->bPracticeHints=!p->bPracticeHints;
    GConfig->SetBool(TEXT("OrbitalVanguard.Practice"),TEXT("Hints"),p->bPracticeHints,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);
 }
 return true;
}
