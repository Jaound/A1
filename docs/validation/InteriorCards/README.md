# 선내 지도와 인원 카드 검증

2026-11-24. 개발 브랜치 `codex/phase1-playground`, 시작 코드 `d17679b`. Windows Development, DX11, ControlRange 영웅 전투 시험장.

| 확인 방법 | 실제 결과 |
| --- | --- |
| 빌드 | Win64 Development Editor 및 Windows Development 게임 빌드 성공. 최종 실행 파일과 Windows 패키지 내부 실행 파일의 SHA-256 일치 |
| 신규 PIE 검사 | [17개 통과](Result.json). 두 줄 카드로 34명 전원 접근, 팀별 12·12·10명, 드래그·휠·필터와 선택 범위, 같은 방의 분대원 선택, 개인 지휘, 교전 중 영웅과 UI 125%의 카드 클릭 영역 확인 |
| 기존 PIE 검사 | 영웅 시험장 54개, 두 팀 수송·진입·결산 63개 통과. 병렬 실행에서 선내 지도 첫 그리기가 두 프레임을 넘긴 사례가 있어 버튼이 그려질 때까지 제한 시간 내 대기하도록 검사 스크립트를 수정했다. 기존 입력 판정과 요구 조건을 유지했다. |
| 실제 마우스 | 1440×810 Windows 패키지에서 지도 열기·최대화, 슬라이더를 오른쪽 끝까지 드래그해 명단 첫 카드가 이리스에서 일반병 17로 바뀌는 것을 확인했다. A팀 필터에서 이리스를 클릭해 2층 전환과 왼쪽 아래 HP 300/300·상태·위치 표시를 확인했다. |
| 미검증 범위 | 패키지에서 Shift 다중 선택과 철수 완료까지의 수동 조작은 별도로 수행하지 않았다. |

[기본 창 교전 화면](Boarding.png) · [최대화 교전 화면](Expanded.png). 두 이미지는 실제 PIE 렌더에서 선내 창 부분을 잘라낸 것이다. 모함에 있는 준비 상태와 실제 적함 교전 상태를 따로 확인했다.

프로젝트 폴더에서 `Tools/ValidateInteriorCards.py`, `Tools/ValidateHeroArena.py`, `Tools/ValidateBoardingWaves.py`를 ControlRange 에디터 플레이로 실행한다. 예시는 [이전 시험장 검증의 재현 명령](../HeroFormation/README.md)에 있다. 신규 검사 원본 캡처와 결과는 `Saved/Validation/InteriorCards`, 빌드 로그는 `Saved/Logs/InteriorCardsBuild.log`, 패키지 로그는 `Saved/Logs/InteriorCardsRestage.log`에 있다.
