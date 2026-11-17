# 모델 원본 반입 목록

2026-11-17 KST. 사용자 첨부 **무장 9종 + 함선 15종 + 함재기 1종 + 강습 포드 1종 + 미사일 탄체 2종**의 반입 대상과 저장 위치를 기록한다.

함재기는 초소형함 자산으로 별도 분류하고, 강습 포드는 함선과 다른 운송체로 관리한다. 강습선은 일반 소형함이다. 미사일 탄체는 발사대와 별도 분류하며, 대함/일반 미사일이라는 자산명만으로 유도 여부나 최종 무장 매핑을 확정하지 않는다.

## 현재 상태

**원본 ZIP은 아직 저장소에 없다.** 현재 다운로드 도구의 파일당 32 MiB 제한으로 28개 요청이 모두 거절됐다. 따라서 실제 크기·해시·압축 무결성·FBX/텍스처 구조·라이선스·메시 분리 가능성은 확인하지 않았다. manifest의 null은 미확인을 뜻한다. 파일명에서 추정한 무장 이름은 게임 데이터 매핑 확정이 아니다.

이 커밋은 목록과 LFS 규칙만 준비하며 모델 업로드나 UE 적용 완료를 뜻하지 않는다. 코드 브랜치와 work는 수정하지 않는다.

## 원본 저장 규칙

- [manifest.json](manifest.json)의 destination 경로에 원본 ZIP을 그대로 보관한다.
- 원본 ZIP은 Git LFS로 저장한다. 전역에 영향을 주지 않도록 git lfs install --local을 사용한다.
- 원본을 확보한 다음 크기·SHA-256·ZIP 무결성을 확인하고 manifest를 갱신한다.
- 추후 분리·피벗 수정본과 UE 에셋은 원본과 별도 경로에서 관리한다.
- 게임 명세의 함체 등록과 모델 파일 반입 여부를 구분한다.

## 로컬 PC에서 이어서 반입

원본 28개가 있는 PC의 별도 에셋 작업 폴더에서 이 브랜치를 체크아웃한다.
manifest에 지정한 경로로 복사하고 LFS 적용 여부를 확인한 뒤 커밋·푸시한다.
크기·해시·ZIP 검사까지 완료하기 전 source_status를 imported로 바꾸지 않는다.

```powershell
git fetch origin codex/model-assets
# 기존 변경을 보존하고 별도 워크트리에서 진행
git worktree add ../A1-model-assets -b codex/model-assets-local origin/codex/model-assets
Set-Location ../A1-model-assets
git lfs install --local
# manifest.json의 destination에 28개 원본 ZIP 복사
git add art/models/source
git lfs ls-files
# 위 목록에서 실제 추가한 ZIP이 LFS로 추적되는지 확인
git diff --cached --stat
git commit -m "Import original ship and weapon model archives"
git push origin HEAD:codex/model-assets
```

채팅을 통해 다시 전달하려면 각 전송 파일을 32 MiB 미만(권장 30 MiB 이하)으로 분할해야 한다. 분할 원본을 모두 받은 후 재조립·무결성 검증하고 원본 ZIP으로 반입한다.

## 이후 검사

1. 원본 파일 28종과 manifest 일치, SHA-256/ZIP 무결성.
2. FBX·텍스처 목록, 좌표축·단위·피벗·메시/재질 구성.
3. 무장의 고정 받침대·회전 포탑·고저각 포신 분리 가능성.
4. 함선별 장착점·출격구·레이더 가동부·충돌·스케일.
5. 대표 모델 UE 적용 후 나머지 확대.

