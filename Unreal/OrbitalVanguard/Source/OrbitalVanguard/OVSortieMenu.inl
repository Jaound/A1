// Included by the title widget to share the game's menu typography and button styles.
void UOVTitleScreenWidget::BuildSortie(UOverlay* root)
{
 SortiePanel=WidgetTree->ConstructWidget<UBorder>();SortiePanel->SetPadding(FMargin(18));
 SortiePanel->SetBrush(OVStyle::MakeRoundedBoxBrush(FLinearColor(.012,.025,.045,.99),OVStyle::SlateBorder,8,1));
 auto* frame=root->AddChildToOverlay(SortiePanel);frame->SetPadding(FMargin(16));frame->SetHorizontalAlignment(HAlign_Fill);frame->SetVerticalAlignment(VAlign_Fill);
 SortiePanel->SetVisibility(ESlateVisibility::Collapsed);
}
void UOVTitleScreenWidget::SortieClick(FName action,int32 index)
{
 auto* pc=Cast<AOVController>(GetOwningPlayer());if(!pc)return;auto* play=pc->GetPlayScenario();
 if(CurrentPanel!=EOVTitleMenuPanel::Sortie||play->ActiveId!="SortiePrep")return;
 if(action=="Cancel"){FName id=play->Draft.Mission;pc->LeavePlayScenario();auto* hud=CastChecked<AOVHUD>(pc->GetHUD());hud->OpenTitleScreen();ShowPanel(EOVTitleMenuPanel::ScenarioSelect);SelectScenario(id);return;}
 if(action=="Start"){if(play->CommitSortie())return;CastChecked<AOVHUD>(pc->GetHUD())->OpenTitleScreen();ShowPanel(EOVTitleMenuPanel::Sortie);}
 else if((action=="Altitude"||action=="Yaw")&&play->Draft.Hulls.IsValidIndex(play->SortieSelected)){
   const auto& h=play->Draft.Hulls[play->SortieSelected];play->SetSortiePosition(h.Position+FVector(0,0,action=="Altitude"?index*2500:0),h.Yaw+(action=="Yaw"?index*45:0));}
 else play->SortieCommand(action,index);
 RefreshSortie();
}
void UOVTitleScreenWidget::RefreshSortie()
{
 auto* pc=Cast<AOVController>(GetOwningPlayer());if(!pc||!SortiePanel)return;
 auto* play=pc->GetPlayScenario();auto& draft=play->Draft;int stage=play->SortieStage;
 auto* mission=UOVPlayScenarioSubsystem::Definitions().FindByPredicate([&](const auto& def){return def.Id==draft.Mission;});
 if(!mission || stage<0 || stage>6)return;
 auto* stack=WidgetTree->ConstructWidget<UVerticalBox>();SortiePanel->SetContent(stack);
 auto label=[&](UPanelWidget* parent,const FString& s,int points=15,bool strong=false,FLinearColor tint=OVStyle::TextWhite){
  auto* text=OVStyle::MakeLabel(WidgetTree,s,points,tint,strong);text->SetAutoWrapText(true);text->SetMargin(FMargin(4,4));parent->AddChild(text);};
 auto button=[&](UPanelWidget* parent,FName action,int n,const FString& s,bool on=false,bool primary=false,UTexture* portrait=nullptr){
  auto* b=WidgetTree->ConstructWidget<UOVScenarioPickButton>();b->OwnerWidget=this;b->SortieAction=action;b->Choice=n;
  b->SetStyle(primary?OVStyle::MakePrimaryButtonStyle():OVStyle::MakeSecondaryButtonStyle(on));
  auto* row=WidgetTree->ConstructWidget<UHorizontalBox>();CastChecked<UButtonSlot>(b->AddChild(row))->SetHorizontalAlignment(HAlign_Fill);
  if(portrait){auto* image=WidgetTree->ConstructWidget<UImage>();image->SetBrushResourceObject(portrait);auto* size=WidgetTree->ConstructWidget<USizeBox>();size->SetWidthOverride(action=="Add"?78:40);size->SetHeightOverride(40);size->SetContent(image);row->AddChild(size);}
  auto* text=OVStyle::MakeLabel(WidgetTree,s,14,primary?OVStyle::DarkText:on?OVStyle::CyanAccent:OVStyle::TextWhite,on||primary);
  text->SetJustification(portrait?ETextJustify::Left:ETextJustify::Center);text->SetAutoWrapText(parent->IsA<UVerticalBox>());text->SetMargin(FMargin(6,4));row->AddChildToHorizontalBox(text)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  b->OnClicked.AddDynamic(b,&UOVScenarioPickButton::HandleClicked);parent->AddChild(b);return b;};
 const TCHAR* stages[]={TEXT("임무"),TEXT("함대 선택"),TEXT("함선 설계"),TEXT("함대 배치"),TEXT("영웅 선택"),TEXT("영웅 배치"),TEXT("최종 브리핑")};
 const TCHAR* maps[]={TEXT("일반 우주"),TEXT("블랙홀"),TEXT("소행성 지대")};
 int map=draft.Map==EOVSpaceHazardKind::BlackHole?1:draft.Map==EOVSpaceHazardKind::AsteroidField?2:0;
 label(stack,FString::Printf(TEXT("%s   /   %s"),mission->Title,stages[stage]),23,true);
 label(stack,FString::Printf(TEXT("%d / 7 단계     %s     함대 %d / %d 코스트     영웅 %d / %d명"),stage+1,maps[map],play->SortieCost(),play->SortieBudget(),draft.Heroes.Num(),play->SortieHeroLimit()),15,false,OVStyle::CyanAccent);
 auto* columns=WidgetTree->ConstructWidget<UHorizontalBox>();stack->AddChildToVerticalBox(columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 auto column=[&](){auto* scroll=WidgetTree->ConstructWidget<UScrollBox>();auto* slot=columns->AddChildToHorizontalBox(scroll);slot->SetPadding(FMargin(5));slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));auto* body=WidgetTree->ConstructWidget<UVerticalBox>();scroll->AddChild(body);return body;};
 auto* left=column();auto* mid=column();auto* right=column();
 const TCHAR* sizes[]={TEXT("전체 체급"),TEXT("소형"),TEXT("중형"),TEXT("대형"),TEXT("초대형")};
 const TCHAR* roles[]={TEXT("모든 역할"),TEXT("전투"),TEXT("항공"),TEXT("지원")};
 const TCHAR* rooms[]={TEXT("빈 공간"),TEXT("코어"),TEXT("함교"),TEXT("생명 유지"),TEXT("엔진"),TEXT("무기 제어"),TEXT("탄약고"),TEXT("축전실"),TEXT("미사일 적재실"),TEXT("쉴드 제어"),TEXT("레이더"),TEXT("요격 제어"),TEXT("포드 발사실"),TEXT("격납고"),TEXT("함재기 관제"),TEXT("자원 저장소"),TEXT("보안실")};
 const TCHAR* families[]={TEXT("함포"),TEXT("빔"),TEXT("미사일"),TEXT("요격")};
 const TCHAR* heroRoles[]={TEXT("병력"),TEXT("근거리"),TEXT("원거리"),TEXT("지원"),TEXT("지휘")};
 auto ships=[&](UVerticalBox* dest){label(dest,TEXT("아군 편성"),17,true,OVStyle::CyanAccent);for(int i=0;i<draft.Hulls.Num();++i)button(dest,"Ship",i,FString::Printf(TEXT("%02d   %s"),i+1,*OVCalibration::GetHullName(draft.Hulls[i].Entry.Hull)),i==play->SortieSelected);};
 auto* roster=pc->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
 auto picture=[&](UVerticalBox* dest,UTexture* texture,float height){if(!texture)return;auto* im=WidgetTree->ConstructWidget<UImage>();im->SetBrushResourceObject(texture);auto* box=WidgetTree->ConstructWidget<USizeBox>();box->SetHeightOverride(height);box->SetContent(im);dest->AddChild(box);};
 auto preview=[&](){auto* ship=play->InspectSortie(play->SortieSelected);if(ship){if(!SortieMirror)SortieMirror=NewObject<UOVShipPreview>(this);SortieMirror->Update(ship,UOVShipPreview::DefaultHullView(),1,1.8,INDEX_NONE,512);auto* canvas=WidgetTree->ConstructWidget<UCanvasPanel>();
    auto* im=WidgetTree->ConstructWidget<UImage>();im->SetBrushResourceObject(SortieMirror->GetTexture());auto* imageSlot=canvas->AddChildToCanvas(im);imageSlot->SetSize(FVector2D(480,267));
    if(stage==2 && play->bSortieExternal){const auto& mounts=ship->Systems->Layout->WeaponMounts;
     for(int i=0;i<mounts.Num();++i){auto* pin=button(canvas,"Slot",i,FString::FromInt(i+1),i==play->SortieMount);
      auto* pos=CastChecked<UCanvasPanelSlot>(pin->Slot);pos->SetAlignment(FVector2D(.5,.5));pos->SetAutoSize(true);
      pos->SetPosition(SortieMirror->Project(mounts[i].LocalPosition)*FVector2D(480,267));}}
    auto* natural=WidgetTree->ConstructWidget<USizeBox>();natural->SetWidthOverride(480);natural->SetHeightOverride(267);natural->SetContent(canvas);
    auto* fit=WidgetTree->ConstructWidget<UScaleBox>();fit->SetStretch(EStretch::ScaleToFit);fit->SetContent(natural);auto* area=WidgetTree->ConstructWidget<USizeBox>();area->SetHeightOverride(stage==2?220:185);area->SetContent(fit);mid->AddChild(area);}return ship;};
 auto deploymentPlot=[&](bool editable){
   auto* plot=CreateWidget<UOVSortiePlot>(pc);plot->Menu=this;
   plot->SetVisibility(editable?ESlateVisibility::Visible:ESlateVisibility::HitTestInvisible);
   auto* box=WidgetTree->ConstructWidget<USizeBox>();box->SetHeightOverride(editable?245:155);box->SetContent(plot);mid->AddChild(box);
 };
 auto roleName=[](FName hull){
   if(hull=="air_defense")return TEXT("방공 호위");if(hull=="radar")return TEXT("공유 탐지");
   if(hull=="carrier"||hull=="battlecarrier")return TEXT("항공 작전");if(hull=="corvette")return TEXT("전방 차폐");
   if(hull=="repair")return TEXT("보급·수리");if(hull=="transport"||hull=="mining")return TEXT("물류");
   if(hull=="assault")return TEXT("강습 운송");return TEXT("대함 전투");
 };
 if(stage==0){
   label(left,TEXT("작전 환경"),17,true);
   const TCHAR* info[]={TEXT("환경 효과 없이 함대 전술에 집중합니다."),TEXT("중력권 안에서는 함선이 끌립니다. 중심부 진입 시 완파됩니다."),TEXT("소행성은 이동과 사선을 막습니다. 부서지면 엄폐가 사라집니다.")};
   for(int i=0;i<3;++i)button(left,"Map",i,maps[i],i==map);
   picture(mid,ScenarioPhotos.FindRef(map==1?FName("BlackHoleTrial"):draft.Mission),150);label(mid,maps[map],19,true);label(mid,info[map]);
   label(right,TEXT("작전 목표"),17,true);label(right,mission->Objective);label(right,TEXT("적 함대가 교전 능력을 잃으면 승리합니다. 준비 중에는 전투 시간이 흐르지 않습니다."));
 }else if(stage==1){
   button(left,"Filter",0,sizes[play->SortieSizeFilter]);button(left,"Role",0,roles[play->SortieRoleFilter]);
   auto ids=OVCalibration::GetHullIds();for(int i=0;i<ids.Num();++i){FOVHullMetadata m;if(!OVCalibration::GetHullMetadata(ids[i],m))continue;
    int role=m.bAllowAircraftOperations?2:(ids[i]=="repair"||ids[i]=="mining"||ids[i]=="transport"||ids[i]=="radar")?3:1;
    if((play->SortieSizeFilter&&play->SortieSizeFilter!=m.SizeClass)||(play->SortieRoleFilter&&play->SortieRoleFilter!=role))continue;
    button(left,"Add",i,FString::Printf(TEXT("+ %s   %s · %s · %d"),*OVCalibration::GetHullName(ids[i]),sizes[m.SizeClass],roleName(ids[i]),m.FleetCost),false,false,CastChecked<AOVHUD>(pc->GetHUD())->HullCardImage(ids[i]));}
   if(draft.Hulls.IsValidIndex(play->SortieSelected)){const auto id=draft.Hulls[play->SortieSelected].Entry.Hull;preview();label(mid,OVCalibration::GetHullName(id),19,true);FOVHullMetadata m;OVCalibration::GetHullMetadata(id,m);label(mid,FString::Printf(TEXT("%s · %d코스트 · 공격 %d문 / 요격 %d문"),sizes[m.SizeClass],m.FleetCost,m.AttackMountCount,m.PointDefenseCount));
    button(mid,"Detail",0,play->bSortieDetails?TEXT("상세 접기"):TEXT("장비와 시설 상세"));if(play->bSortieDetails)label(mid,m.Description);button(mid,"Remove",play->SortieSelected,TEXT("선택 함선 제외"));}
   button(right,"Recommend",0,TEXT("추천 편성 불러오기"));ships(right);
 }else if(stage==2||stage==5){
   ships(left);auto* ship=preview();
   if(ship&&ship->Systems->Layout){auto* layout=ship->Systems->Layout.Get();
    if(stage==2){button(mid,"Tab",0,play->bSortieExternal?TEXT("내부 시설 보기"):TEXT("외장 무장 보기"));button(left,"Defaults",0,TEXT("기본 장비 복원"));}
    if(stage==2&&play->bSortieExternal){
      for(int i=0;i<layout->WeaponMounts.Num();++i){auto& w=layout->WeaponMounts[i];button(mid,"Slot",i,FString::Printf(TEXT("%d번 · %s · T%.0f"),i+1,families[int(w.Type)],w.Tier),i==play->SortieMount);}
      label(right,TEXT("교체할 무장"),17,true);for(int i=0;i<pc->Combat->Settings->Weapons.Num();++i){auto& w=pc->Combat->Settings->Weapons[i];button(right,"Equip",i,FString::Printf(TEXT("%s T%d · %.0fm / %.1f초"),families[int(w.Family)],w.Tier,w.Range/100,w.Interval));}
    }else{
      auto* decks=WidgetTree->ConstructWidget<UHorizontalBox>();mid->AddChild(decks);TSet<int> levels;
      for(auto& r:layout->Slots)if(!levels.Contains(r.DeckId)){levels.Add(r.DeckId);button(decks,"Deck",r.DeckId,FString::Printf(TEXT("%d층"),r.DeckId),r.DeckId==play->SortieDeck);}
      TArray<int> onDeck;TArray<double> xs,ys;for(int i=0;i<layout->Slots.Num();++i){const auto& r=layout->Slots[i];if(r.DeckId==play->SortieDeck){onDeck.Add(i);xs.AddUnique(r.LocalCenter.X);ys.AddUnique(r.LocalCenter.Y);}}xs.Sort();ys.Sort();
      auto* grid=WidgetTree->ConstructWidget<UUniformGridPanel>();mid->AddChild(grid);for(int i:onDeck){const auto& r=layout->Slots[i];auto* cell=WidgetTree->ConstructWidget<UVerticalBox>();grid->AddChildToUniformGrid(cell,xs.IndexOfByKey(r.LocalCenter.X),ys.IndexOfByKey(r.LocalCenter.Y));button(cell,"Room",i,rooms[int(r.Facility)],i==play->SortieRoom);}
      if(stage==2){label(right,TEXT("시설 위치 교환"),17,true);label(right,TEXT("도면에서 시설을 고른 뒤 교환할 방을 선택하세요."));for(int i:onDeck)button(right,"Swap",i,FString::Printf(TEXT("%d번 · %s"),i+1,rooms[int(layout->Slots[i].Facility)]));}
      else{label(right,TEXT("탑승 인원"),17,true);for(int i=0;i<draft.Heroes.Num();++i){auto& h=draft.Heroes[i];auto* def=roster->Definition(h.Definition);if(def)button(right,"Person",i,def->Name+(h.HullKey?TEXT(" · 배치"):TEXT(" · 미배치")),i==play->SortieHero,false,def->Portrait);}
       if(draft.Heroes.IsValidIndex(play->SortieHero)){
        const auto& who=draft.Heroes[play->SortieHero];auto* host=draft.Hulls.FindByPredicate([&](const auto& h){return h.Key==who.HullKey;});
        if(host){label(right,TEXT("탑승 · ")+OVCalibration::GetHullName(host->Entry.Hull));if(host->Key==draft.Hulls[play->SortieSelected].Key){
         int where=layout->GetSlotIndex(who.StartRoom);if(layout->Slots.IsValidIndex(where))label(right,FString::Printf(TEXT("시작 · %d층 %s"),layout->Slots[where].DeckId,rooms[int(layout->Slots[where].Facility)]));
         int post=layout->GetSlotIndex(who.DutyRoom);label(right,layout->Slots.IsValidIndex(post)?FString(TEXT("보직 · "))+rooms[int(layout->Slots[post].Facility)]:who.bBoard?TEXT("임무 · 강습"):TEXT("임무 · 수호"));}}
        button(right,"Embark",0,TEXT("이 방에서 시작"));button(right,"Duty",0,TEXT("이 시설 보직 배정"));button(right,"Board",0,TEXT("강습 편제 전환"));}}
    }}
 }else if(stage==3){
   ships(left);label(mid,TEXT("아군 시작 구역   →   적 방향"),16,true);
   deploymentPlot(true);
   label(mid,TEXT("왼쪽 구역을 클릭해 배치하세요. 흰 표식과 수직선이 선택 함선의 위치와 고도를 보여 줍니다."));button(right,"Auto",0,TEXT("자동 배치"));
   if(draft.Hulls.IsValidIndex(play->SortieSelected)){auto& h=draft.Hulls[play->SortieSelected];label(right,FString::Printf(TEXT("위치 %.0f, %.0fm\n고도 %.0fm · 방향 %.0f°"),h.Position.X/100,h.Position.Y/100,h.Position.Z/100,h.Yaw));}
   button(right,"Altitude",1,TEXT("고도 +25m"));button(right,"Altitude",-1,TEXT("고도 −25m"));button(right,"Yaw",1,TEXT("우현 45°"));button(right,"Yaw",-1,TEXT("좌현 45°"));
   if(map==1)label(right,TEXT("황색선 · 중력권\n적색선 · 완파 중심"));if(map==2)label(right,TEXT("모래색 원 · 소행성 실체"));
 }else if(stage==4){
   label(left,TEXT("시험 영웅 명단"),17,true);auto ids=play->SortieRoster();for(int i=0;i<ids.Num();++i){auto* h=roster->Definition(ids[i]);if(!h)continue;
    bool on=draft.Heroes.ContainsByPredicate([&](const auto& chosen){return chosen.Definition==h->Id;});button(left,"Hero",i,h->Name+FString::Printf(TEXT(" · %s · Lv.1"),heroRoles[int(h->Role)]),on,false,h->Portrait);}
   if(!draft.Heroes.IsEmpty()){auto* h=roster->Definition(draft.Heroes.Last().Definition);if(h){picture(mid,h->Portrait,155);label(mid,h->Name,18,true);label(mid,h->Signature);}}
   label(right,TEXT("선택한 영웅"),17,true);for(auto& h:draft.Heroes)if(auto* def=roster->Definition(h.Definition))label(right,def->Name);label(right,TEXT("영웅 없이도 출격할 수 있습니다."));
 }else{
   label(left,TEXT("아군 공개 편성"),17,true,OVStyle::CyanAccent);
   TMap<FName,int> counts;for(const auto& hull:draft.Hulls)++counts.FindOrAdd(hull.Entry.Hull);
   for(auto id:OVCalibration::GetHullIds())if(int n=counts.FindRef(id))label(left,FString::Printf(TEXT("%s  × %d"),*OVCalibration::GetHullName(id),n));
   for(const auto& who:draft.Heroes){const auto* hero=roster->Definition(who.Definition);const auto* host=draft.Hulls.FindByPredicate([&](const auto& h){return h.Key==who.HullKey;});
    if(hero && host)label(left,hero->Name+TEXT(" · ")+OVCalibration::GetHullName(host->Entry.Hull)+(who.bBoard?TEXT(" · 강습"):!who.DutyRoom.IsNone()?TEXT(" · 보직"):TEXT(" · 수호")));}
   label(mid,maps[map],18,true);deploymentPlot(false);label(mid,FString::Printf(TEXT("아군 함선 %d척 · 영웅 %d명"),draft.Hulls.Num(),draft.Heroes.Num()));
   for(int i=0;i<6;++i)button(mid,"Review",i,FString(stages[i])+TEXT(" 수정"));
   label(right,TEXT("적 공개 편성"),17,true,FLinearColor(1,.48,.4));for(auto& entry:mission->Fleet)label(right,FString::Printf(TEXT("%s  × %d · %s"),*OVCalibration::GetHullName(entry.Hull),entry.Count,roleName(entry.Hull)));
 }
 if(!play->SortieFeedback.IsEmpty())label(stack,play->SortieFeedback,15,true,OVStyle::AmberBadge);
 auto* bar=WidgetTree->ConstructWidget<UHorizontalBox>();stack->AddChildToVerticalBox(bar);button(bar,"Cancel",0,TEXT("준비 취소"));auto* spacer=WidgetTree->ConstructWidget<USpacer>();bar->AddChildToHorizontalBox(spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 if(stage>0)button(bar,"Prev",0,TEXT("이전"));button(bar,stage==6?"Start":"Next",0,stage==6?TEXT("전투 시작"):TEXT("다음"),false,true);
}
