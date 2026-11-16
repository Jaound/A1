# Play Game 3 조작 참조 자료

사용자가 제공한 `Play_Game_3.zip`에서 카메라와 이동 입력만 분석·추출한 자료다. UE 프로젝트에 그대로 컴파일하는 소스가 아니다. 다른 게임 시스템·에디터 캐시·실행 배치 파일은 포함하지 않았다.

- [RtsCameraRig.gd](RtsCameraRig.gd): 원본 카메라 파일 전체, 변경 없이 보관.
- [SpaceViewport_input.gd.txt](SpaceViewport_input.gd.txt): 원본 `SpaceViewport.gd` 184~351행 발췌. 의존 클래스와 상위 상태가 생략돼 있어 독립 실행 스크립트가 아니다.
- [source_profile.json](source_profile.json): 소스에서 추출한 수치와 UE 이식 시 미확인 항목. UE 런타임 설정 파일로 해석하지 않는다.
- [provenance.json](provenance.json): ZIP·관련 소스·2A 보고 문서의 SHA-256과 원래 경로.
- [camera_probe.gd](camera_probe.gd): 추출한 원본 프로젝트 안의 카메라를 이벤트/API로 검사하는 별도 실행 도구.
- [camera_probe_result.json](camera_probe_result.json): Godot 4.6.3 headless의 실제 검사 결과. 기본 12항목 성공, Alt-LMB 해제 문제 별도 재현, 1600×900의 실제 투영 각도 측정.

## 검사 재현

원본 ZIP을 별도 폴더에 풀고 Godot 4.6.3이 있는 환경에서 수행한다. 아래는 명령 형태의 예시이며 사용자 경로에 맞춘다.

```text
godot --headless --editor --import --path "<원본 Play Game 폴더>" --quit
godot --headless --path "<원본 Play Game 폴더>" --script "<저장소>/reference/PlayGame3Controls/camera_probe.gd"
```

원본 프로젝트의 GameManager와 카메라를 사용한다. 실행 결과의 `CAMERA_PROBE_JSON`에 일반 검사와 관찰된 경계 문제가 따로 기록된다. `passed=12`는 원본 전체 게임·OS 마우스 입력·UE5 이식이 성공했다는 뜻이 아니다. 프로젝트 원래 엔진 표기는 Godot 4.3이고 이번 검사는 설치된 4.6.3으로 수행했다.

이 자료를 실제 작업에 적용할 때는 [조작 이식안](../../docs/05_references/PLAY_GAME_3_CONTROLS.md)과 [2A 검토](../../docs/archive/analysis/PHASE2A_REVIEW.md)를 읽는다.
