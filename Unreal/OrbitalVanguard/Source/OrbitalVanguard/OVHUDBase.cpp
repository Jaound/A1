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
const TCHAR* ModuleName(OVBase::Kind type)
{
    switch (type)
    {
    case OVBase::Kind::Core: return TEXT("중앙 코어");
    case OVBase::Kind::Corridor: return TEXT("연결 통로");
    case OVBase::Kind::Junction: return TEXT("분기 허브");
    case OVBase::Kind::Yard: return TEXT("소형 조선소");
    case OVBase::Kind::Dock: return TEXT("표준 접안 모듈");
    default: return TEXT("자원 창고");
    }
}
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
    case OVBase::Error::Clearance: return TEXT("함선 출입로가 막힙니다 · 전방 50m와 바깥 비행선을 비워 주세요.");
    case OVBase::Error::Funds: return TEXT("일반 자원이 부족합니다.");
    case OVBase::Error::Limit: return TEXT("코어 I 설치 한도에 도달했습니다.");
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
    BaseNotice.Reset();
    bStarMapOpen=false;
    CancelPointerDrag();
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
    if (action == TEXT("BaseBack")) ToggleBaseView();
    else if (action == TEXT("BaseCorridor")) BasePick = 1;
    else if (action == TEXT("BaseJunction")) BasePick = 2;
    else if (action == TEXT("BaseStorage")) BasePick = 3;
    else if (action == TEXT("BaseYard")) BasePick = 4;
    else if (action == TEXT("BaseDock")) BasePick = 5;
    else if (action == TEXT("BaseInspect")) BasePick = 0;
    else if (action == TEXT("BaseCancel"))
    {
        if (BaseInspect >= 0 && BasePresentation->CancelConstruction(BaseInspect))
        {
            BaseNotice = TEXT("건설 예약을 취소했습니다.");
            BaseInspect = INDEX_NONE;
        }
        else BaseNotice = TEXT("다른 공사가 이 연결을 사용 중입니다 · 끝쪽 공사부터 취소하세요.");
    }
    return true;
}

void AOVHUD::AdvanceBaseConstruction(float gameSeconds)
{
    if (IsValid(BasePresentation)) BasePresentation->AdvanceConstruction(gameSeconds);
}

void AOVHUD::UpdateBaseView()
{
    auto* pc = Cast<AOVController>(PlayerOwner);
    if (!pc || !IsValid(BasePresentation)) return;
    auto pressed = [&](FKey key) { return pc->WasInputKeyJustPressed(key); };
    if (pressed(EKeys::Escape)) { ToggleBaseView(); return; }
    if ((pressed(EKeys::SpaceBar) || pressed(EKeys::F1))) pc->Action(TEXT("Pause"));
    if ((pressed(EKeys::One) || pressed(EKeys::F2))) pc->Action(TEXT("Speed1"));
    if ((pressed(EKeys::Two) || pressed(EKeys::F3))) pc->Action(TEXT("Speed2"));
    if ((pressed(EKeys::Three) || pressed(EKeys::F4))) pc->Action(TEXT("Speed3"));
    if (pressed(EKeys::Home)) pc->FrameEngagement();
    BaseTurns = (BaseTurns+4+int(pressed(EKeys::E))-int(pressed(EKeys::Q)))%4;
    BaseLayer = FMath::Clamp(BaseLayer+int(pressed(EKeys::R))-int(pressed(EKeys::F)),-8,8);
    float x,y;
    if (pc->IsInputKeyDown(EKeys::MiddleMouseButton) || !pressed(EKeys::LeftMouseButton) || !pc->GetMousePosition(x,y)) return;
    if (x/UIScale < 210 || x/UIScale > ViewportPixels.X/UIScale-250 || y/UIScale < 52) return;
    OVBase::Cell cell;
    if (!PointerCell(*pc,*BasePresentation,BaseLayer,cell)) return;
    auto& build = BasePresentation->Build;
    if (BasePick == 0)
    {
        BaseInspect = INDEX_NONE;
        for (std::size_t i=0; i<build.Modules().size(); ++i)
            if (OVBase::Contains(build.Modules()[i],cell)) { BaseInspect = static_cast<int32>(i); break; }
        return;
    }
    const auto candidate=UnderPointer(build,BasePick,cell,BaseTurns);
    FString blocker;
    const auto placement=BasePresentation->CheckPlacement(candidate,&blocker);
    BaseNotice=placement==OVBase::Error::Ship ? FString() : PlacementStatus(placement,blocker);
    const FName ids[]={NAME_None,TEXT("Corridor"),TEXT("Junction"),TEXT("Storage"),TEXT("Yard"),TEXT("Dock")};
    if (BasePresentation->ReserveModule(ids[BasePick],FIntVector(candidate.Origin.X,candidate.Origin.Y,candidate.Origin.Z),BaseTurns))
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
    if (auto* batch=GetWorld()->GetLineBatcher(UWorld::ELineBatcherType::World))
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
    for(const auto& module : BasePresentation->Build.Modules())
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
    if (BasePick != 0 && pc->GetMousePosition(x,y) && x/UIScale >= 210 &&
        x/UIScale <= width-250 && y/UIScale >= 52 && PointerCell(*pc,*BasePresentation,BaseLayer,cell))
    {
        const auto candidate=UnderPointer(build,BasePick,cell,BaseTurns);
        const auto dim = OVBase::Dimensions(candidate.Type);
        const auto center = BasePresentation->CellCenter(candidate.Origin);
        const auto rotation = FRotator(0,90*BaseTurns,0);
        const auto point = [&](double bx,double by) { return center+rotation.RotateVector(FVector(bx,by,0))*OVBase::CellCm; };
        const FVector corners[]{point(-.5,-.5),point(dim.X-.5,-.5),point(dim.X-.5,dim.Y-.5),point(-.5,dim.Y-.5)};
        FString blocker;
        const auto error = BasePresentation->CheckPlacement(candidate,&blocker);
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
            const FVector start=BasePresentation->CellCenter(mouth);
            const FVector d(forward.X,forward.Y,0), side(-forward.Y,forward.X,0);
            for (int sign: {-1,1})
                line(start+(d*.5+side*.5*sign)*OVBase::CellCm,start+(d*2.5+side*.5*sign)*OVBase::CellCm,AccentColor,2);
            const FVector tip=start+d*3*OVBase::CellCm;
            line(start,tip,AccentColor,3);
            for (int sign: {-1,1})
                line(tip,tip+(-d*.42+side*.28*sign)*OVBase::CellCm,AccentColor,3);
        }
        DrawText(PlacementStatus(error,blocker),color,220,height-35,OVTypography::Font(EOVTextRole::Body));
    }

    DrawRect(PanelColor,0,0,width,52);
    DrawRect(PanelColor,0,52,210,height-52);
    DrawRect(PanelColor,width-250,52,250,height-52);
    Button(TEXT("BaseBack"),TEXT("함대 화면"),12,11,105);
    int links = 0, facilities = 0, queue = 0;
    for (const auto& item : build.Modules())
    {
        if (OVBase::IsLink(item.Type)) ++links;
        if (!OVBase::IsLink(item.Type) && item.Type!=OVBase::Kind::Core) ++facilities;
        if (item.Remaining>0) ++queue;
    }
    DrawText(FString::Printf(TEXT("기지 시험장 · 코어 I   자원 소모 없음   기능 %d/8 · 연결 %d/24"),
        facilities,links), AccentColor,130,16,OVTypography::Font(EOVTextRole::Heading));
    Button(TEXT("BaseInspect"),TEXT("시설 선택"),12,77,184,BasePick==0);
    Button(TEXT("BaseCorridor"),TEXT("일자 통로 · 15초"),12,115,184,BasePick==1);
    Button(TEXT("BaseJunction"),TEXT("분기 허브 · 15초"),12,153,184,BasePick==2);
    Button(TEXT("BaseStorage"),TEXT("창고 I · 60초"),12,191,184,BasePick==3);
    Button(TEXT("BaseYard"),TEXT("소형 조선소 · 120초"),12,229,184,BasePick==4);
    Button(TEXT("BaseDock"),TEXT("표준 접안 · 75초"),12,267,184,BasePick==5);
    DrawText(FString::Printf(TEXT("고도 %d m · 회전 %d°"),BaseLayer*25,BaseTurns*90),
        FLinearColor::White,12,306,OVTypography::Font(EOVTextRole::Body));
    DrawText(TEXT("Q/E 회전 · R/F 고도\nWASD 이동 · 휠 드래그 회전\n휠 확대 · Home 전체 보기\nSpace 정지 · 1/2/3 배속 · Esc 함대"),
        FLinearColor::White,12,334,OVTypography::Font(EOVTextRole::Body));

    DrawText(TEXT("청록색 연결구 바로 바깥 칸에\n통로를 놓고 끝에 창고를 이으세요."),AccentColor,12,410,OVTypography::Font(EOVTextRole::Body));
    if (BasePick==4 || BasePick==5)
    {
        const bool yard=BasePick==4;
        if (!BaseBayCards[BasePick-4])
            BaseBayCards[BasePick-4]=LoadObject<UTexture2D>(nullptr,yard?
                TEXT("/Game/Presentation/Base/T_YardSmall_Card.T_YardSmall_Card"):
                TEXT("/Game/Presentation/Base/T_DockStandard_Card.T_DockStandard_Card"));
        if (BaseBayCards[BasePick-4] && height>588)
            DrawTexture(BaseBayCards[BasePick-4],14,448,180,89,0,0,1,1);
        DrawText(yard?TEXT("4×3×3 · 120초"):TEXT("4×3×2 · 75초"),AccentColor,12,height-84,OVTypography::Font(EOVTextRole::Body));
    }
    const float right = width-236;
    constexpr float top = 78;
    Button(TEXT("RestartScene"),TEXT("재시작"),width-182,11,75);
    Button(TEXT("PlaySceneExit"),TEXT("목록 복귀"),width-102,11,92);
    Button(TEXT("Pause"),pc->bSimulationPaused?TEXT("재개 ▷"):TEXT("정지 Ⅱ"),12,height-48,65);
    Button(TEXT("Speed1"),TEXT("1×"),80,height-48,38,pc->SimulationSpeed==1);
    Button(TEXT("Speed2"),TEXT("2×"),121,height-48,38,pc->SimulationSpeed==2);
    Button(TEXT("Speed3"),TEXT("3×"),162,height-48,38,pc->SimulationSpeed==3);
    DrawText(FString::Printf(TEXT("공사 %d건 · %s"),queue,pc->bSimulationPaused?TEXT("일시정지"):TEXT("진행 중")),
        AccentColor,right,top,OVTypography::Font(EOVTextRole::Heading));
    if (BaseInspect >= 0 && static_cast<std::size_t>(BaseInspect)<build.Modules().size())
    {
        const auto& item = build.Modules()[BaseInspect];
        DrawText(ModuleName(item.Type),FLinearColor::White,right,112,OVTypography::Font(EOVTextRole::Heading));
        const auto network=build.Network(false);
        bool accessible=false;
        for (size_t i=0;i<network.size();++i) if (network[i] && OVBase::Connected(item,build.Modules()[i])) accessible=true;
        const AOVShip* blocker=BasePresentation->ConstructionBlocker(BaseInspect);
        const FString phase=blocker ? FString::Printf(TEXT("%s 이동 대기"),*blocker->ShipName) :
            (accessible ? TEXT("공사 중") : TEXT("연결 대기"));
        DrawText(item.Remaining>0?FString::Printf(TEXT("%s · %.1f초"),*phase,item.Remaining):FString(TEXT("가동 중")),
            FLinearColor::White,right,142,OVTypography::Font(EOVTextRole::Body));
        if (item.Remaining>0) Button(TEXT("BaseCancel"),TEXT("공사 취소"),right,180,220);
    }
    if (!BaseNotice.IsEmpty()) DrawText(BaseNotice,FLinearColor::White,220,61,OVTypography::Font(EOVTextRole::Body));
}
