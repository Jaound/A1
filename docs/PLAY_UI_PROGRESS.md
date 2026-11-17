# 플레이 화면·영웅 카드·가동 포탑 작업

## 최신 상태 — 2026-11-17, 구현 중 / 새 검증 전

- 시작 코드 `f8f4d7a3106805d77040ce9091913cebf0d2f374`, 시작 미커밋 변경 없음. 작업 브랜치 `codex/phase1-playground`.
- work fetch 직후 기획 `ce06643f4029a6d60200406363e148a396f11234`를 `.git/ov-play-ui-planning-sha`에 고정하고 문서 변경을 충돌 없이 병합했다. 다른 fetch의 FETCH_HEAD를 기획 기준으로 사용하지 않는다.
- 실제 확인한 원형은 `Downloads/Play_Game_3.zip`, SHA256 `bc65912ae558c70ef163ee68e2d6ac767c16de25497cc4e38d197c64f8a80c25`. 기존 참조와 같은 버전이다. 후속 FBX 프로토타입 원본을 확인한 것으로 표현하지 않는다.
- MainGame의 상단/아웃라이너/지휘부, BoardingBlueprintPanel의 외부3D·내부·격납고·핀, ShipMeshFactory의 절차적 선체/무장 표시를 실제 읽었다. 원형의 경제/구형 게임 규칙을 옮기지 않는다.
- 원본 모델: `C:\Users\user\Documents\OrbitalVanguardAssets\Incoming` 31개 실제 ZIP, 기존 Source FBX38개. 원본 해시·압축 목록·FBX 경로는 외부 `Reports/PlayUI_SourceInventory.json`. 원본과 기존 Source를 덮어쓰지 않았다.

## 기획 차이와 보존

최신 work의 D119는 수송선을 중형으로 정했지만 실제 함체 수치/배치 재산정은 R37이다. D120은 항공순양함 격납고 설치 상한8개이며 실제 장착수/재고와 다르다. 첨부의 항모 상한4개 및 항공순양함 T4 주포2문은 이 기획 SHA에 확정 문구가 없다. 이번 표시 작업에서는 기존 후보 편성/화력/격납고를 임의 수정하지 않는다. 자료 차이를 숨기지 않고 후속 데이터 개정과 분리한다.

## 진행 파일과 다음 작업

1. 기본 HUD·함선 목록/게이지·선택 명령·접힌 장면 메뉴: `OVHUDPlay.cpp`, `OVHUDMission.cpp`, `OVHUD2C.cpp`.
2. 영웅 초상 카드/상태/실제 위치·선내 마커: `OVHeroWidget.*`, `OVPortraitWidget.*`, `OVHUDInterior.cpp`.
3. Insert 카드/간단 생성·고급 진단 분리: `OVDevelopmentWidget.*`, `OVCatalogCardWidget.*`.
4. 논리각·발사 ID를 읽는 가동 포탑과 실제 포구 일치: `OVWeaponVisualComponent.*`, `OVShip.*`, 전투/PD 및 자동 검사.
5. 실제 메시의 읽기 전용 3D 프리뷰: `OVShipPreview.*`, `OVHUD.*`. 전투 함선 Actor를 복제하지 않는다.

다음: 첫 Win64 Editor 빌드 → 관련 자동/카드·포탑 API → 크기별 렌더 확인·오류 수정 → 기존 회귀 → 포탑 영상·성능 비교 → 실행/검증 기록과 코드 브랜치 푸시. 현재 새 구현의 성공을 과거80개 자동/API 결과로 대신하지 않는다. 직접 OS 입력은 별도이며 이전 캡처 실패가 계속되면 한정 재시도 후 미검증으로 남긴다.

이번 범위에서 캠페인·경제·연구를 확장하지 않는다. 긴 코드명·해시·진단문단은 기본 플레이/생성 화면에서 숨기고 기능 API는 보존한다.
