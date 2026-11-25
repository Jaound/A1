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
 else if(action=="ShowRooms" || action=="ShowWeapons"){
   if((action=="ShowWeapons")!=play->bSortieExternal)play->SortieCommand("Tab",0);
 }else if(action=="EquipAntiShip"){
   auto* ship=play->InspectSortie(play->SortieSelected);
   if(ship && ship->Systems->Layout && ship->Systems->Layout->WeaponMounts.IsValidIndex(play->SortieMount) && ship->Systems->Layout->WeaponMounts[play->SortieMount].IsAntiShipMissile())play->SortieCommand("Equip",index);
   else play->SortieFeedback=TEXT("대함 미사일은 전략미사일함의 전용 장착점에 놓으세요.");
 }else play->SortieCommand(action,index);
 RefreshSortie();
}
void UOVTitleScreenWidget::RefreshSortie()
{
 auto* pc=Cast<AOVController>(GetOwningPlayer());if(!pc||!SortiePanel)return;
 auto* play=pc->GetPlayScenario();auto& draft=play->Draft;int stage=play->SortieStage;
 auto* mission=UOVPlayScenarioSubsystem::Definitions().FindByPredicate([&](const auto& def){return def.Id==draft.Mission;});
 if(!mission || stage<0 || stage>6)return;
 float scrollOffsets[3]={};
 const bool samePage=stage==SortieScrollStage;
 if(samePage){
   for(int i=0;i<3;++i)if(SortieColumns[i].IsValid() && (i==0 || stage!=2 || bSortieScrollExternal==play->bSortieExternal))
     scrollOffsets[i]=SortieColumns[i]->GetScrollOffset();
 }
 SortieScrollStage=stage;bSortieScrollExternal=play->bSortieExternal;
 auto* stack=WidgetTree->ConstructWidget<UVerticalBox>();SortiePanel->SetContent(stack);
 auto label=[&](UPanelWidget* parent,const FString& s,int points=15,bool strong=false,FLinearColor tint=OVStyle::TextWhite){
  auto* text=OVStyle::MakeLabel(WidgetTree,s,points,tint,strong);text->SetAutoWrapText(true);text->SetMargin(FMargin(4,4));parent->AddChild(text);};
 // Give the brush its native dimensions before fitting it inside the bounded artwork area.
 auto fitImage=[&](UTexture* texture){
  auto* image=WidgetTree->ConstructWidget<UImage>();
  image->SetBrushResourceObject(texture);
  image->SetDesiredSizeOverride(FVector2D(texture->GetSurfaceWidth(),texture->GetSurfaceHeight()));
  auto* fit=WidgetTree->ConstructWidget<UScaleBox>();
  fit->SetStretch(EStretch::ScaleToFit);fit->SetContent(image);return fit;
 };
 auto button=[&](UPanelWidget* parent,FName action,int n,const FString& s,bool on=false,bool primary=false,UTexture* portrait=nullptr){
  auto* b=WidgetTree->ConstructWidget<UOVScenarioPickButton>();b->OwnerWidget=this;b->SortieAction=action;b->Choice=n;
  b->SetStyle(primary?OVStyle::MakePrimaryButtonStyle():OVStyle::MakeSecondaryButtonStyle(on));
  auto* row=WidgetTree->ConstructWidget<UHorizontalBox>();CastChecked<UButtonSlot>(b->AddChild(row))->SetHorizontalAlignment(HAlign_Fill);
  if(portrait){auto* size=WidgetTree->ConstructWidget<USizeBox>();size->SetWidthOverride(action=="Add"?78:40);size->SetHeightOverride(40);size->SetContent(fitImage(portrait));row->AddChild(size);}
  auto* text=OVStyle::MakeLabel(WidgetTree,s,14,primary?OVStyle::DarkText:on?OVStyle::CyanAccent:OVStyle::TextWhite,on||primary);
  text->SetJustification(portrait?ETextJustify::Left:ETextJustify::Center);text->SetAutoWrapText(parent->IsA<UVerticalBox>());text->SetMargin(FMargin(6,4));row->AddChildToHorizontalBox(text)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  b->OnClicked.AddDynamic(b,&UOVScenarioPickButton::HandleClicked);parent->AddChild(b);return b;};
 const TCHAR* stages[]={TEXT("임무"),TEXT("함대 선택"),TEXT("함선 설계"),TEXT("함대 배치"),TEXT("영웅 선택"),TEXT("영웅 배치"),TEXT("최종 브리핑")};
 const TCHAR* maps[]={TEXT("일반 우주"),TEXT("블랙홀"),TEXT("소행성 지대")};
 int map=draft.Map==EOVSpaceHazardKind::BlackHole?1:draft.Map==EOVSpaceHazardKind::AsteroidField?2:0;
 label(stack,FString::Printf(TEXT("%s   /   %s"),mission->Title,stages[stage]),23,true);
 label(stack,FString::Printf(TEXT("%d / 7 단계     %s     함대 %d / %d 코스트     영웅 %d / %d명"),stage+1,maps[map],play->SortieCost(),play->SortieBudget(),draft.Heroes.Num(),play->SortieHeroLimit()),15,false,OVStyle::CyanAccent);
 auto* columns=WidgetTree->ConstructWidget<UHorizontalBox>();stack->AddChildToVerticalBox(columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 const bool design=stage==2 || stage==5;
 int32 columnIndex=0;
 auto column=[&](float weight){
   const int32 index=columnIndex++;
   auto* body=WidgetTree->ConstructWidget<UVerticalBox>();
   UHorizontalBoxSlot* slot=nullptr;
   if(design && index==1){
     SortieColumns[index].Reset();
     slot=columns->AddChildToHorizontalBox(body);
   }else{
     auto* scroll=WidgetTree->ConstructWidget<UScrollBox>();
     SortieColumns[index]=scroll;
     scroll->AddChild(body);slot=columns->AddChildToHorizontalBox(scroll);
   }
   slot->SetPadding(FMargin(5));FSlateChildSize width(ESlateSizeRule::Fill);width.Value=weight;slot->SetSize(width);
   return body;
 };
 auto* left=column(design?.64f:1.f);auto* mid=column(design?1.65f:1.f);auto* right=column(design?.9f:1.f);
 const TCHAR* sizes[]={TEXT("전체 체급"),TEXT("소형"),TEXT("중형"),TEXT("대형"),TEXT("초대형")};
 const TCHAR* roles[]={TEXT("모든 역할"),TEXT("전투"),TEXT("항공"),TEXT("지원")};

 const TCHAR* heroRoles[]={TEXT("병력"),TEXT("근거리"),TEXT("원거리"),TEXT("지원"),TEXT("지휘")};
 auto ships=[&](UVerticalBox* dest){label(dest,TEXT("아군 편성"),17,true,OVStyle::CyanAccent);for(int i=0;i<draft.Hulls.Num();++i)button(dest,"Ship",i,FString::Printf(TEXT("%02d   %s"),i+1,*OVCalibration::GetHullName(draft.Hulls[i].Entry.Hull)),i==play->SortieSelected);};
 auto* roster=pc->GetGameInstance()->GetSubsystem<UOVHeroSubsystem>();
 auto picture=[&](UVerticalBox* dest,UTexture* texture,float height){
   if(!texture)return;
   auto* box=WidgetTree->ConstructWidget<USizeBox>();box->SetHeightOverride(height);
   box->SetContent(fitImage(texture));dest->AddChild(box);
 };
 auto preview=[&](){
   auto* ship=play->InspectSortie(play->SortieSelected);
   if(ship){
     if(!SortieMirror)SortieMirror=NewObject<UOVShipPreview>(this);
     SortieMirror->Update(ship,UOVShipPreview::DefaultHullView(),1.1,1.8,INDEX_NONE,740);
     picture(mid,SortieMirror->GetTexture(),240);
   }
   return ship;
 };
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
   if(map==2)deploymentPlot(false);else picture(mid,ScenarioPhotos.FindRef(map==1?FName("BlackHoleTrial"):draft.Mission),220);label(mid,maps[map],19,true);label(mid,info[map]);
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
   if(draft.Hulls.IsEmpty())label(right,TEXT("왼쪽 함선 카드를 눌러 첫 함선을 추가하세요."));
 }else if(stage==2||stage==5){
   ships(left);
   auto* ship=play->InspectSortie(play->SortieSelected);
   if(ship&&ship->Systems->Layout){auto* layout=ship->Systems->Layout.Get();
    const auto& hull=draft.Hulls[play->SortieSelected];
    const bool exterior=stage==2 && play->bSortieExternal;
    if(stage==2){
      auto* tabs=WidgetTree->ConstructWidget<UHorizontalBox>();mid->AddChild(tabs);
      button(tabs,"ShowRooms",0,TEXT("내부 시설"),!exterior);
      button(tabs,"ShowWeapons",0,TEXT("외장 무장"),exterior);
      button(right,exterior?"RecommendWeapons":"RecommendRooms",0,exterior?TEXT("추천 무장 적용"):TEXT("추천 시설 적용"),false,true);
      button(left,"RecommendFleetDesign",0,TEXT("전 함선 추천 설계"));
    }
    if(!exterior){
      auto* decks=WidgetTree->ConstructWidget<UHorizontalBox>();mid->AddChild(decks);TSet<int> levels;
      for(const auto& room:layout->Slots)if(!levels.Contains(room.DeckId)){levels.Add(room.DeckId);button(decks,"Deck",room.DeckId,FString::Printf(TEXT("%d층"),room.DeckId),room.DeckId==play->SortieDeck);}
    }
    if(!SortieMirror)SortieMirror=NewObject<UOVShipPreview>(this);
    auto* board=CreateWidget<UOVSortieDesignBoard>(pc);board->Configure(this,ship,SortieMirror);
    auto* boardArea=WidgetTree->ConstructWidget<USizeBox>();
    boardArea->SetContent(board);mid->AddChildToVerticalBox(boardArea)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto part=[&](FName cmd,int32 value,const FString& name,const FString& details,int32 image){
      auto* card=CreateWidget<UOVSortiePartCard>(pc);card->Configure(this,board,cmd,value,hull.Key,name,details,image);
      right->AddChildToVerticalBox(card)->SetPadding(FMargin(0,1));
    };
    if(exterior){
      label(mid,TEXT("□ 일반      ◇ 전용      △ 요격"),14,true);
      label(mid,TEXT("휠 버튼을 누른 채 마우스를 움직여 회전합니다. 휠로 확대하세요."),13);
      if(layout->WeaponMounts.IsValidIndex(play->SortieMount)){
        const auto& slot=layout->WeaponMounts[play->SortieMount];
        const auto equipped=play->SortieWeaponAt(play->SortieMount);
        label(right,FString::Printf(TEXT("%d번 · %s · T%.0f"),play->SortieMount+1,slot.Type==EOVWeaponType::Interception?TEXT("요격 장착점"):slot.AuthoredSlotPurpose=="Dedicated"?TEXT("전용 장착점"):TEXT("일반 장착점"),slot.Tier),15,true);
        if(!equipped.IsNone())button(right,"ClearMount",0,TEXT("탈거 · ")+OVSortieParts::WeaponName(equipped,slot.IsAntiShipMissile()));
      }
      label(right,TEXT("무장을 선체의 장착점에 끌어 놓으세요."),13);
      TSet<FName> dedicated;
      for(const auto& mount:layout->WeaponMounts)if(mount.IsAntiShipMissile()&&!mount.RequiredProfileId.IsNone())dedicated.Add(mount.RequiredProfileId);
      const auto& profiles=pc->Combat->Settings->Weapons;
      auto weaponCard=[&](int i,bool strategic){
        const auto& w=profiles[i];
        const FString caption=OVSortieParts::WeaponName(w.ProfileId,strategic)+FString::Printf(TEXT(" T%d"),w.Tier);
        part(strategic?"EquipAntiShip":"Equip",i,caption,FString::Printf(TEXT("%s%.0f m · 재사용 %.1f초"),strategic?TEXT("전용 · "):TEXT(""),w.Range/100,w.Interval),OVSortieParts::WeaponIllustration(w.ProfileId,strategic));
      };
      for(int i=0;i<profiles.Num();++i)if(dedicated.Contains(profiles[i].ProfileId))weaponCard(i,true);
      for(int i=0;i<2;++i){const FName id=i?FName("PD_Kinetic"):FName("PD_Beam");part("EquipPD",i,OVSortieParts::WeaponName(id),TEXT("요격 전용 장착점"),OVSortieParts::WeaponIllustration(id));}
      for(int i=0;i<profiles.Num();++i)weaponCard(i,false);
    }else{
      if(stage==2){
        if(layout->Slots.IsValidIndex(play->SortieRoom)){
          const int32 placed=int32(play->SortieRoomAt(play->SortieRoom));
          label(right,FString::Printf(TEXT("%d층 %d번 · %s"),layout->Slots[play->SortieRoom].DeckId,play->SortieRoom+1,OVSortieParts::RoomName(placed)),15,true);
          if(placed)button(right,"ClearRoom",0,TEXT("선택 시설 비우기"));
        }
        label(right,TEXT("시설을 도면의 빈 방에 끌어 놓으세요."),13);
        const TCHAR* functions[]={TEXT(""),TEXT("함선 전체에 동력을 공급"),TEXT("항법과 지휘 명령 수행"),TEXT("승무원의 생존 유지"),TEXT("함선의 추진과 기동"),TEXT("대함 무장 사격 통제"),TEXT("함포 탄약 공급"),TEXT("빔 무장 전력 공급"),TEXT("미사일 발사에 필요한 탄체 공급"),TEXT("함선 쉴드 제어"),TEXT("함대 공유 탐지 제공"),TEXT("근접 방공 무장 통제"),TEXT("개인 강습 포드 발사"),TEXT("기체 보관과 출격"),TEXT("함재기 작전 지원"),TEXT("전장 보급 자원 보관"),TEXT("선내 침투 방어")};
        for(int i=1;i<=int(EOVFacilityType::Security);++i)part("SetRoom",i,OVSortieParts::RoomName(i),functions[i],i);
      }
      else{label(right,TEXT("탑승 인원"),17,true);for(int i=0;i<draft.Heroes.Num();++i){auto& h=draft.Heroes[i];auto* def=roster->Definition(h.Definition);if(def)button(right,"Person",i,def->Name+(h.HullKey?TEXT(" · 배치"):TEXT(" · 미배치")),i==play->SortieHero,false,def->Portrait);}
       if(draft.Heroes.IsValidIndex(play->SortieHero)){
        const auto& who=draft.Heroes[play->SortieHero];auto* host=draft.Hulls.FindByPredicate([&](const auto& h){return h.Key==who.HullKey;});
        if(host){label(right,TEXT("탑승 · ")+OVCalibration::GetHullName(host->Entry.Hull));if(host->Key==draft.Hulls[play->SortieSelected].Key){
         int where=layout->GetSlotIndex(who.StartRoom);if(layout->Slots.IsValidIndex(where))label(right,FString::Printf(TEXT("시작 · %d층 %s"),layout->Slots[where].DeckId,OVSortieParts::RoomName(int(play->SortieRoomAt(where)))));
         int post=layout->GetSlotIndex(who.DutyRoom);label(right,layout->Slots.IsValidIndex(post)?FString(TEXT("보직 · "))+OVSortieParts::RoomName(int(play->SortieRoomAt(post))):who.bBoard?TEXT("임무 · 강습"):TEXT("임무 · 수호"));}}
        button(right,"Embark",0,TEXT("이 방에서 시작"));button(right,"Duty",0,TEXT("이 시설 보직 배정"));button(right,"Board",0,TEXT("강습 편제 전환"));}}
    }}
 }else if(stage==3){
   ships(left);label(mid,TEXT("아군 시작 구역   →   적 방향"),16,true);
   deploymentPlot(true);
   label(mid,TEXT("왼쪽 구역을 클릭해 배치하세요. 흰 표식과 수직선이 선택 함선의 위치와 고도를 보여 줍니다."));button(right,"Auto",0,TEXT("자동 배치"));
   label(right,TEXT("함대 진형"),16,true);
   const TCHAR* presetNames[]={TEXT("균형 전열"),TEXT("항모 호위"),TEXT("수송 호위")};
   const FName presets[]={"FormationScreen","FormationCarrier","FormationConvoy"};
   for(int i=0;i<3;++i)button(right,"Formation",i,presetNames[i],draft.Formation==presets[i]);
   if(draft.Hulls.IsValidIndex(play->SortieSelected)){auto& h=draft.Hulls[play->SortieSelected];label(right,FString::Printf(TEXT("위치 %.0f, %.0fm\n고도 %.0fm · 방향 %.0f°"),h.Position.X/100,h.Position.Y/100,h.Position.Z/100,h.Yaw));}
   button(right,"Altitude",1,TEXT("고도 +25m"));button(right,"Altitude",-1,TEXT("고도 −25m"));button(right,"Yaw",1,TEXT("우현 45°"));button(right,"Yaw",-1,TEXT("좌현 45°"));
   if(map==1)label(right,TEXT("황색선 · 중력권\n적색선 · 완파 중심"));if(map==2)label(right,TEXT("모래색 원 · 소행성 실체"));
 }else if(stage==4){
   label(left,TEXT("시험 영웅 명단"),17,true);auto ids=play->SortieRoster();for(int i=0;i<ids.Num();++i){auto* h=roster->Definition(ids[i]);if(!h)continue;
    bool on=draft.Heroes.ContainsByPredicate([&](const auto& chosen){return chosen.Definition==h->Id;});button(left,"Hero",i,h->Name+FString::Printf(TEXT(" · %s · Lv.1"),heroRoles[int(h->Role)]),on,false,h->Portrait);}
   if(!draft.Heroes.IsEmpty()){auto* h=roster->Definition(draft.Heroes.Last().Definition);if(h){picture(mid,h->Portrait,300);label(mid,h->Name,18,true);label(mid,h->Signature);}}
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
 for(int i=0;i<3;++i)if(scrollOffsets[i]>0 && SortieColumns[i].IsValid())SortieColumns[i]->SetScrollOffset(scrollOffsets[i]);
 if(!play->SortieFeedback.IsEmpty())label(stack,play->SortieFeedback,15,true,OVStyle::AmberBadge);
 auto* bar=WidgetTree->ConstructWidget<UHorizontalBox>();stack->AddChildToVerticalBox(bar);button(bar,"Cancel",0,TEXT("준비 취소"));auto* spacer=WidgetTree->ConstructWidget<USpacer>();bar->AddChildToHorizontalBox(spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 if(stage>0)button(bar,"Prev",0,TEXT("이전"));button(bar,stage==6?"Start":"Next",0,stage==6?TEXT("전투 시작"):TEXT("다음"),false,true);
}
