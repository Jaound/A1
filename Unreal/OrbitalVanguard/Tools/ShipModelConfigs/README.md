# 원본 선체 준비 설정

이 설정은 `Documents/OrbitalVanguardAssets/Source`의 로컬 원본 FBX를 `PrepareDestroyerFullMeshes.py`로 읽어 전체 형상과 원본 해상도 맵을 별도의 `Prepared/<tag>`에 복사한다. 원본은 쓰지 않는다. 이미 출력 폴더가 있으면 덮어쓰지 않고 중단하므로, 재시험은 설정 사본의 `tag`를 바꿔 새 폴더에 실행한다. 백터·바스티온은 스크립트의 기본 설정이다. 초계함은 별도 `PreparePC14FullMesh.py`를 사용한다.

`OV_FULL_SHIP_CONFIG`에 설정 파일의 절대 경로를 넣고 Blender 백그라운드에서 준비 스크립트를 실행한다. 이어 `OV_FULL_SHIP_TAG`에 해당 `tag`를 넣어 UE 에디터의 Python에서 `ImportDestroyerFullMeshes.py`를 실행한다. 선택적으로 `OV_FULL_SHIP_ID`를 설정하면 해당 함종만 반입한다. 이미 있는 UE 에셋은 덮어쓰지 않는다. 예전 지원함 일괄 반입 종료 오류의 원인은 미확정이며 개별 실행의 정상 종료는 오류 해결을 뜻하지 않는다.

이 과정은 지오메트리·질감 반입일 뿐 원본 무장의 가동 분리, 포탑·발진구·접안점 접합, LOD, 패키징, 최종 아트 승인이나 직접 조작 검증을 뜻하지 않는다. 새 에셋은 Git LFS 규칙과 저장 용량을 확인한다.
