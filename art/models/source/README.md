# 모델 원본 업로드 폴더

2026-11-17 KST. 사용자 첨부 **29종: 함선16 + 무장9 + 함재기1 + 강습포드1 + 미사일탄체2**의 목적지를 준비했다.

## 실제 반입 상태

원본 ZIP은 아직 없다. 29개 첨부 모두 이 클라우드 다운로드 도구의32 MiB 제한으로 반입하지 못했다. 이 문서와 manifest는 목록이며 실제 파일 검사·UE 적용 완료가 아니다. 원본이 있는 Windows PC에서 아래 방법으로 업로드한다.

## 폴더 구조

    art/models/source/
      ships/               함선16종: corvette, frigate, vector, bastion, cruiser,
                           air_defense, radar, battleship, carrier, missile_ship,
                           dreadnought, battlecarrier, repair, mining, transport, assault
      weapons/             기존 무장9종
      weapons/anti_ship_missile_launcher/  대함 미사일 발사 모듈(추가 업로드 위치)
      aircraft/fighter/    함재기
      transport_devices/boarding_pod/  개인 강습포드
      projectiles/anti_ship_missile/   대함 미사일 탄체
      projectiles/standard_missile/    일반 미사일 탄체

기존 29종 모델의 실제 원본 파일명과 경로는 [manifest.json](manifest.json)에 있다. 대함 미사일 발사 모듈은 새 업로드 위치만 준비했으며 원본 파일명은 아직 manifest에 등록하지 않았다. 이 파일은 아래 도우미 스크립트 대신 해당 폴더에 직접 복사한다. Git은 빈 폴더를 보존하지 않으므로 각 폴더의 .gitkeep으로 경로를 만들었다. 원본 ZIP은 이름을 바꾸거나 풀지 않고 보관한다.

## 권장: Windows Git + Git LFS로 업로드

웹 브라우저 업로드는 파일당25 MiB 제한이 있고 LFS 반입 절차를 대신하지 못한다. 이 원본들은32 MiB보다 크므로 로컬 Git LFS를 사용한다. LFS는 큰 파일의 본체를 Git 이력과 별도로 저장하며 GitHub 계정/플랜의 파일 크기·저장·전송 한도가 적용된다.

1. Windows PowerShell에서 git --version과 git lfs version을 확인한다. 명령이 없다면 Git for Windows/Git LFS를 먼저 설치한다.
2. 원본 ZIP29개를 한 폴더에 모은다. 예: C:\Users\user\Downloads\OV-Models. manifest의 원래 파일명을 유지한다.
3. 기존 UE 작업 폴더와 다른 새 경로로 아래 브랜치를 복제한다. 아래 대상 폴더가 이미 있으면 비어 있는 다른 경로를 사용한다.

    Set-Location C:\Users\user
    git clone --branch codex/model-assets --single-branch https://github.com/Jaound/A1.git A1-model-assets-upload
    Set-Location .\A1-model-assets-upload
    git lfs install --local
    & .\art\models\source\Import-ModelArchives.ps1 -SourceDirectory 'C:\Users\user\Downloads\OV-Models'
    git status --short
    git add art/models/source
    git lfs ls-files
    git diff --cached --stat

4. git lfs ls-files에 추가한 ZIP이 표시되는지 확인한다. 29개를 모두 넣었다면29개가 보여야 한다.
5. 목록을 확인하고 커밋·푸시한다. 최초 인증 요청이 나타나면 자신의 GitHub 계정으로 로그인한다.

    git commit -m "Import 29 original model archives"
    git push origin codex/model-assets
    git status --short

도우미 스크립트는 원본명으로 파일을 찾아 지정된 하위 폴더에 복사하고, 복사 전후 SHA-256을 비교해 manifest의 크기·해시·반입 상태를 기록한다. 누락 파일은 나열하고 실제 찾은 파일만 처리한다. 이미 목적지에 다른 내용의 파일이 있으면 덮어쓰지 않고 중단한다. 스크립트는 커밋·푸시·UE 임포트를 하지 않는다. ZIP CRC와 메시/텍스처 검사는 별도로 해야 하며 inspection_status는 그대로 유지한다.

PowerShell 실행 정책 때문에 스크립트가 차단되면 전역 정책은 바꾸지 않고 다음 한 번의 프로세스로 실행할 수 있다.

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\art\models\source\Import-ModelArchives.ps1 -SourceDirectory 'C:\Users\user\Downloads\OV-Models'

## 직접 복사하는 방법

도우미 대신 manifest의 destination 경로에 해당 ZIP을 복사한 후 같은 git add / lfs ls-files / commit / push 순서로 업로드해도 된다. 이때 manifest의 크기·해시·반입 상태는 별도로 갱신해야 한다.

## 후속 검사와 변경 범위

- 원본 보관, ZIP 무결성 확인, 메시/재질 검사, 부품 분리, UE 적용은 각각 별도 단계다.
- 모델 이름만으로 무장 성능·미사일 유도 여부를 확정하지 않는다.
- 격납고·함선 기본 편성은 에셋 반입으로 바뀌지 않는다.
- 게임 코드·work·기지 디자인 브랜치는 수정하지 않는다.
- 도우미 스크립트의 실제 Windows 실행은 이 클라우드에서 미검증이다.

