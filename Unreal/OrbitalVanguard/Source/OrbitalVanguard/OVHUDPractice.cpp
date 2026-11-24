#include "OVHUD.h"
#include "OVController.h"
#include "OVPlayScenarioSubsystem.h"
#include "OVTitleScreenWidget.h"
#include "OVShip.h"
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
 Button("PracticeReset",TEXT("장 다시 실습"),x+80,y+122,100);
 if(done&&play->PracticeChapter<8)Button("PracticeFollowing",TEXT("다음 장"),x+width-95,y+122,88,true);
 else if(!done&&play->bPracticeReady)Button("PracticeAdvance",TEXT("다음"),x+width-95,y+122,88,true);
}
bool AOVHUD::PracticeAction(FName action)
{
 if(!action.ToString().StartsWith(TEXT("Practice")))return false;
 auto* pc=Cast<AOVController>(PlayerOwner);auto* p=pc?pc->GetPlayScenario():nullptr;
 if(!p||p->PracticeChapter<0||bPauseMenu||bTitleScreenOpen||bStarMapOpen)return false;
 if(action=="PracticeIndex"){pc->LeavePlayScenario();OpenTitleScreen();if(TitleScreenWidget)TitleScreenWidget->OpenPractice();}
 else if(action=="PracticeReset")p->StartPractice(pc,p->PracticeChapter);
 else if(action=="PracticeFollowing"&&p->PracticeStep==p->PracticeStepCount())p->StartPractice(pc,p->PracticeChapter+1);
 else if(action=="PracticeAdvance")p->AdvancePractice();
 else if(action=="PracticeEquip")p->PracticeEquipment(p->PracticeStep==5);
 else if(action=="PracticeHints"){
    p->bPracticeHints=!p->bPracticeHints;
    GConfig->SetBool(TEXT("OrbitalVanguard.Practice"),TEXT("Hints"),p->bPracticeHints,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);
 }
 return true;
}
