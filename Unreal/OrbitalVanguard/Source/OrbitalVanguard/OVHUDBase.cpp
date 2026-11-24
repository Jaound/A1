#include "OVHUD.h"

#include "Camera/CameraActor.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "OVBasePresentation.h"
#include "OVController.h"

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
    default: return TEXT("자원 창고");
    }
}
const TCHAR* PlacementStatus(OVBase::Error error)
{
    switch (error)
    {
    case OVBase::Error::None: return TEXT("건설 가능");
    case OVBase::Error::Occupied: return TEXT("공간이 이미 사용 중입니다.");
    case OVBase::Error::NoConnection: return TEXT("맞물리는 연결 포트가 없습니다.");
    case OVBase::Error::Funds: return TEXT("일반 자원이 부족합니다.");
    case OVBase::Error::Limit: return TEXT("코어 I 설치 한도에 도달했습니다.");
    case OVBase::Error::Busy: return TEXT("시설 건설 슬롯을 사용 중입니다.");
    default: return TEXT("시험 배치 범위를 벗어났습니다.");
    }
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

void AOVHUD::ToggleBaseView()
{
    auto* pc = Cast<AOVController>(PlayerOwner);
    if (!pc || !pc->RangeCamera) return;
    if (!IsValid(BasePresentation))
        BasePresentation = GetWorld()->SpawnActor<AOVBasePresentation>(
            FVector(0,0,-400000),FRotator::ZeroRotator);
    if (!IsValid(BasePresentation)) return;
    if (bBaseViewOpen)
    {
        bBaseViewOpen = false;
        BasePresentation->Show(false);
        pc->SetViewTarget(pc->RangeCamera);
        return;
    }
    BasePresentation->Show(true);
    if (!IsValid(BasePresentation->View()))
    {
        BasePresentation->Show(false);
        return;
    }
    bBaseViewOpen = true;
    bStarMapOpen = false;
    CancelPointerDrag();
    pc->SetViewTarget(BasePresentation->View());
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
    if (action == TEXT("BaseBack")) ToggleBaseView();
    else if (action == TEXT("BaseCorridor")) BasePick = 1;
    else if (action == TEXT("BaseJunction")) BasePick = 2;
    else if (action == TEXT("BaseStorage")) BasePick = 3;
    else if (action == TEXT("BaseInspect")) BasePick = 0;
    else if (action == TEXT("BaseCancel"))
    {
        if (BaseInspect >= 0 && BasePresentation->Build.Cancel(BaseInspect))
        {
            BaseNotice = TEXT("건설을 취소하고 자원을 반환했습니다.");
            BaseInspect = INDEX_NONE;
            BasePresentation->RefreshMeshes();
        }
        else BaseNotice = TEXT("진행 중인 공사를 선택하세요. 연결을 사용하는 다른 공사가 있으면 먼저 취소하세요.");
    }
    return true;
}

void AOVHUD::AdvanceBaseConstruction(float gameSeconds)
{
    if (!IsValid(BasePresentation)) return;
    auto& build = BasePresentation->Build;
    const auto before = build.Network(false);
    build.Advance(gameSeconds);
    if (before != build.Network(false)) BasePresentation->RefreshMeshes();
}

void AOVHUD::UpdateBaseView(float dt)
{
    auto* pc = Cast<AOVController>(PlayerOwner);
    if (!pc || !IsValid(BasePresentation)) return;
    auto pressed = [&](FKey key) { return pc->WasInputKeyJustPressed(key); };
    auto down = [&](FKey key) { return int(pc->IsInputKeyDown(key)); };
    if (pressed(EKeys::Escape)) { ToggleBaseView(); return; }
    if (pressed(EKeys::F1)) pc->Action(TEXT("Pause"));
    if (pressed(EKeys::F2)) pc->Action(TEXT("Speed1"));
    if (pressed(EKeys::F3)) pc->Action(TEXT("Speed2"));
    if (pressed(EKeys::F4)) pc->Action(TEXT("Speed3"));
    BaseTurns = (BaseTurns+4+int(pressed(EKeys::E))-int(pressed(EKeys::Q)))%4;
    BaseLayer = FMath::Clamp(BaseLayer+int(pressed(EKeys::PageUp))-int(pressed(EKeys::PageDown)),-8,8);
    BasePresentation->UpdateView(dt, down(EKeys::Right)-down(EKeys::Left),down(EKeys::Up)-down(EKeys::Down),
        int(pressed(EKeys::MouseScrollUp))-int(pressed(EKeys::MouseScrollDown)));

    float x,y;
    if (!pressed(EKeys::LeftMouseButton) || !pc->GetMousePosition(x,y)) return;
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
    const OVBase::Module candidate{static_cast<OVBase::Kind>(BasePick),cell,BaseTurns};
    BaseNotice = PlacementStatus(build.Check(candidate));
    if (build.Start(candidate))
    {
        BaseNotice = TEXT("공간과 자원을 예약했습니다.");
        BaseInspect = static_cast<int32>(build.Modules().size())-1;
        BasePresentation->RefreshMeshes();
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
    for (int i=-12; i<=12; ++i)
    {
        const double edge = i+.5;
        const FLinearColor grid(.16f,.47f,.63f,.5f);
        line(plane(edge,-11.5),plane(edge,12.5),grid,1);
        line(plane(-11.5,edge),plane(12.5,edge),grid,1);
    }

    float x,y;
    OVBase::Cell cell;
    auto& build = BasePresentation->Build;
    if (BasePick != 0 && pc->GetMousePosition(x,y) && x/UIScale >= 210 &&
        x/UIScale <= width-250 && y/UIScale >= 52 && PointerCell(*pc,*BasePresentation,BaseLayer,cell))
    {
        const OVBase::Module candidate{static_cast<OVBase::Kind>(BasePick),cell,BaseTurns};
        const auto dim = OVBase::Dimensions(candidate.Type);
        const auto center = BasePresentation->CellCenter(cell);
        const auto rotation = FRotator(0,90*BaseTurns,0);
        const auto point = [&](double bx,double by) { return center+rotation.RotateVector(FVector(bx,by,0))*OVBase::CellCm; };
        const FVector corners[]{point(-.5,-.5),point(dim.X-.5,-.5),point(dim.X-.5,dim.Y-.5),point(-.5,dim.Y-.5)};
        const auto error = build.Check(candidate);
        const FLinearColor color = error == OVBase::Error::None ? FLinearColor::Green : FLinearColor::Red;
        for (int i=0; i<4; ++i) line(corners[i],corners[(i+1)%4],color,2);
        DrawText(PlacementStatus(error),color,220,height-35,OVTypography::Font(EOVTextRole::Body));
    }

    DrawRect(PanelColor,0,0,width,52);
    DrawRect(PanelColor,0,52,210,height-52);
    DrawRect(PanelColor,width-250,52,250,height-52);
    Button(TEXT("BaseBack"),TEXT("함대 화면"),12,11,105);
    int links = 0, facilities = 0, queue = 0, warehouses = 0;
    for (const auto& item : build.Modules())
    {
        if (OVBase::IsLink(item.Type)) ++links;
        if (item.Type == OVBase::Kind::Storage) { ++facilities; if(item.Remaining<=0) ++warehouses; }
        if (item.Remaining>0) ++queue;
    }
    DrawText(FString::Printf(TEXT("기지 시험장 · 코어 I   일반 %d/%d   기능 %d/8 · 연결 %d/24"),
        build.Funds(),6000+warehouses*6000,facilities,links), AccentColor,130,16,OVTypography::Font(EOVTextRole::Heading));
    Button(TEXT("BaseInspect"),TEXT("시설 선택"),12,77,184,BasePick==0);
    Button(TEXT("BaseCorridor"),TEXT("연결 통로 · 40"),12,115,184,BasePick==1);
    Button(TEXT("BaseJunction"),TEXT("분기 허브 · 40"),12,153,184,BasePick==2);
    Button(TEXT("BaseStorage"),TEXT("자원 창고 · 300"),12,191,184,BasePick==3);
    DrawText(FString::Printf(TEXT("고도 %d m · 회전 %d°"),BaseLayer*25,BaseTurns*90),
        FLinearColor::White,12,238,OVTypography::Font(EOVTextRole::Body));
    DrawText(TEXT("Q/E 회전 · PgUp/PgDn 고도\n방향키 시점 · 휠 확대\nF1 시간 제어 · Esc 함대"),
        FLinearColor::White,12,267,OVTypography::Font(EOVTextRole::Body));

    const float right = width-236;
    constexpr float top = 78;
    DrawText(FString::Printf(TEXT("공사 %d건 · %s"),queue,pc->bSimulationPaused?TEXT("일시정지"):TEXT("진행 중")),
        AccentColor,right,top,OVTypography::Font(EOVTextRole::Heading));
    if (BaseInspect >= 0 && static_cast<std::size_t>(BaseInspect)<build.Modules().size())
    {
        const auto& item = build.Modules()[BaseInspect];
        DrawText(ModuleName(item.Type),FLinearColor::White,right,112,OVTypography::Font(EOVTextRole::Heading));
        DrawText(item.Remaining>0?FString::Printf(TEXT("남은 작업 %.1f초"),item.Remaining):FString(TEXT("가동 중")),
            FLinearColor::White,right,142,OVTypography::Font(EOVTextRole::Body));
        if (item.Remaining>0) Button(TEXT("BaseCancel"),TEXT("공사 취소 · 전액 환불"),right,180,220);
    }
    if (!BaseNotice.IsEmpty()) DrawText(BaseNotice,FLinearColor::White,220,61,OVTypography::Font(EOVTextRole::Body));
}
