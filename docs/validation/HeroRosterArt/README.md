# 영웅 원화 34명 적용 검증

원본은 `codex/model-assets`의 `e9ac1995f0738c88087a37738a29b7cc65c50277`에 있는 [원화 목록](https://github.com/Jaound/A1/blob/e9ac1995f0738c88087a37738a29b7cc65c50277/art/characters/source/manifest.json)이다. 게임 명단의 정의 ID와 한국어 이름으로 34명을 대응시켰다. 전신 1086×1448과 SD 1254×1254 각 34장의 SHA-256, 크기, 해상도를 대조했다. 전신과 SD에서 머리·복장·장비가 같은 인물인지 34쌍 모두 육안으로 확인했다. [전체 대조표](Roster.jpg)

새로 반입한 33명은 명단용 얼굴 256×256, 전신 768×1024, 투명 배경 SD 512×512를 사용한다. 초상 위치와 SD 안쪽 배경 제거 위치는 캐릭터별로 기록했다. 이리스 베인(M02)은 두 원본이 기존 것과 바이트 단위로 일치해 기존 가공본 3개를 사용한다. 일반병은 기존 병과 표시를 사용한다.

| 2026-11-24 확인 항목 | 결과 |
| --- | --- |
| 빌드 | UE 5.8.4 Win64 Development Editor 성공. Windows Development 패키지 생성 성공 |
| 패키지 포함 여부 | 최종 Windows IoStore 컨테이너의 102개 경로가 34명 × 전신·초상·SD 목록과 일치 |
| 게임 내 연결 | 실제 영웅 정의 객체의 텍스처 경로 102개 일치. 영웅 시험장 [157개 통과](HeroArena.json). 34종 모두 실제 편성과 포드 진입 포함 |
| 선내 카드 | 필터·가로 스크롤·분대와 개인 선택·교전 중 클릭 영역 등 [17개 통과](InteriorCards.json) |
| 실제 렌더 | DX11 에디터 플레이에서 [표준 근거리 장교 명단 화면](Standing.jpg)과 [선내 교전 화면](Boarding.jpg)을 캡처해 확인. 새 원화 전체는 대조표와 게임 내 연결 검사로 확인 |
| 직접 입력 | 이번 검증은 게임 명령 호출과 화면 캡처로 진행. 34명을 마우스로 차례로 선택하는 검사는 미실시 |

## 재현

저장소에서 `git fetch origin codex/model-assets`를 실행해 원본 이력을 확보한다. 프로젝트 폴더에서 Pillow와 NumPy가 설치된 Python으로 `Tools/PrepareHeroRosterArt.py`를 실행한 뒤 에디터의 파이썬 명령으로 `Tools/ImportHeroRosterArt.py`를 실행한다. 준비 스크립트가 읽는 원본 커밋은 고정되어 있다.

프로젝트에서 실행한 대표 명령은 아래와 같다.

```powershell
$engine = 'E:\UE_5.8\Engine'
$project = (Resolve-Path .\OrbitalVanguard.uproject).Path
$editor = "$engine\Binaries\Win64\UnrealEditor-Cmd.exe"
& "$engine\Build\BatchFiles\Build.bat" OrbitalVanguardEditor Win64 Development "-Project=$project" -WaitMutex
foreach ($test in 'ValidateHeroArena','ValidateInteriorCards') {
    & $editor $project /Game/Maps/ControlRange -unattended -nosplash -d3d11 "-ExecCmds=py $PWD/Tools/$test.py"
}
```

빌드·반입·패키지 로그는 프로젝트의 `Saved/Logs/HeroRoster34Build.log`, `HeroRoster34ImportFinal.log`, `HeroRoster34Package.log`에 있다. 패키지 내역은 `Saved/HeroArt34/Cooked.csv`다. 첫 병렬 검사에서는 지도 확대 직후 클릭 영역을 찾지 못했다. 화면 저장 완료 및 지도 그리기를 기다리도록 검사 시점을 수정하고 같은 판정 조건으로 다시 통과했다.
