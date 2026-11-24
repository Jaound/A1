#include "OVHUD.h"
#include "OVCarrierSubsystem.h"
#include "OVController.h"
#include "OVInteriorSubsystem.h"
#include "OVShip.h"
#include "OVShipPreview.h"
#include "OVTransportSubsystem.h"
#include "OVShipSystemsComponent.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"

namespace
{
FName CommandId(const TCHAR* Prefix,int32 Id)
{
    return FName(*(FString(Prefix)+FString::FromInt(Id)));
}
}

bool AOVHUD::PickingShip() const
{
    const auto* PC=Cast<AOVController>(PlayerOwner);
    return PC && (PC->InputMode==EOVInputMode::BoardingTarget || PC->InputMode==EOVInputMode::DockingTarget);
}

bool AOVHUD::AcceptShipPick(AOVShip* Ship)
{
    auto* PC=Cast<AOVController>(PlayerOwner);
    if(!PC||!PickingShip()||PickerOpenedFrame==GFrameCounter)return false;
    const bool Dock=PC->InputMode==EOVInputMode::DockingTarget;
    if(!IsValid(Ship)||Ship->IsHidden() || !Ship->Systems || Ship->bFriendly!=Dock)return false;
    PC->InputMode=EOVInputMode::None;
    const bool Sent=Dock?PC->RequestSmallShipDock(PendingBay,Ship):PC->PrepareBoarding(BoardingOrigin.Get(),Ship,BoardingShuttle.Get(),BoardingSelection);
    InteriorFeedback=PC->LastAction;
    return Sent;
}

bool AOVHUD::HandleShipOperations(FName Action)
{
    const FString Name=Action.ToString();
    if(!Name.StartsWith(TEXT("Ops")))return false;
    auto* PC=Cast<AOVController>(PlayerOwner);
    if(!PC||!PC->Carrier||!PC->Interior)return true;
    const int32 Id=Name.Contains(TEXT("_"))?FCString::Atoi(*Name.RightChop(Name.Find(TEXT("_"))+1)):0;
    auto* Bay=PC->Carrier->Hangars.FindByPredicate([Id](const auto& H){return H.HangarId==Id;});
    auto EnterInterior=[&](AOVShip* Ship){
        if(!IsValid(Ship))return;
        if(InteriorShip!=Ship){InteriorSelectedUnits.Reset();InteriorDeck=1;InteriorTargetSlot=NAME_None;InteriorCardColumn=0;InteriorTeamFilter=INDEX_NONE;}
        InteriorShip=Ship;bDetailsOpen=false;bHeroPanelOpen=false;bInteriorOpen=true;bInteriorMinimized=false;
        if(Ship->bFriendly)PC->Interior->AddShip(Ship);
    };
    if(Action==TEXT("OpsArenaInterior") && !PC->Ships.IsEmpty())EnterInterior(PC->Ships[0]);
    else if(Action==TEXT("OpsInterior"))EnterInterior(PC->Selected);
    else if(Action==TEXT("OpsExterior")||Action==TEXT("OpsBays")){
        if(InteriorShip.IsValid())PC->SelectShip(InteriorShip.Get());
        OpenDetails();bHangarView=Action==TEXT("OpsBays");bAircraftOnly=false;
    }else if(Action==TEXT("OpsHost") && PC->Selected && PC->Selected->HangarHost.IsValid()){
        PC->SelectShip(PC->Selected->HangarHost.Get());OpenDetails();bHangarView=true;bAircraftOnly=false;
    }else if(Name.StartsWith(TEXT("OpsAircraftList_")))bAircraftOnly=true;
    else if(Action==TEXT("OpsAllBays"))bAircraftOnly=false;
    else if(Action==TEXT("OpsPrevBays"))BayPage=FMath::Max(0,BayPage-1);
    else if(Action==TEXT("OpsNextBays"))++BayPage;
    else if(Action==TEXT("OpsCloseRoutes"))bChoosingTransfer=bChoosingTransport=false;
    else if(Action==TEXT("OpsAssault")||Action==TEXT("OpsTransfer")){
        BoardingSelection=InteriorSelectedUnits;BoardingOrigin=InteriorShip;BoardingShuttle.Reset();
        if(!BoardingOrigin.IsValid()||BoardingSelection.IsEmpty()){InteriorFeedback=TEXT("먼저 영웅 또는 병력을 선택하세요");PC->LastAction=InteriorFeedback;return true;}
        PC->Carrier->RegisterSmallBays(BoardingOrigin.Get());
        bChoosingTransport=Action==TEXT("OpsAssault");bChoosingTransfer=!bChoosingTransport;
        if(bChoosingTransport){
            const bool Pod=BoardingOrigin->Systems->GetEfficiency(EOVFacilityType::Boarding)>0 && !BoardingOrigin->HangarHost.IsValid();
            TArray<AOVShip*> Routes;
            if(BoardingOrigin->HullDefinitionId==TEXT("assault")&&!BoardingOrigin->HangarHost.IsValid())Routes.Add(BoardingOrigin.Get());
            for(const auto& H:PC->Carrier->Hangars)if(H.Carrier==BoardingOrigin&&H.StoredSmallShip.IsValid()&&H.StoredSmallShip->HullDefinitionId==TEXT("assault")&&PC->Carrier->IsHangarOperational(H))Routes.Add(H.StoredSmallShip.Get());
            if(Routes.Num()+int32(Pod)==1){
                bChoosingTransport=false;BoardingShuttle=Routes.IsEmpty()?nullptr:Routes[0];PC->InputMode=EOVInputMode::BoardingTarget;PickerOpenedFrame=GFrameCounter;
            }
        }
    }else if(Action==TEXT("OpsPods")||Action==TEXT("OpsSelf")||Name.StartsWith(TEXT("OpsVia_"))){
        if(!bChoosingTransport)return true;
        BoardingShuttle=Action==TEXT("OpsSelf")?BoardingOrigin.Get():Bay?Bay->StoredSmallShip.Get():nullptr;
        bChoosingTransport=false;PC->InputMode=EOVInputMode::BoardingTarget;PickerOpenedFrame=GFrameCounter;
    }else if(Name.StartsWith(TEXT("OpsCross_")) && bChoosingTransfer && Bay){
        auto* To=Bay->Carrier==BoardingOrigin?Bay->StoredSmallShip.Get():Bay->Carrier.Get();
        PC->TransferDockedCrew(BoardingOrigin.Get(),To,BoardingSelection);bChoosingTransfer=false;InteriorFeedback=PC->LastAction;
    }else if(Name.StartsWith(TEXT("OpsReady_")))PC->ConfirmBoarding(Id);
    else if(Name.StartsWith(TEXT("OpsCancel_")))PC->CancelCrewOperation(Id);
    else if(Bay && Bay->Carrier==PC->Selected && PC->Selected->bFriendly && bHangarView){
        if((Name.StartsWith(TEXT("OpsShip_"))||Name.StartsWith(TEXT("OpsImage_"))) && Bay->StoredSmallShip.IsValid()){
            PC->SelectShip(Bay->StoredSmallShip.Get());OpenDetails();bHangarView=false;
        }else if(Name.StartsWith(TEXT("OpsLaunch_"))){
            if(PC->LaunchSmallShip(Id)){OpenDetails();bHangarView=false;}
        }else if(Name.StartsWith(TEXT("OpsDock_"))){
            PendingBay=Id;PC->InputMode=EOVInputMode::DockingTarget;PickerOpenedFrame=GFrameCounter;
        }else if(Name.StartsWith(TEXT("OpsAbortDock_"))) {if(auto* In=Bay->InboundShip.Get())In->StopMovement();Bay->InboundShip.Reset();}
        else if(Name.StartsWith(TEXT("OpsWingMode_")))PC->Carrier->SetHangarMode(Id,EOVHangarMode::Aircraft);
    }
    return true;
}

bool AOVHUD::DrawSmallBays(AOVShip* Home,float X,float Y,float W)
{
    auto* PC=CastChecked<AOVController>(PlayerOwner);
    if(!Home->bFriendly || !PC->Carrier)return false;
    PC->Carrier->RegisterSmallBays(Home);
    if(bAircraftOnly){
        Button(TEXT("OpsAllBays"),TEXT("전체 격납고"),X+10,Y+72,105);return false;
    }
    const auto Bays=PC->Carrier->Hangars.FilterByPredicate([Home](const auto& H){return H.Carrier==Home;});
    if(Bays.IsEmpty())return false;
    const FLinearColor Accent(.23f,.91f,1),Ink(.83f,.89f,.94f);
    DrawText(TEXT("격납고 · 보관 중인 전력"),Accent,X+12,Y+77,OVTypography::Font(EOVTextRole::Heading));
    const int32 Cols=W>=580?2:1;
    const float Width=(W-24)/Cols-5;
    if(BayPageHost!=Home){BayPageHost=Home;BayPage=0;}
    const int32 Capacity=Cols*FMath::Max(1,FMath::FloorToInt((WindowSize.Y-117)/74));
    const int32 Pages=FMath::DivideAndRoundUp(Bays.Num(),Capacity);
    BayPage=FMath::Clamp(BayPage,0,Pages-1);
    if(BayPage>0)Button(TEXT("OpsPrevBays"),TEXT("이전"),X+W-126,Y+72,54);
    if(BayPage+1<Pages)Button(TEXT("OpsNextBays"),TEXT("다음"),X+W-68,Y+72,54);
    for(int32 I=BayPage*Capacity;I<FMath::Min(Bays.Num(),(BayPage+1)*Capacity);++I){
        const auto& B=Bays[I];const int32 Id=B.HangarId;
        const float CX=X+10+(I%Cols)*(Width+5),CY=Y+108+((I%Capacity)/Cols)*74;
        DrawRect(FLinearColor(.032f,.071f,.1f),CX,CY,Width,70);
        const bool Ready=PC->Carrier->IsHangarOperational(B);
        DrawText(FString::Printf(TEXT("격납고 %d · %s"),I+1,Ready?TEXT("가동"):TEXT("정지")),Ink,CX+5,CY+2,OVTypography::Font(EOVTextRole::Body));
        if(auto* Guest=B.StoredSmallShip.Get()){
            auto* Art=SelectionPortraits.FindRef(Guest->HullDefinitionId).Get();
            if(!Art && FApp::CanEverRender()){
                auto* Preview=NewObject<UOVShipPreview>(this);Preview->Update(Guest,UOVShipPreview::DefaultHullView(),1.12f,2.8f,INDEX_NONE,384,true);
                Art=Preview->GetTexture();if(Art)SelectionPortraits.Add(Guest->HullDefinitionId,Art);Preview->Release();
            }
            const auto Key=CommandId(TEXT("OpsShip_"),Id);
            if(Art)DrawTexture(Art,CX+4,CY+28,65,65/2.8f,0,0,1,1,FLinearColor::White,BLEND_Opaque);
            AddHitBox(FVector2D(CX+4,CY+21)*UIScale,FVector2D(65,39)*UIScale,CommandId(TEXT("OpsImage_"),Id),true,111);
            Button(Key,Guest->ShipName,CX+71,CY+22,Width-135);
            const auto Count=PC->Interior->GetUnitsOnShip(Guest,0).Num();
            DrawText(FString::Printf(TEXT("선체 %.0f%% · 탑승 %d명"),Guest->Systems->HullHP*100/FMath::Max(1.f,Guest->Systems->MaxHullHP),Count),Ink,CX+72,CY+50,OVTypography::Font(EOVTextRole::Numeric));
            if(Ready)Button(CommandId(TEXT("OpsLaunch_"),Id),TEXT("출격"),CX+Width-58,CY+22,54);
        }else if(B.InboundShip.IsValid()){
            Button(CommandId(TEXT("OpsAbortDock_"),Id),TEXT("접근 취소"),CX+6,CY+25,94);
            DrawText(B.InboundShip->ShipName,Ink,CX+108,CY+31,OVTypography::Font(EOVTextRole::Body));
        }else{
            const bool Wing=B.Mode==EOVHangarMode::Aircraft;
            const int32 Assigned=PC->Carrier->Aircraft.FilterByPredicate([Id](const auto& A){return A.HangarId==Id && A.State!=EOVAircraftState::Wrecked;}).Num();
            if(Wing)Button(CommandId(TEXT("OpsAircraftList_"),Id),FString::Printf(TEXT("함재기 %d대 보기"),Assigned),CX+5,CY+25,132);
            if(Ready&&!Assigned)Button(CommandId(TEXT("OpsDock_"),Id),TEXT("소형함 수납"),CX+(Wing?140:5),CY+25,105);
            if(Ready&&!Wing&&Home->Systems->Layout->bAllowAircraftOperations)Button(CommandId(TEXT("OpsWingMode_"),Id),TEXT("함재기용 전환"),CX+116,CY+25,122);
        }
    }
    return true;
}

void AOVHUD::DrawShipOperations()
{
    auto* PC=CastChecked<AOVController>(PlayerOwner);
    const float Height=ViewportPixels.Y/UIScale;
    float Bottom=Height-CommandPanelHeight()-14;
    if(PC->Transport) for(const auto& Shuttle:PC->Transport->Shuttles){
        if(!Shuttle.bDockBlocked || Shuttle.FactionId!=0 || !Shuttle.Ship.IsValid())continue;
        Bottom-=32;
        DrawRect(FLinearColor(.12f,.048f,.014f,.98f),12,Bottom,480,30);
        DrawText(Shuttle.Ship->ShipName+TEXT(" · ")+Shuttle.Reason,FLinearColor(1,.77f,.34f),20,Bottom+5,OVTypography::Font(EOVTextRole::Button));
    }
    for(const auto& Op:PC->CrewOperations){
        if(!Op.Origin.IsValid()||!Op.Target.IsValid())continue;
        const int32 N=PC->Mustered(Op);
        const bool Pods=!Op.bTransfer && !Op.Shuttle.IsValid();
        const int32 Lines=Pods?FMath::DivideAndRoundUp(Op.Units.Num(),3):0;
        const float H=40+(Pods?24+Lines*18:0);
        const float RowY=Bottom-H;
        DrawRect(FLinearColor(.015f,.045f,.06f,.98f),12,RowY,480,H);
        AddHitBox(FVector2D(12,RowY)*UIScale,FVector2D(480,H)*UIScale,CommandId(TEXT("OpsCrewBG_"),Op.Serial),true,225);
        if(Pods){
            const bool Shield=Op.Target->Systems->bShieldActive && Op.Target->Systems->Shield>0;
            DrawText(Shield?TEXT("경고 · 적 쉴드 활성 / 접촉 시 포드 소실"):TEXT("적 쉴드 비활성 · 포드 발사 순서"),Shield?FLinearColor(1,.63f,.2f):FLinearColor(.5f,.95f,.86f),19,RowY+38,OVTypography::Font(EOVTextRole::Body));
            for(int32 I=0;I<Op.Units.Num();++I){
                const auto Unit=PC->Interior->GetUnit(Op.Units[I]);
                const FString Name=Unit.Role==EOVInteriorRole::Trooper?FString::Printf(TEXT("일반병 #%d"),Unit.UnitId):Unit.Name;
                const FString Label=FString::Printf(TEXT("%02d  %s"),I+1,*Name);
                DrawText(FitLabel(OVTypography::Font(EOVTextRole::Body),Label,150),FLinearColor::White,19+(I%3)*156,RowY+60+(I/3)*18,OVTypography::Font(EOVTextRole::Body));
            }
        }
        DrawText(FString::Printf(TEXT("%s · %s%s  %d/%d"),Op.bTransfer?TEXT("승하선"):Op.Shuttle.IsValid()?TEXT("승선 준비"):TEXT("포드 탑승"),Op.bTransfer?TEXT(""):TEXT("적 "),*Op.Target->ShipName,N,Op.Units.Num()),FLinearColor::White,18,RowY+9,OVTypography::Font(EOVTextRole::Body));
        if(!Op.bTransfer && N==Op.Units.Num())Button(CommandId(TEXT("OpsReady_"),Op.Serial),Op.Shuttle.IsValid()?TEXT("출격 확인"):TEXT("발사 확인"),288,RowY+6,100);
        Button(CommandId(TEXT("OpsCancel_"),Op.Serial),TEXT("취소"),394,RowY+6,80);Bottom=RowY-3;
    }
    if(!bChoosingTransport&&!bChoosingTransfer)return;
    const float X=ViewportPixels.X/UIScale*.5f-155,Y=100;
    TGuardValue<bool> Layer(bDrawingOperationsModal,true);
    TArray<TPair<FName,FString>> Choices;
    if(BoardingOrigin.IsValid()){
        if(bChoosingTransport && !BoardingOrigin->HangarHost.IsValid()){
            if(BoardingOrigin->Systems->GetEfficiency(EOVFacilityType::Boarding)>0)Choices.Emplace(TEXT("OpsPods"),TEXT("개인 포드"));
            if(BoardingOrigin->HullDefinitionId==TEXT("assault"))Choices.Emplace(TEXT("OpsSelf"),BoardingOrigin->ShipName);
        }
        for(const auto& H:PC->Carrier->Hangars){
            auto* Guest=H.StoredSmallShip.Get();
            if(!Guest||!PC->Carrier->IsHangarOperational(H))continue;
            if(bChoosingTransport && H.Carrier==BoardingOrigin && Guest->HullDefinitionId==TEXT("assault"))Choices.Emplace(CommandId(TEXT("OpsVia_"),H.HangarId),Guest->ShipName);
            if(bChoosingTransfer && (H.Carrier==BoardingOrigin || Guest==BoardingOrigin))Choices.Emplace(CommandId(TEXT("OpsCross_"),H.HangarId),Guest==BoardingOrigin?H.Carrier->ShipName:Guest->ShipName);
        }
    }
    const float H=82+Choices.Num()*32;
    DrawRect(FLinearColor(.016f,.045f,.065f,1),X,Y,310,H);
    AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(310,H)*UIScale,TEXT("OpsBackground"),true,275);
    DrawText(Choices.IsEmpty()?TEXT("이용 가능한 연결이 없습니다"):bChoosingTransfer?TEXT("옮겨 탈 함선 선택"):TEXT("강습 수단 선택"),FLinearColor::White,X+9,Y+8,OVTypography::Font(EOVTextRole::Heading));
    float CY=Y+38;for(const auto& Choice:Choices){Button(Choice.Key,Choice.Value,X+10,CY,290);CY+=32;}
    Button(TEXT("OpsCloseRoutes"),TEXT("취소"),X+10,CY,290);
}
