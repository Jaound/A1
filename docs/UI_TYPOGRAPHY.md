# UI 글꼴 사용 안내

전장 Canvas HUD와 타이틀·영웅·개발 창의 UMG/Slate가 같은 글꼴 설정을 사용한다. 게임 실행에는 Windows에 설치된 서체나 원본 ZIP이 필요하지 않다. 검증 기록과 원본 식별값은 [화면·검사 결과](validation/Typography/Results.json)와 [정적 TTF 해시](validation/Typography/FontSources.json)에 있다.

## 화면별 역할

| 역할 | 화면에서의 용도 | 원본 정적 파일 | UE Font / FontFace 접미사 | 기본 Slate 크기 |
|---|---|---|---|---:|
| Body | 한국어 설명, 서사, 대사, 긴 로그와 툴팁 | NanumSquareNeo-bRg.ttf | Body | 15 |
| Heading | 창 제목, 중요 상태 제목 | NanumSquareNeo-dEb.ttf | Heading | 19 |
| Button | 명령 버튼 | NanumSquareNeo-dEb.ttf | Heading | 14 |
| HeroName | 영웅 이름 | NanumSquareNeo-dEb.ttf | Heading | 17 |
| Numeric | HP·쉴드, 재사용 시간, 거리, 자원, 타이머, 경험치 | D2Coding-Ver1.4.0-20261003.ttf | Numeric | 14 |
| Logo | ORBITAL VANGUARD, 영문 함선 식별명 | Orbitron Bold.ttf | Logo | 32 |
| Comms | 진입·강습 관련 짧은 통신 경보 | Orbit-Regular.ttf | Comms | 14 |

콘텐츠 경로는 `/Game/UI/Fonts/Typography/Font_<접미사>`와 `Face_<접미사>`다. 기본 페이스에 없는 문자는 같은 프로젝트의 나눔스퀘어 네오 Regular를 참조한다. 예를 들어 Orbitron으로 출력하는 영문 표제에 한글을 붙여도 해당 음절을 표시한다. 전장과 상세 프리뷰는 기존 논리 좌표·버튼 높이·UI 배율을 유지하고 실제 서체로 폭을 측정한다. 긴 버튼 이름과 함선 이름에는 말줄임을 쓰고 영웅 툴팁은 360 논리 단위에서 줄을 바꾼다. 숫자는 하나의 문자열로 그린다. 화물량은 고정된 열의 오른쪽에 맞추고, 함선 거리와 고도는 미터, 재사용 시간은 초, 분·초 타이머는 두 자리 형식으로 표시한다.

## 변경 또는 재반입

1. 새 카드나 화면에서는 `Source/OrbitalVanguard/OVTypography.h`의 역할을 선택한다. UMG 텍스트는 `OVTypography::Style`, Canvas는 `OVTypography::Font`로 그리고 같은 폰트로 측정한다. 공통 기본 크기와 자산 연결은 `OVTypography.cpp`에 있다.
2. 기존 자산을 다시 만드는 경우 언리얼 에디터를 연다. PowerShell에서 해당 세션의 `$env:OV_FONT_SOURCE`를 원본 네 파일이 있는 폴더로 지정하고 그 세션에서 에디터를 시작한다. 에디터의 Python 콘솔에서 아래를 실행한다. 배포 대상 게임에서는 이 단계를 실행할 필요가 없다.

   ```python
   import os, runpy, unreal
   runpy.run_path(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "ImportFonts.py"))
   ```

3. 스크립트는 ZIP 내부의 지정된 정적 TTF와 Orbit 단독 파일만 읽는다. D2Coding 기본판을 사용한다. 새 배포본으로 교체할 때는 원본 해시·라이선스·포함 경로를 함께 확인하고 기존 자산 이름을 유지한다.
4. `Config/DefaultGame.ini`에서 위 글꼴 디렉터리를 쿠킹하고 `Content/ThirdPartyNotices/Fonts`를 읽을 수 있는 일반 파일로 패키지에 복사한다. Windows 배포 시 실행 파일과 함께 전체 패키지 폴더를 전달한다.

## 배포 고지

원본 파일 4개와 출처 안내가 배포본의 `OrbitalVanguard/Content/ThirdPartyNotices/Fonts`에 들어간다.

- 나눔스퀘어 네오 두 굵기는 네이버의 나눔글꼴 저작권 고지 및 한·영 라이선스 전문을 사용한다. [서체 배포](https://campaign.naver.com/nanumsquare_neo/) · [네이버 공식 고지](https://help.naver.com/service/30016/contents/18088?osType=PC)
- Orbitron은 사용자 배포 ZIP의 `orbitron-master/Open Font License.markdown` 원문, D2Coding은 해당 ZIP의 `OFL.txt` 원문을 동봉했다.
- Orbit는 Google Fonts 공식 배포 트리의 해당 서체 전용 고지를 동봉했다. [Orbit OFL 원문](https://github.com/google/fonts/blob/main/ofl/orbit/OFL.txt)

## 다시 검사하는 방법

- 에디터 자동화에서 `OrbitalVanguard.UI.Typography`를 실행한다. 기본·대체 페이스, 혼합 문자의 글리프와 숫자 폭을 검사한다.
- 프로젝트의 기존 `Tools/ValidateHeroArena.py`와 `Tools/ValidateInteriorCards.py`를 PIE에서 실행하면 영웅 원화·선내 카드·교전의 기존 동작을 검사한다. `Tools/CaptureTypographyPanels.py`는 일시정지·은하 지도·함선 상세·교전 장면을 렌더한다. 실행 결과는 `Saved/Validation`에 남는다.
- 패키지에서 `Tools/CaptureTypography.ps1 -Game <게임 exe> -OutputDir <캡처 폴더>`를 실행하면 1280×720과 1920×1080에서 각각 UI 배율 0.8·1.0·1.5를 검사한다. 게임 로그의 실제 뷰포트와 배율도 대조한다.
- 육안 비교용 [함대전 6개 조건](validation/Typography/Fleet.jpg) · [타이틀·설정·임무 선택·결과](validation/Typography/Menus.jpg) · [일시정지·지도·함선·교전·영웅](validation/Typography/Panels.jpg). 직접 플레이에서는 영웅 창에서 이리스 베인의 스킬에 포인터를 올리고, 함대전에서 화물·재사용 시간과 밝은 이펙트 위의 함명을 확인할 수 있다.


## 이번에 확인한 범위

Win64 Development Editor와 Windows Development 패키지가 성공했다. 글꼴 자동화 1건, 영웅 시험장 PIE 157개, 선내 카드 PIE 17개, 임무 결과 PIE 26개가 통과했다. 50코스트 함대전의 여섯 화면은 독립 패키지를 각각 실행해 촬영했다. PIE에서 밝은 빔·쉴드 피격 중 HUD, 이리스 베인의 이름과 서사·스킬 168자의 줄바꿈도 확인했다. 툴팁은 실제 위젯을 시험용 위치에 렌더했다. 마우스 호버로 열리는 위치와 모든 화면의 해상도별 조합은 직접 검증하지 않았다.

최종 패키지를 열어 Font 5개와 FontFace 5개를 찾았고, 별도로 추출한 5개 런타임 페이로드에서 원본 TTF 데이터의 SHA-256이 모두 일치했다. 맑은 고딕의 이전 쿠킹 산출물을 제외하기 위해 마지막 배포본은 전체 쿠킹으로 만들었다. 영웅 이미지 경로 102개, 라이선스 원문 네 개와 출처 파일도 대조했다. 에디터 기동 시 발생하는 기존 자체 진단 메시지는 검사 결과 JSON에 별도로 적었다.
