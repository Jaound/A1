#include "OVHUD.h"

#include "Components/LineBatchComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "OVBasePresentation.h"
#include "OVController.h"
#include "OVShip.h"

namespace
{
const FLinearColor PanelColor(.012f,.023f,.042f,.97f);
const FLinearColor AccentColor(.22f,.83f,.95f);
const TCHAR* ModuleName(OVBase::Kind type) { return OVBase::Def(type).Name; }

FString ResourceText(OVBase::Resources value)
{
    return FString::Printf(TEXT("%d / %d / %d"),value.Common,value.Advanced,value.Rare);
}

const TCHAR* TechnologyName(OVBase::Technology tech)
{
    return OVBase::Technologies[static_cast<int>(tech)].Name;
}

FName CatalogAction(int index) { return FName(*FString::Printf(TEXT("BaseCatalog_%d"),index)); }
FName TechnologyAction(int index) { return FName(*FString::Printf(TEXT("BaseTech_%d"),index)); }
const TCHAR* const WindowLabels[]={TEXT("운영"),TEXT("건설"),TEXT("연구"),TEXT("저장")};
const FName WindowActions[]={TEXT("BaseOperations"),TEXT("BaseBuild"),TEXT("BaseResearch"),TEXT("BaseFiles")};
FString PlacementStatus(OVBase::Error error, const FString& shipName)
{
    switch (error)
    {
    case OVBase::Error::None: return TEXT("건설 가능");
    case OVBase::Error::Ship: return FString::Printf(TEXT("%s이(가) 건설 공간에 있습니다 · 함대 화면에서 이동시키세요."),*shipName);
    case OVBase::Error::Occupied: return TEXT("공간이 이미 사용 중입니다.");
    case OVBase::Error::Seam: return TEXT("양쪽 연결면이 어긋납니다 · 다른 칸이나 방향을 선택하세요.");
    case OVBase::Error::Direction: return TEXT("연결구 방향이 맞지 않습니다 · Q/E 회전");
    case OVBase::Error::NoConnection: return TEXT("맞물리는 연결 포트가 없습니다.");
    case OVBase::Error::Clearance: return TEXT("함선 출입로가 막힙니다 · 전방 여유와 바깥 비행선을 비워 주세요.");
    case OVBase::Error::Funds: return TEXT("보유 자원이 부족합니다.");
    case OVBase::Error::Tier: return TEXT("코어 등급이 필요합니다.");
    case OVBase::Error::Research: return TEXT("선행 연구가 필요합니다.");
    case OVBase::Error::Capacity: return TEXT("저장 공간이 부족합니다.");
    case OVBase::Error::Dependency: return TEXT("다른 예약 공사가 이 연결을 사용 중입니다.");
    case OVBase::Error::Upgrade: return TEXT("증설 가능 상태를 확인하세요.");
    case OVBase::Error::Limit: return TEXT("현재 코어의 설치 한도에 도달했습니다.");
    case OVBase::Error::Busy: return TEXT("시설 건설 슬롯을 사용 중입니다.");
    default: return TEXT("시험 배치 범위를 벗어났습니다.");
    }
}

OVBase::Module UnderPointer(const OVBase::BuildState& build,int32 pick,OVBase::Cell cell,int turns)
{
    OVBase::Module candidate{static_cast<OVBase::Kind>(pick),cell,turns};
    if (!OVBase::HasBay(candidate.Type)) return candidate;
    const auto ports=OVBase::Ports({candidate.Type,{},turns});
    for (const auto& module:build.Modules())
        for (const auto& other:OVBase::Ports(module))
            if (other.At+other.Direction==cell)
                for (const auto& port:ports)
                    if (port.Direction+other.Direction==OVBase::Cell{})
                    {
                        candidate.Origin=cell+OVBase::Cell{-port.At.X,-port.At.Y,-port.At.Z};
                        return candidate;
                    }
    return candidate;
}

bool PointerCell(AOVController& pc,const AOVBasePresentation& base,int32 layer,OVBase::Cell& result)
{
    FVector rayOrigin,rayDirection;
    if (!pc.DeprojectMousePositionToWorld(rayOrigin,rayDirection) || FMath::IsNearlyZero(rayDirection.Z)) return false;
    const FVector origin = base.GetActorLocation();
    const double t = (origin.Z + layer*OVBase::CellCm - rayOrigin.Z) / rayDirection.Z;
    if (t < 0) return false;
    const auto point = (rayOrigin + rayDirection*t - origin) / OVBase::CellCm;
    if (point.ContainsNaN() || FMath::Abs(point.X) > 30 || FMath::Abs(point.Y) > 30) return false;
    result = {FMath::RoundToInt32(point.X),FMath::RoundToInt32(point.Y),layer};
    return true;
}
}

bool AOVHUD::BeginBaseTrial()
{
    auto* pc=Cast<AOVController>(PlayerOwner);
    if (!pc || !pc->RangeCamera) return false;
    BasePresentation=GetWorld()->SpawnActor<AOVBasePresentation>(FVector(0,0,2000),FRotator::ZeroRotator);
    if (!BasePresentation) return false;
    BasePresentation->InitializeModules();
    pc->bFollow=false;
    pc->CameraFocus=BasePresentation->GetActorLocation();
    pc->CameraDistance=25500;
    pc->CameraPitch=-38;
    pc->CameraYaw=-145;
    pc->UpdateCamera();
    ToggleBaseView();
    return bBaseViewOpen;
}

FBox AOVHUD::GetBaseBounds() const
{
    return IsValid(BasePresentation) ? BasePresentation->GetComponentsBoundingBox(true) : FBox(ForceInit);
}

void AOVHUD::ToggleBaseView()
{
    if (!IsValid(BasePresentation)) return;
    bBaseViewOpen=!bBaseViewOpen;
    BaseDragWindow=INDEX_NONE;
    BaseNotice.Reset();
    bStarMapOpen=false;
    CancelPointerDrag();
}

int32 AOVHUD::BaseWindowAt(FVector2D point) const
{
    for (int i=BaseWindowOrder.Num()-1;i>=0;--i)
    {
        const int id=BaseWindowOrder[i];
        const auto& window=BaseWindows[id];
        const FVector2D local=point-window.Position;
        if (local.X>=0 && local.Y>=0 && local.X<window.Size.X && local.Y<(window.bMinimized?32:window.Size.Y)) return id;
    }
    return INDEX_NONE;
}

void AOVHUD::OpenBaseWindow(int32 id)
{
    auto& window=BaseWindows[id];
    if (window.Size.IsZero())
    {
        window.Size=id<2?FVector2D(500,455):id==2?FVector2D(250,408):FVector2D(235,190);
        window.Position=FVector2D(12+id*48,78+id*26);
    }
    window.bMinimized=false;
    BaseWindowOrder.Remove(id);
    BaseWindowOrder.Add(id);
}

bool AOVHUD::HandleBaseAction(FName action)
{
    if (action == TEXT("BaseView"))
    {
        if (!bTitleScreenOpen && !bPauseMenu && !bStarMapOpen && !IsDebriefOpen() && !PickingShip())
            ToggleBaseView();
        return true;
    }
    if (!bBaseViewOpen || !IsValid(BasePresentation)) return false;
    if (action == TEXT("PlaySceneExit") || action == TEXT("RestartScene")) return false;
    if (action == TEXT("Pause") || action == TEXT("Speed1") || action == TEXT("Speed2") || action == TEXT("Speed3"))
    {
        if (auto* pc=Cast<AOVController>(PlayerOwner)) pc->Action(action);
        return true;
    }
    const FString tagged=action.ToString();
    if (tagged.StartsWith(TEXT("BaseWin")) && tagged.Len()>9 && tagged[8]==TCHAR('_'))
    {
        const int32 id=tagged[7]-TCHAR('0');
        if (id<0 || id>=4 || !BaseWindowOrder.Contains(id)) return true;
        auto& window=BaseWindows[id];
        BaseWindowOrder.Remove(id);
        BaseWindowOrder.Add(id);
        action=FName(*tagged.Mid(9));
        if (action==TEXT("Pin")) { window.bPinned=!window.bPinned; return true; }
        if (action==TEXT("Min") || action==TEXT("Close"))
        {
            if (action==TEXT("Close")) BaseWindowOrder.Remove(id);
            else window.bMinimized=!window.bMinimized;
            if (id==1) { BasePick=0; BaseRouteCell.Reset(); }
            if (id==0) BaseRemoval=0;
            BaseDragWindow=INDEX_NONE;
            return true;
        }
        if (action==TEXT("Frame")) return true;
    }
    auto& build=BasePresentation->Build;
    if (action==TEXT("BaseBack")) ToggleBaseView();
    else if (action==TEXT("BaseOperations") || action==TEXT("BaseBuild") || action==TEXT("BaseResearch") || action==TEXT("BaseFiles"))
    {
        for (int id=0;id<4;++id) if (action==WindowActions[id]) OpenBaseWindow(id);
    }
    else if (action==TEXT("BaseCorridor") || action==TEXT("BaseJunction") || action==TEXT("BaseStorage") ||
        action==TEXT("BaseYard") || action==TEXT("BaseDock"))
    {
        OpenBaseWindow(1);
        BasePick=action==TEXT("BaseCorridor")?1:action==TEXT("BaseJunction")?2:action==TEXT("BaseStorage")?3:action==TEXT("BaseYard")?4:5;
        BasePage=BasePick/5; BaseRouteCell.Reset(); BaseRemoval=0;
    }
    else if (action==TEXT("BasePrevious") || action==TEXT("BaseNext"))
    {
        BasePage=FMath::Clamp(BasePage+(action==TEXT("BaseNext")?1:-1),0,3);
        BasePick=0; BaseRouteCell.Reset();
    }
    else if (action==TEXT("BaseCore"))
    {
        OpenBaseWindow(0); BaseInspect=0; BaseRemoval=0; BasePick=0; BaseRouteCell.Reset();
    }
    else if (action==TEXT("BaseInspect"))
    {
        BasePick=0; BaseRemoval=0; BaseRouteCell.Reset();
    }
    else if (action==TEXT("BaseAutoConnect"))
    {
        bBaseAutoConnect=!bBaseAutoConnect; BaseRouteCell.Reset();
    }
    else if (action==TEXT("BaseRouteConfirm") && BaseRouteCell.IsSet() && BasePick>0)
    {
        const auto anchor=BaseRouteCell.GetValue();
        const auto candidate=UnderPointer(build,BasePick,{anchor.X,anchor.Y,anchor.Z},BaseTurns);
        const auto route=BasePresentation->PlanConnection(candidate);
        FString blocker;
        const auto result=BasePresentation->CheckPlan(route,nullptr,&blocker);
        if (result==OVBase::Error::None && BasePresentation->ReservePlan(route))
        {
            BaseInspect=static_cast<int32>(build.Modules().size())-1;
            BaseNotice=TEXT("모듈과 연결 통로를 예약했습니다.");
        }
        else BaseNotice=PlacementStatus(result,blocker);
        BaseRouteCell.Reset();
    }
    else if (action==TEXT("BaseUpgrade") && BaseInspect>=0)
    {
        const auto result=build.CheckUpgrade(BaseInspect);
        BaseNotice=PlacementStatus(result,FString());
        if (BasePresentation->UpgradeModule(BaseInspect)) BaseNotice=TEXT("증설을 예약했습니다.");
    }
    else if (action==TEXT("BaseRemoveConfirm") && BaseInspect>=0 && BaseRemoval)
    {
        const bool cancel=BaseRemoval==1;
        const auto preview=build.InspectRemoval(BaseInspect,cancel);
        const bool removed=cancel?BasePresentation->CancelConstruction(BaseInspect):BasePresentation->DemolishModule(BaseInspect);
        BaseNotice=removed?TEXT("처리했습니다."):PlacementStatus(preview.Result,FString());
        if (removed) BaseInspect=INDEX_NONE;
        BaseRemoval=0;
    }
    else if (action==TEXT("BaseCancel") || action==TEXT("BaseDemolish"))
        BaseRemoval=action==TEXT("BaseCancel")?1:2;
    else if (action==TEXT("BaseDismiss") || action==TEXT("BaseRouteDismiss"))
    {
        BaseRemoval=0; BaseRouteCell.Reset();
    }
    else if (action==TEXT("BaseFunded"))
    {
        build.SetTestAccounting(!build.Charges());
        BaseNotice=build.Charges()?TEXT("비용 검사를 켰습니다."):TEXT("새 공사가 자원을 소모하지 않습니다.");
    }
    else if (action==TEXT("BaseFill"))
    {
        build.FillTestStock();
        BaseNotice=TEXT("현재 저장 한도까지 시험 자원을 채웠습니다.");
    }
    else if (action==TEXT("BaseSave")) BasePresentation->SaveTrial(BaseNotice);
    else if (action==TEXT("BaseLoad"))
    {
        if (BasePresentation->LoadTrial(BaseNotice))
        {
            BaseInspect=INDEX_NONE; BasePick=0; BaseRemoval=0; BaseRouteCell.Reset();
        }
    }
    else
    {
        for (int32 i=1;i<static_cast<int32>(OVBase::Kind::Count);++i)
            if (action==CatalogAction(i))
            {
                BasePick=i; BaseRouteCell.Reset(); BaseRemoval=0; BaseNotice.Reset(); return true;
            }
        for (int32 i=1;i<static_cast<int32>(OVBase::Technology::Count);++i)
            if (action==TechnologyAction(i))
            {
                BaseNotice=build.UnlockTestTechnology(static_cast<OVBase::Technology>(i))?
                    TEXT("시험용 선행 기술을 준비했습니다."):TEXT("코어 등급과 선행 기술부터 준비하세요.");
                return true;
            }
    }
    return true;
}

void AOVHUD::AdvanceBaseConstruction(float gameSeconds)
{
    if (IsValid(BasePresentation)) BasePresentation->AdvanceConstruction(gameSeconds);
}

bool AOVHUD::BaseWindowPointer(FVector2D pixel,bool press)
{
    const FVector2D mouse=pixel/UIScale;
    if (!press)
    {
        if (BaseDragWindow==INDEX_NONE) return false;
        auto& window=BaseWindows[BaseDragWindow];
        if (pixel.X>=0 && pixel.Y>=0)
            (bResizeBaseWindow?window.Size:window.Position)=mouse-BaseDragOffset;
        BaseDragWindow=INDEX_NONE;
        return true;
    }
    const int id=BaseWindowAt(mouse);
    if (id==INDEX_NONE) return false;
    auto& window=BaseWindows[id];
    const FVector2D local=mouse-window.Position;
    const bool resize=!window.bMinimized && local.X>=window.Size.X-20 && local.Y>=window.Size.Y-20;
    if (window.bPinned || (!resize && (local.Y>=32 || local.X>=window.Size.X-102))) return false;
    BaseDragWindow=id;
    bResizeBaseWindow=resize;
    BaseDragOffset=mouse-(resize?window.Size:window.Position);
    BaseWindowOrder.Remove(id);
    BaseWindowOrder.Add(id);
    return true;
}

void AOVHUD::UpdateBaseView()
{
    auto* pc = Cast<AOVController>(PlayerOwner);
    if (!pc || !IsValid(BasePresentation)) return;
    auto pressed = [&](FKey key) { return pc->WasInputKeyJustPressed(key); };
    if (pressed(EKeys::Escape)) { ToggleBaseView(); return; }
    float mx,my;
    if (pc->GetMousePosition(mx,my))
    {
        const FVector2D mouse=FVector2D(mx,my)/UIScale;
        if (BaseDragWindow!=INDEX_NONE)
        {
            auto& window=BaseWindows[BaseDragWindow];
            (bResizeBaseWindow?window.Size:window.Position)=mouse-BaseDragOffset;
            return;
        }
    }
    if ((pressed(EKeys::SpaceBar) || pressed(EKeys::F1))) pc->Action(TEXT("Pause"));
    if ((pressed(EKeys::One) || pressed(EKeys::F2))) pc->Action(TEXT("Speed1"));
    if ((pressed(EKeys::Two) || pressed(EKeys::F3))) pc->Action(TEXT("Speed2"));
    if ((pressed(EKeys::Three) || pressed(EKeys::F4))) pc->Action(TEXT("Speed3"));
    if (pressed(EKeys::Home)) pc->FrameEngagement();
    if (pressed(EKeys::Q) || pressed(EKeys::E) || pressed(EKeys::R) || pressed(EKeys::F))
        BaseRouteCell.Reset();
    BaseTurns = (BaseTurns+4+int(pressed(EKeys::E))-int(pressed(EKeys::Q)))%4;
    BaseLayer = FMath::Clamp(BaseLayer+int(pressed(EKeys::R))-int(pressed(EKeys::F)),-8,8);
    float x,y;
    if (pc->IsInputKeyDown(EKeys::MiddleMouseButton) || !pressed(EKeys::LeftMouseButton) || !pc->GetMousePosition(x,y)) return;
    if (IsOverPanel(x,y) || BaseRemoval || BaseRouteCell.IsSet()) return;
    OVBase::Cell cell;
    if (!PointerCell(*pc,*BasePresentation,BaseLayer,cell)) return;
    auto& build = BasePresentation->Build;
    if (BasePick == 0)
    {
        BaseInspect = INDEX_NONE;
        for (std::size_t i=0; i<build.Modules().size(); ++i)
            if (OVBase::Contains(build.Modules()[i],cell)) { BaseInspect = static_cast<int32>(i); OpenBaseWindow(0); break; }
        return;
    }
    const auto candidate=UnderPointer(build,BasePick,cell,BaseTurns);
    FString blocker;
    const auto placement=BasePresentation->CheckPlacement(candidate,&blocker);
    BaseNotice=placement==OVBase::Error::Ship ? FString() : PlacementStatus(placement,blocker);
    if (placement==OVBase::Error::NoConnection || placement==OVBase::Error::Direction)
    {
        if (bBaseAutoConnect && !BasePresentation->PlanConnection(candidate).empty())
            BaseRouteCell=FIntVector(cell.X,cell.Y,cell.Z);
        return;
    }
    if (placement==OVBase::Error::None && BasePresentation->ReservePlan({candidate}))
    {
        BaseNotice = TEXT("건설을 예약했습니다.");
        BaseInspect = static_cast<int32>(build.Modules().size())-1;
    }
}

void AOVHUD::DrawBaseView()
{
    auto* pc = Cast<AOVController>(PlayerOwner);
    if (!pc || !IsValid(BasePresentation)) return;
    const float width = ViewportPixels.X/UIScale;
    const float height = ViewportPixels.Y/UIScale;
    const auto project = [&](FVector world,FVector2D& out) {
        if (!pc->ProjectWorldLocationToScreen(world,out,true)) return false;
        out /= UIScale;
        return true;
    };
    const auto plane = [&](double x,double y) {
        return BasePresentation->CellCenter({0,0,BaseLayer})+FVector(x,y,0)*OVBase::CellCm;
    };
    const auto line = [&](FVector a,FVector b,FLinearColor color,float thickness) {
        FVector2D p,q;
        if (project(a,p) && project(b,q)) DrawLine(p.X,p.Y,q.X,q.Y,color,thickness);
    };
    const bool showPorts=BaseWindowOrder.Contains(1) && !BaseWindows[1].bMinimized;
    if (auto* batch=showPorts?GetWorld()->GetLineBatcher(UWorld::ELineBatcherType::World):nullptr)
    {
        TArray<FBatchedLine> grid;
        grid.Reserve(50);
        const FLinearColor color(.07f,.19f,.23f);
        for (int i=-12;i<=12;++i)
        {
            const double edge=i+.5;
            grid.Emplace(plane(edge,-11.5),plane(edge,12.5),color,0.f,0.f,SDPG_World);
            grid.Emplace(plane(-11.5,edge),plane(12.5,edge),color,0.f,0.f,SDPG_World);
        }
        batch->DrawLines(grid);
    }

    // Port arrows mark outward normals on the exposed end caps.
    if (showPorts) for(const auto& module : BasePresentation->Build.Modules())
        for(const auto& port : OVBase::Ports(module))
        {
            const FVector d(port.Direction.X,port.Direction.Y,port.Direction.Z);
            const FVector face=BasePresentation->CellCenter(port.At)+d*(OVBase::CellCm*.5);
            FVector2D p;
            if(project(face,p)) DrawRect(AccentColor,p.X-3,p.Y-3,6,6);
            line(face,face+d*(OVBase::CellCm*.25),AccentColor,2);
        }
    float x,y;
    OVBase::Cell cell;
    auto& build = BasePresentation->Build;
    const bool pointing=pc->GetMousePosition(x,y) && !IsOverPanel(x,y) &&
        PointerCell(*pc,*BasePresentation,BaseLayer,cell);
    if (BaseRouteCell.IsSet()) cell={BaseRouteCell->X,BaseRouteCell->Y,BaseRouteCell->Z};
    if (BasePick != 0 && (pointing || BaseRouteCell.IsSet()))
    {
        const auto candidate=UnderPointer(build,BasePick,cell,BaseTurns);
        const auto dim = OVBase::Dimensions(candidate.Type);
        const auto center = BasePresentation->CellCenter(candidate.Origin);
        const auto rotation = FRotator(0,90*BaseTurns,0);
        const auto point = [&](double bx,double by) { return center+rotation.RotateVector(FVector(bx,by,0))*OVBase::CellCm; };
        const FVector corners[]{point(-.5,-.5),point(dim.X-.5,-.5),point(dim.X-.5,dim.Y-.5),point(-.5,dim.Y-.5)};
        FString blocker;
        auto error = BasePresentation->CheckPlacement(candidate,&blocker);
        std::vector<OVBase::Module> path;
        if (bBaseAutoConnect && (error==OVBase::Error::NoConnection || error==OVBase::Error::Direction || BaseRouteCell.IsSet()))
        {
            path=BasePresentation->PlanConnection(candidate);
            if (!path.empty())
            {
                error=BasePresentation->CheckPlan(path,nullptr,&blocker);
                for (std::size_t i=0; i+1<path.size(); ++i)
                {
                    const auto routeCenter=BasePresentation->CellCenter(path[i].Origin);
                    const auto next=BasePresentation->CellCenter(path[i+1].Origin);
                    line(routeCenter,next,FLinearColor(1.f,.64f,.22f),4);
                    FVector2D marker;
                    if (project(routeCenter,marker)) DrawRect(FLinearColor(1.f,.64f,.22f),marker.X-4,marker.Y-4,8,8);
                }
            }
        }
        const FLinearColor color = error == OVBase::Error::None ? FLinearColor::Green : FLinearColor::Red;
        const FVector bottom(0,0,-.5*OVBase::CellCm), top(0,0,(dim.Z-.5)*OVBase::CellCm);
        for (int i=0; i<4; ++i)
        {
            line(corners[i]+bottom,corners[(i+1)%4]+bottom,color,2);
            line(corners[i]+top,corners[(i+1)%4]+top,color,2);
            line(corners[i]+bottom,corners[i]+top,color,2);
        }
        for(const auto& port:OVBase::Ports(candidate))
        {
            const FVector d(port.Direction.X,port.Direction.Y,port.Direction.Z);
            const FVector face=BasePresentation->CellCenter(port.At)+d*(OVBase::CellCm*.5);
            line(face,face+d*(OVBase::CellCm*.45),color,4);
        }
        if (OVBase::HasBay(candidate.Type))
        {
            const auto mouth=candidate.Origin+OVBase::Rotate(OVBase::BayMouth(candidate.Type),BaseTurns);
            const auto forward=OVBase::Rotate(OVBase::BayForward(candidate.Type),BaseTurns);
            const auto localMouth=OVBase::BayMouth(candidate.Type);
            const auto bayPath=OVBase::Approach(candidate.Type);
            const bool yard=OVBase::Def(candidate.Type).YardBay;
            const int lateralOffset=bayPath.AcrossCm-(yard ? localMouth.Y : localMouth.X)*OVBase::CellCm;
            const auto offset=OVBase::Rotate({yard ? 0 : lateralOffset,yard ? lateralOffset : 0,
                bayPath.HeightCm-localMouth.Z*OVBase::CellCm},BaseTurns);
            const FVector start=BasePresentation->CellCenter(mouth)+FVector(offset.X,offset.Y,offset.Z);
            const FVector d(forward.X,forward.Y,0), side(-forward.Y,forward.X,0);
            for (int sign: {-1,1})
                line(start+(d*.5+side*.5*sign)*OVBase::CellCm,start+(d*(OVBase::Def(candidate.Type).FlightCells+.5)+side*.5*sign)*OVBase::CellCm,AccentColor,2);
            const FVector tip=start+d*(OVBase::Def(candidate.Type).FlightCells+1)*OVBase::CellCm;
            line(start,tip,AccentColor,3);
            for (int sign: {-1,1})
                line(tip,tip+(-d*.42+side*.28*sign)*OVBase::CellCm,AccentColor,3);
        }
        FString preview=PlacementStatus(error,blocker);
        if (!path.empty() && error==OVBase::Error::None)
        {
            const int count=static_cast<int>(path.size())-1;
            const auto price=OVBase::Def(candidate.Type).Price+OVBase::Resources{40*count,0,0};
            preview=FString::Printf(TEXT("연결 %d칸 · 총 %s%s"),count,*ResourceText(price),build.Charges()?TEXT(""):TEXT(" · 무료 시험"));
        }
        DrawText(preview,color,14,height-73,OVTypography::Font(EOVTextRole::Body));
    }

    const auto stock=build.Available(), cap=build.Capacity();
    DrawRect(PanelColor,0,0,width,63);
    Button(TEXT("BaseBack"),TEXT("함대 화면"),8,5,85);
    Button(TEXT("Pause"),pc->bSimulationPaused?TEXT("재개 ▷"):TEXT("정지 Ⅱ"),98,5,60);
    for (int speed=1;speed<=3;++speed)
        Button(FName(*FString::Printf(TEXT("Speed%d"),speed)),FString::Printf(TEXT("%d×"),speed),
            162+(speed-1)*38,5,35,pc->SimulationSpeed==speed);
    Button(TEXT("RestartScene"),TEXT("재시작"),width-162,5,70);
    Button(TEXT("PlaySceneExit"),TEXT("목록 복귀"),width-88,5,80);
    DrawText(FString::Printf(TEXT("일반  %d / %d      고급  %d / %d      최고급  %d / %d"),
        stock.Common,cap.Common,stock.Advanced,cap.Advanced,stock.Rare,cap.Rare),
        AccentColor,12,38,OVTypography::Font(EOVTextRole::Numeric));
    const float slot=FMath::Min(width/4.f,104.f);
    for (int id=0;id<4;++id)
        Button(WindowActions[id],WindowLabels[id],width*.5f+(id-2)*slot,height-39,slot-4,BaseWindowOrder.Contains(id));
    for (int z=0;z<BaseWindowOrder.Num();++z) DrawBaseWindow(BaseWindowOrder[z],30+z*3);
    if (!BaseNotice.IsEmpty()) DrawText(BaseNotice,FLinearColor::White,12,height-95,OVTypography::Font(EOVTextRole::Body));
}

void AOVHUD::DrawBaseWindow(int32 id,int32 priority)
{
    auto& win=BaseWindows[id];
    const float screenW=ViewportPixels.X/UIScale, screenH=ViewportPixels.Y/UIScale;
    const float designW=id<2?495.f:225.f;
    const float designH=id<2?429.f:id==2?365.f:140.f;
    win.Size.X=FMath::Clamp(win.Size.X,FMath::Min(designW*.72,double(screenW)),double(screenW));
    win.Size.Y=FMath::Clamp(win.Size.Y,FMath::Min(designH*.72+38,double(screenH-110)),double(screenH-110));
    win.Position.X=FMath::Clamp(win.Position.X,0.,FMath::Max(0.,screenW-win.Size.X));
    win.Position.Y=FMath::Clamp(win.Position.Y,64.,FMath::Max(64.,screenH-45-(win.bMinimized?32.:win.Size.Y)));
    const float X=win.Position.X,Y=win.Position.Y,W=win.Size.X,H=win.bMinimized?32:win.Size.Y;
    const auto tag=[id](FName n){return FName(*FString::Printf(TEXT("BaseWin%d_%s"),id,*n.ToString()));};
    DrawRect(PanelColor,X,Y,W,H);
    AddHitBox(win.Position*UIScale,FVector2D(W,H)*UIScale,tag(TEXT("Frame")),true,priority);
    DrawRect(FLinearColor(.04f,.14f,.19f),X,Y,W,32);
    const auto body=OVTypography::Font(EOVTextRole::Body);
    const auto heading=OVTypography::Font(EOVTextRole::Heading);
    DrawText(WindowLabels[id],AccentColor,X+10,Y+6,heading);
    const auto control=[&](FName n,const FString& label,float left,float top,float w,float h,bool active,float factor)
    {
        DrawRect(active?FLinearColor(.04f,.38f,.46f):FLinearColor(.06f,.14f,.20f),left,top,w,h);
        DrawText(FitLabel(body,label,w/factor-8),FLinearColor::White,left+4*factor,top+4*factor,body,factor);
        ActionCenters.Add(n,FVector2D(left+w*.5f,top+h*.5f));
        AddHitBox(FVector2D(left,top)*UIScale,FVector2D(w,h)*UIScale,tag(n),true,priority+1);
    };
    control(TEXT("Pin"),TEXT("핀"),X+W-99,Y+2,31,27,win.bPinned,1);
    control(TEXT("Min"),win.bMinimized?TEXT("+"):TEXT("−"),X+W-65,Y+2,30,27,false,1);
    control(TEXT("Close"),TEXT("×"),X+W-32,Y+2,30,27,false,1);
    if (win.bMinimized) return;
    if (!win.bPinned)
        for (float length : {6.f,11.f,16.f}) DrawLine(X+W-length,Y+H-2,X+W-2,Y+H-length,AccentColor,1);
    const float factor=FMath::Min((W-5)/designW,(H-38)/designH);
    const auto px=[&](float v){return X+v*factor;};
    const auto py=[&](float v){return Y+32+(v-60)*factor;};
    const auto label=[&](const FString& t,FLinearColor c,float x,float y,UFont* f)
        {DrawText(t,c,px(x),py(y),f,factor);};
    const auto button=[&](FName n,const FString& t,float x,float y,float w,bool active=false)
        {control(n,t,px(x),py(y),w*factor,27*factor,active,factor);};
    const auto rect=[&](FLinearColor c,float x,float y,float w,float h)
        {DrawRect(c,px(x),py(y),w*factor,h*factor);};
    const float right=228;
    const float height=designH+60;
    const TCHAR* roman[]={TEXT("I"),TEXT("II"),TEXT("III"),TEXT("IV")};
    auto& build=BasePresentation->Build;
    const auto capacity=build.Capacity();
    if (id==1)
    {
        label(FString::Printf(TEXT("모듈 카탈로그  %d/4"),BasePage+1),AccentColor,12,66,heading);
        button(TEXT("BasePrevious"),TEXT("이전"),12,91,93);
        button(TEXT("BaseNext"),TEXT("다음"),110,91,93);
        const float row=FMath::Clamp((height-295.f)/5.f,29.f,43.f);
        for (int32 i=BasePage*5; i<FMath::Min((BasePage+1)*5,static_cast<int32>(OVBase::Kind::Count)); ++i)
        {
            const auto kind=static_cast<OVBase::Kind>(i);
            const auto& def=OVBase::Def(kind);
            const float rowY=127+(i-BasePage*5)*row;
            const bool open=build.CoreTier()>=def.CoreTier && build.HasTechnology(def.Required);
            const FString title=open?FString(def.Name):FString(TEXT("잠김 · "))+def.Name;
            button(i==0?FName(TEXT("BaseCore")):CatalogAction(i),title,12,rowY,191,BasePick==i);
        }
        const float tools=132+5*row;
        button(TEXT("BaseInspect"),TEXT("시설 선택"),12,tools,190,BasePick==0);
        button(TEXT("BaseAutoConnect"),bBaseAutoConnect?TEXT("자동 연결 켜짐"):TEXT("자동 연결 꺼짐"),12,tools+34,190,bBaseAutoConnect);
        label(FString::Printf(TEXT("고도 %d m · 방향 %d°"),BaseLayer*25,BaseTurns*90),FLinearColor::White,12,tools+72,body);
        if (height>tools+160)
            label(TEXT("Q/E 회전 · R/F 고도\nWASD 이동 · 휠 드래그 회전\nSpace 정지 · Home 전체 보기"),FLinearColor::White,12,tools+94,body);
    }
    else if (id==2)
    {
        label(TEXT("선행 기술 · 시험 준비"),AccentColor,12,66,heading);
        label(TEXT("시험장에만 적용"),FLinearColor::White,12,94,body);
        const float row=FMath::Clamp((height-160.f)/8.f,29.f,38.f);
        for (int32 i=1;i<static_cast<int32>(OVBase::Technology::Count);++i)
        {
            const auto tech=static_cast<OVBase::Technology>(i);
            const auto& rule=OVBase::Technologies[i];
            const FString techLabel=build.HasTechnology(tech)?FString(rule.Name)+TEXT(" ✓"):
                FString::Printf(TEXT("%s · 코어 %s"),rule.Name,roman[rule.CoreTier-1]);
            button(TechnologyAction(i),techLabel,12,118+(i-1)*row,191,build.HasTechnology(tech));
        }
    }
    else if (id==3)
    {
        label(TEXT("시험장 저장"),AccentColor,12,66,heading);
        button(TEXT("BaseSave"),TEXT("현재 기지 저장"),12,99,190);
        button(TEXT("BaseLoad"),TEXT("저장한 기지 불러오기"),12,137,190);
        label(TEXT("배치 · 자원 · 진행 중 공사"),FLinearColor::White,12,179,body);
    }
    else
    {
        label(TEXT("기지 운영"),AccentColor,12,66,heading);
        button(TEXT("BaseInspect"),TEXT("시설 선택"),12,99,190,BasePick==0);
        button(TEXT("BaseCore"),TEXT("중앙 코어 보기"),12,137,190,BaseInspect==0);
        button(TEXT("BaseFunded"),build.Charges()?TEXT("비용 시험 끄기"):TEXT("비용 시험 켜기"),12,175,190,build.Charges());
        button(TEXT("BaseFill"),TEXT("시험 자원 채우기"),12,213,190);
        int links=0,facilities=0,queue=0;
        for (const auto& item:build.Modules())
        {
            if (OVBase::IsLink(item.Type)) ++links;
            else if (item.Type!=OVBase::Kind::Core) ++facilities;
            if (item.Remaining>0) ++queue;
        }
        const auto limits=build.Limits();
        label(FString::Printf(TEXT("코어 %s · 공사 %d건"),roman[build.CoreTier()-1],queue),AccentColor,12,256,heading);
        label(FString::Printf(TEXT("시설 %d/%d · 연결 %d/%d"),facilities,limits.Facilities,links,limits.Links),FLinearColor::White,12,284,body);
        label(FString::Printf(TEXT("건설 슬롯 %d/%d"),build.ActiveFacilities(),limits.Concurrent),FLinearColor::White,12,308,body);
    }

    if (BasePick>0 && id==1)
    {
        const auto kind=static_cast<OVBase::Kind>(BasePick);
        const auto& def=OVBase::Def(kind);
        label(def.Name,FLinearColor::White,right,199,heading);
        label(FString::Printf(TEXT("%d×%d×%d칸 · %d초"),def.Size.X,def.Size.Y,def.Size.Z,def.Seconds),FLinearColor::White,right,225,body);
        label(FString::Printf(TEXT("비용  %s"),*ResourceText(def.Price)),FLinearColor::White,right,248,body);
        label(FString::Printf(TEXT("코어 %s · %s"),roman[def.CoreTier-1],build.CoreTier()>=def.CoreTier?TEXT("충족"):TEXT("필요")),
            build.CoreTier()>=def.CoreTier?AccentColor:FLinearColor::Yellow,right,271,body);
        label(FString::Printf(TEXT("연구  %s%s"),TechnologyName(def.Required),build.HasTechnology(def.Required)?TEXT(""):TEXT(" · 필요")),
            FLinearColor::White,right,294,body);
        const bool yard=kind==OVBase::Kind::Yard, dock=kind==OVBase::Kind::Dock;
        if (yard || dock)
        {
            const int32 card=yard?0:1;
            if (!BaseBayCards[card])
                BaseBayCards[card]=LoadObject<UTexture2D>(nullptr,yard?TEXT("/Game/Presentation/Base/T_YardSmall_Card.T_YardSmall_Card"):
                    TEXT("/Game/Presentation/Base/T_DockStandard_Card.T_DockStandard_Card"));
            if (BaseBayCards[card] && height>=480) DrawTexture(BaseBayCards[card],px(right),py(323),232*factor,116*factor,0,0,1,1);
        }
        else
        {
            const TCHAR* model=BasePresentation->HasOriginalModel(kind)?
                (BasePick<=5?TEXT("원본 모델"):TEXT("원본 외형 · 연결 어댑터")):TEXT("시험 형상 · 모델 준비 중");
            label(model,AccentColor,right,323,body);
            if (height>452)
            {
                const float w=FMath::Min(150.f,24.f*def.Size.X);
                const float h=FMath::Min(58.f,12.f*def.Size.Z);
                rect(FLinearColor(.05f,.15f,.20f),right,348,232,74);
                rect(FLinearColor(.20f,.47f,.58f),right+18,413-h,w,h);
                rect(AccentColor,right+12,410-h*.5f,10,5);
            }
        }
    }
    else if (id==0 && BaseInspect>=0 && static_cast<std::size_t>(BaseInspect)<build.Modules().size())
    {
        const auto& item=build.Modules()[BaseInspect];
        label(FString::Printf(TEXT("%s %s"),ModuleName(item.Type),
            item.Type==OVBase::Kind::Core || item.Type==OVBase::Kind::Storage?roman[item.Level-1]:TEXT("")),
            FLinearColor::White,right,199,heading);
        const auto network=build.Network(false);
        bool accessible=network[BaseInspect];
        for (std::size_t i=0;i<network.size() && !accessible;++i)
            accessible=network[i] && OVBase::Connected(item,build.Modules()[i]);
        const AOVShip* blocker=BasePresentation->ConstructionBlocker(BaseInspect);
        const FString phase=blocker?FString::Printf(TEXT("%s 이동 대기"),*blocker->ShipName):
            FString(accessible?TEXT("공사 중"):TEXT("연결 대기"));
        label(item.Remaining>0?FString::Printf(TEXT("%s · %.1f초"),*phase,item.Remaining):
            FString(network[BaseInspect]?TEXT("코어 연결 · 가동 중"):TEXT("코어 단절 · 작업 정지")),
            FLinearColor::White,right,227,body);
        if (BaseRemoval)
        {
            const auto info=build.InspectRemoval(BaseInspect,BaseRemoval==1);
            label(FString::Printf(TEXT("%s  %s"),BaseRemoval==1?TEXT("취소 환불"):TEXT("철거 환불"),
                *ResourceText(info.Refund)),AccentColor,right,255,body);
            if (!(info.Overflow==OVBase::Resources{}))
                label(FString::Printf(TEXT("공간 초과  %s"),*ResourceText(info.Overflow)),FLinearColor::Yellow,right,277,body);
            label(FString::Printf(TEXT("코어 연결 상실 %d동"),static_cast<int32>(info.Disconnected.size())),
                FLinearColor::White,right,300,body);
            for (std::size_t n=0;n<info.Disconnected.size() && n<3;++n)
                for (const auto& affected:build.Modules())
                    if (affected.Id==info.Disconnected[n])
                        label(FString::Printf(TEXT("정지 · %s #%d"),ModuleName(affected.Type),affected.Id),
                            FLinearColor::Yellow,right,322+21*n,body);
            if (info.Result==OVBase::Error::None) button(TEXT("BaseRemoveConfirm"),TEXT("확정"),right,389,112);
            else label(PlacementStatus(info.Result,FString()),FLinearColor::Yellow,right,389,body);
            button(TEXT("BaseDismiss"),TEXT("닫기"),right+119,421,112);
        }
        else if (item.Remaining>0) button(TEXT("BaseCancel"),TEXT("공사 취소"),right,259,232);
        else
        {
            const auto upgrade=build.Upgrade(BaseInspect);
            if (upgrade.Level)
            {
                label(FString::Printf(TEXT("%s · %d초 · %s"),roman[upgrade.Level-1],upgrade.Seconds,*ResourceText(upgrade.Price)),
                    AccentColor,right,258,body);
                label(FString::Printf(TEXT("연구  %s"),TechnologyName(upgrade.Required)),FLinearColor::White,right,280,body);
                const auto check=build.CheckUpgrade(BaseInspect);
                const auto missing=(upgrade.Price-capacity).Positive();
                if (check==OVBase::Error::Capacity)
                    label(FString::Printf(TEXT("저장 공간 부족  %s"),*ResourceText(missing)),FLinearColor::Yellow,right,302,body);
                else if (check!=OVBase::Error::None)
                    label(PlacementStatus(check,FString()),FLinearColor::Yellow,right,302,body);
                if (check==OVBase::Error::None) button(TEXT("BaseUpgrade"),TEXT("이 시설 증설"),right,329,232);
            }
            if (item.Type!=OVBase::Kind::Core) button(TEXT("BaseDemolish"),TEXT("철거 보기"),right,365,232);
        }
    }
    if (id==1 && BaseRouteCell.IsSet())
    {
        button(TEXT("BaseRouteConfirm"),TEXT("연결과 건설 확정"),right,height-73,232);
        button(TEXT("BaseRouteDismiss"),TEXT("경로 취소"),right,height-42,232);
    }
}
