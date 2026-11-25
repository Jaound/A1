# 실습과 함대전 출격 준비 수정

## 실행

타이틀에서 **시스템 실습**을 누르면 장을 고를 수 있다. 목표 카드는 화면 왼쪽 가장자리에 있다. **접기**로 전장을 넓게 보고 **힌트**로 다시 펼친다. 함선 상세의 청록 게이지는 선체, 파란 게이지는 쉴드이며 각각 현재값과 최대값을 표시한다.

- 전장에서 숫자 **1·2·3**으로 해당 속도로 진행하고 **Space**로 정지·재개한다. F2·F3·F4도 사용할 수 있다. 부대 1~3 등록은 Ctrl+숫자, 호출은 Alt+숫자다.
- 선내 지도에서 방을 선택하면 지도 아래에 해당 방의 역할, 점령 및 공작의 효과가 표시된다. 적 함선 이름에는 붉은 글씨와 ‘적’ 표시가 붙는다.
- **작전 개시 → 코스트 함대전 → 작전 출격**에서 빈 함대에 함선을 추가한다. 추천 편성은 직접 불러온다. 설계 화면에서 층을 선택하고 시설 카드를 방으로 끌어 놓는다. 외장 무장은 회전 가능한 함체 위에서 해당 장착점에 놓는다. 각 단계에서 편성 오류를 고친 뒤 출격한다. 전투 시작 시 정지되어 있다.



## 조작과 판정

| 화면 | 조작과 확인할 결과 |
|---|---|
| 1~3장 | 휠 버튼을 누른 채 마우스를 움직여 회전한다. 함선 상세에서 선체와 쉴드의 이름 및 현재값/최대값을 읽는다. 발사 목표는 해당 버튼 외곽 전체로 표시한다. |
| 4·5장 | 전함 곁에서 적 항모의 함재기를 방공함이 실제로 요격한다. 초계함 전방 전개, 방공 호위, 레이더 정찰, 전함 일제 사격, 항모 출격, 구축전함 지휘를 연습한다. 함종 카드를 펼쳐 ‘직접 조종’을 누르면 다른 함종도 직접 다뤄 볼 수 있다. 이미 나온 함선은 다시 선택하며 없는 함선은 후방의 빈 위치에 배치한다. |
| 6장 | 실제 선내도에서 16종의 시설을 선택하고 가동·점령·파괴 효과를 읽는다. 호위함의 함포, 레이더함의 빔, 전략미사일함의 미사일 발사와 호환·티어 제한을 확인한다. |
| 7장 | 격납고의 보관 기체를 선택해 중앙에서 출격시킨다. 왼쪽 아래 항모 명령창으로 귀환, 두 번째 출격, 표적 공격과 제공전을 진행한다. 주변 수리함에서 보급받은 일반 35·고급 5가 기체 1대분이다. 손실 보충과 자동 재개는 실제 재고·기체 상태로 확인한다. |
| 8·9장 | 영웅 명단에서 해당 영웅의 위치를 찾고 함교 수호 도착과 보직 복귀를 확인한다. 강습은 A팀 전체를 선택하고 이동 수단을 고른 뒤 적함을 지정한다. 철수한 강습선은 구축전함의 격납고에서 ‘소형함 수납’으로 직접 회수한다. |
| 함선 설계 | 내부 시설의 방·층·연결을 보고 카드를 빈 방으로 끈다. 외장 무장은 휠 버튼을 누른 채 돌려 장착면을 찾고 휠로 확대한다. 일반·요격·전용 장착점은 글자와 모양·색으로 구별한다. 전략미사일함의 전용 모듈은 일반탄·유도탄을 구분한다. |
| 함대 배치 | 균형 전열·항모 호위·수송 호위 프리셋을 선택한 뒤 개별 함선의 위치·고도·방향을 바꿀 수 있다. 균형 전열의 초계함은 가장 앞에 선다. |

## 검증 환경과 재현

UE 5.8, Win64 Development Editor 및 Windows Development 패키지. 기준 개발 커밋 `b6d138f`, 기획 `d07deb5`, 주요 구현 `36bf102`, 함종 자유 조종과 입력 검증 `c526339`. 절대경로는 아래 재현 예시에서 SSD의 실제 위치다. 빌드 머신 이외에는 배포된 Windows 폴더 전체를 사용한다.

```powershell
$project = 'I:/2026_2/GDD/A1/Unreal/OrbitalVanguard/OrbitalVanguard.uproject'
$engine = 'E:/UE_5.8/Engine'
& "$engine/Build/BatchFiles/Build.bat" OrbitalVanguardEditor Win64 Development "-Project=$project" -WaitMutex
& "$engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project -unattended -NullRHI "-ExecCmds=Automation RunTests OrbitalVanguard" "-TestExit=Automation Test Queue Empty" "-ReportExportPath=I:/2026_2/GDD/RevisionAutomationShipRoleReport"
```

실제 GPU로 에디터의 ControlRange 맵을 열고 프로젝트의 `Tools/ValidatePractice.py`, `Tools/ValidateSortie.py`, `Tools/ValidateBaseStage4.py`를 각각 Python으로 실행한다. 각 스크립트가 독립 PIE를 시작·종료한다. Slate 포인터 회귀 검사는 실제 그래픽 출력이 가능한 별도 에디터에서 `Automation RunTests OrbitalVanguard.Sortie.DesignBoard.SlatePointer`로 실행한다.

## 패키지에서 직접 조작한 내용

- **1280×720, 일반 우주**. 타이틀에서 실습에 들어가 숫자 1·2·3과 Space를 눌렀다. 다시 임무 목록을 열어 빈 50코스트 편성에 초계함과 방공함을 넣고 시설과 빔 요격포를 드래그했다. 추천 설계, 항모 호위 진형, 이리스 베인 배치, 최종 출격, 0초 일시정지, 전장에서 적 함명 색상까지 확인했다. [키 입력 기록](Keyboard.txt) · [시설 장착](DragEngine.jpg) · [빔 요격포 장착](DragPD.jpg) · [영웅 초상](HeroAspect.jpg) · [출격 정지](PausedSortie.jpg) · [적함 상세](EnemyDetails.jpg)
- **1920×1080, 블랙홀**. 초계함과 전략미사일함을 선택하고 시설을 선내 도면에, 전용 무장을 함체 장착점에 끌어 놓았다. 영웅을 탑승시키지 않고 26 대 50코스트로 출격해 브리핑의 편성과 0초 정지를 확인했다. [시설 배치](Engine1920.jpg) · [전용 장착점](StrategicMount1920.jpg) · [최종 브리핑](BlackHoleBriefing1920.jpg) · [전장](BlackHoleLaunch1920.jpg). 이 검사 뒤 선택 편성과 관계없이 승리 조건이 정확하게 표시되도록 함대전 목표 문구를 수정했다
- **최종 배포본**. 타이틀 → 시스템 실습 → 5장 → 함종 카드에서 호위함을 직접 선택해 추가했다. 함선 명령창과 선택 함대에 같은 호위함 08이 표시되고 카메라가 해당 함선으로 이동했다. [최종 패키지 직접 조종](RoleControlPackage.jpg)
- 9개 장 전체의 완료와 12가지 예산·환경 조합은 PIE 검사로 확인했다. 모든 장과 조합을 Windows 패키지에서 수동으로 반복하지는 않았다. 패키지의 마우스 드래그는 카드 장착에서 직접 확인했다.

## 관련 소스와 회귀 자료

- 실습 시나리오·오버레이는 `OVPractice.cpp`와 `OVHUDPractice.cpp`, 실제 키 입력과 속도는 `OVController.cpp`·`DefaultInput.ini`에 있다.
- 출격 준비는 `OVSortie.cpp`·`OVSortieMenu.inl`, 드래그 도면·실제 무장 모델 카드는 `OVSortieDesignWidget.cpp`·`Content/UI/MissionCards/Weapons`, 장착점 투영은 `OVShipPreview.cpp`를 쓴다.
- [5장 함종 카드와 직접 조종](pilot-card.png) · [선체·쉴드](ship-gauges.png) · [시설 선택 실습](facility-lesson.png) · [강습팀 선택](assault-team.png) · [빈 함선 도면](SortieEmptyDeck.png) · [추천 설계 후 도면](SortieFittedDeck.png) · [외장 무장 설계](SortieWeapons.png)

## 검증 결과

| 검사 | 결과 |
|---|---|
| Win64 Development Editor | 빌드 성공. 컴파일 로그 `../PracticeRevisionEditorVerified.log` |
| 실습 PIE | 9장 [162/162](PracticePIE.json). 16종 직접 조종·재선택, 수송선 이동과 정지, 장 복귀 후 유닛 정리, 영웅의 실제 수호 도착, 12인 강습팀의 점령·철수와 강습선 직접 수납 포함 |
| 출격 PIE | 4개 예산 × 3개 환경 [657/657](SortiePIE.json). 배치 제한, 실제 전투 함선 장비·영웅·전용 무장·빔/기관 요격포, 재시작 일시정지, 환경 초기화 포함 |
| 기지 회귀 PIE | [110/110](BasePIE.json). 건조·출격·복귀·수리·개조·저장·복원 경로 포함 |
| 설계 도면 포인터 PIE | DX11에서 [1/1](Pointer.json). 실제 도면 위젯의 위치 판정에 중간 버튼 누름·이동·해제와 휠 이벤트를 보냈다. 첫 소켓의 정규화 좌표는 (0.761, 0.636) → 회전 후 (0.554, 0.756) → 확대 후 (0.558, 0.778). 해제 후의 이동은 회전시키지 않았다. 과거 시험함 배치 3건과 선택적 타이틀 배경 참조 1건의 경고가 남았다. |
| C++ 자동 검사 | [170/170](Automation.json). 경고 없이 120, 경고 포함 50, 실패 0. 그래픽 출력을 사용하는 포인터 검사는 위의 별도 DX11 실행에 포함된다. |
| Windows Development 패키지 | UE 5.8 BuildCookRun의 빌드·쿡·스테이징·아카이브 성공. 코드 `c526339`, UAT 로그 `../PracticeRevisionPlayPackage.log`, 실행 파일 `I:/2026_2/GDD/OrbitalVanguardTests/PracticeRevisionPlay/Windows/OrbitalVanguard.exe`. |
