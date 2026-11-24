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
 const float width=FMath::Min(380.f,float(ViewportPixels.X/UIScale)-12);
 const float x=ViewportPixels.X/UIScale-width-6,y=66;
 const FLinearColor accent(.18,.82,.95),paper(.018,.038,.067,.96);
 const auto body=OVTypography::Font(EOVTextRole::Body);
 DrawRect(paper,x,y,width,play->bPracticeHints?158:58);
 DrawRect(accent,x,y,3,play->bPracticeHints?158:58);
 DrawText(FString::Printf(TEXT("%d장  %s    %d / %d"),play->PracticeChapter+1,*play->PracticeTitle(play->PracticeChapter),FMath::Min(play->PracticeStep+1,play->PracticeStepCount()),play->PracticeStepCount()),accent,x+10,y+3,body);
 DrawText(play->PracticeObjective(),FLinearColor::White,x+10,y+25,OVTypography::Font(EOVTextRole::Heading));
 Button("PracticeHints",play->bPracticeHints?TEXT("접기"):TEXT("힌트"),x+width-52,y+7,46);
 if(play->HasPracticeMarker()){
    FVector2D p;
    if(pc->ProjectWorldLocationToScreen(play->PracticeGoal,p,true)){
       p/=UIScale;
       DrawText(FString::Printf(TEXT("◇ 이동 목표 · 고도 %.0fm"),play->PracticeGoal.Z/100),accent,p.X-45,p.Y-10,body);
    }
 }
 if(!play->bPracticeHints)return;
 if(!play->bPracticeReady && !done){
    FVector2D focus=GetActionCenter(play->PracticeControl());float span=29;
    if(auto* s=play->PracticeFocus()){FVector2D pixels;if(pc->ProjectWorldLocationToScreen(s->GetActorLocation(),pixels,true)){focus=pixels/UIScale;span=22;}}
    if(focus.X>=0 && focus.Y>=0){
       for(int side:{-1,1})for(int vertical:{-1,1}){
          const FVector2D corner=focus+FVector2D(side*span,vertical*13);
          DrawLine(corner.X,corner.Y,corner.X-side*8,corner.Y,accent,1.4f);
          DrawLine(corner.X,corner.Y,corner.X,corner.Y-vertical*6,accent,1.4f);
       }
    }
 }
 const FString tip=play->PracticeInput();int32 start=0;
 for(int line=0;line<3 && start<tip.Len();++line){
    int end=start+1;float w=0,h=0;
    while(end<=tip.Len()){GetTextSize(tip.Mid(start,end-start),w,h,body);if(w>width-26)break;++end;}
    end=FMath::Max(start+1,end-1);DrawText(tip.Mid(start,end-start),FLinearColor::White,x+10,y+49+line*16,body);start=end;
 }
 if(play->PracticeChapter==5 && (play->PracticeStep==4||play->PracticeStep==5))
    Button("PracticeEquip",play->PracticeStep==4?TEXT("호환 장비 장착"):TEXT("상위 티어 시험"),x+10,y+91,145);
 else DrawText(play->bPracticeReady?TEXT("✓ 목표 달성"):done?TEXT("✓ 완료 기록됨"):pc->bSimulationPaused?TEXT("일시정지 · Space로 재개"):TEXT("실습 중"),accent,x+10,y+95,body);
 Button("PracticeIndex",TEXT("장 목록"),x+8,y+122,68);
 Button(done?"PracticeReset":"PracticeRetry",done?TEXT("장 다시 실습"):play->PracticeChapter==8?TEXT("강습 다시 실습"):TEXT("단계 다시 시도"),x+80,y+122,116);
 if(done&&play->PracticeChapter<8)Button("PracticeFollowing",TEXT("다음 장"),x+width-95,y+122,88,true);
 else if(!done&&play->bPracticeReady)Button("PracticeAdvance",TEXT("다음"),x+width-95,y+122,88,true);
 if(!done && (play->PracticeChapter==4||play->PracticeChapter==3)){
   Button("PracticeMore",play->bPracticeDetail?TEXT("상세 접기"):play->PracticeChapter==4?TEXT("함종 카드"):TEXT("무장 비교"),x+width-110,y+160,100);
   if(play->bPracticeDetail){
    DrawRect(paper,x,y+190,width,112);
    if(play->PracticeChapter==4){
     const FName ids[]={"corvette","air_defense","radar","battleship","carrier","dreadnought"};const int n=play->PracticeRoleCard;
     const TCHAR* notes[]={TEXT("전방 차폐와 정찰 · 동일 계열 무장"),TEXT("주력함 호위 · 빔 및 실탄 요격"),TEXT("공유 탐지 · 장거리 표적 지원"),TEXT("지속 화력 · 다수의 주포"),TEXT("항공 작전 · 격납고와 관제실"),TEXT("결전 전력 · 전용 주포")};
     if(auto* picture=HullCardImage(ids[n]))DrawTexture(picture,x+8,y+198,112,40,0,0,1,1);
     FOVHullMetadata info;OVCalibration::GetHullMetadata(ids[n],info);
     const TCHAR* sizes[]={TEXT(""),TEXT("소형"),TEXT("중형"),TEXT("대형"),TEXT("초대형")};
     DrawText(OVCalibration::GetHullName(ids[n])+TEXT(" · ")+sizes[info.SizeClass],accent,x+125,y+197,body);
     DrawText(notes[n],FLinearColor::White,x+12,y+240,body);Button("PracticeRole",TEXT("다른 함종"),x+width-110,y+267,100);
    }else{
     DrawText(TEXT("빔  즉시 판정     함포  직선 탄도     미사일  곡선 접근"),accent,x+8,y+196,body);
     auto* ship=pc->Selected.Get();if(ship){
      int blind=0,ready=0;for(const auto& m:pc->Combat->GetShipState(ship).Mounts){const auto r=pc->Combat->GetReadiness(ship,m.MountId);blind+=r.Reason==TEXT("MOUNT BLIND ARC");ready+=r.bReady;}
      DrawText(FString::Printf(TEXT("선택함   사격 가능 %d문   ·   장착면 사각 %d문"),ready,blind),FLinearColor::White,x+8,y+230,body);
     }DrawText(TEXT("표적의 고도를 바꾸면 상·하부 포탑의 사각이 달라집니다."),FLinearColor::White,x+8,y+256,body);
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
 else if(action=="PracticeEquip")p->PracticeEquipment(p->PracticeStep==5);
 else if(action=="PracticeMore")p->bPracticeDetail=!p->bPracticeDetail;
 else if(action=="PracticeRole")p->PracticeRoleCard=(p->PracticeRoleCard+1)%6;
 else if(action=="PracticeHints"){
    p->bPracticeHints=!p->bPracticeHints;
    GConfig->SetBool(TEXT("OrbitalVanguard.Practice"),TEXT("Hints"),p->bPracticeHints,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);
 }
 return true;
}
