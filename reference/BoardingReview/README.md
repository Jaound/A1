# BoardingReview v0.4

[기획/결과](../../docs/BOARDING_REVIEW.md)를 먼저 읽는다. 전체 함대/UE 구현이 아닌 분리 진단이다.

```bash
python reference/BoardingReview/study.py
python reference/BoardingReview/scale.py
python reference/BoardingReview/linked.py
python reference/BoardingReview/report.py
python -m unittest discover -s reference/BoardingReview -p 'test_*.py' -v
```

`profile.json`은v0.3c를 계승하되 강습선의공격0/PD1·방어/기동 후보와 개인별 포드 정책을 반영한다. 초대형 함체/무장은 바꾸지 않았다. 프로필과코드/의존성 해시는 결과에 기록한다.

- `study.py`: 양측 경비8명 고정 영웅 수/편성/수비 경감 민감도3840회, 기존 조건에서 방어 영웅만 추가320회, 개인 포드960회, 강습선 대함 위협512회.
- `scale.py`: 원 편성/동일100코스트 배수 편성/초대형을 전함5척으로 교체한 동시드96회. 기존v0.3c 프로필을 고정해 원인 비교에 새 강습선 변경을 섞지 않는다.
- `linked.py`: 살아남은 개별 역할을 선내 전투로 잇는192회. 대표 생존자 수로 영웅 유형을 대체하지 않는다.

유효 진단은총5920회다. 격자3840은4편성의 조건부 표본을 합친 값이며 실제 플레이어 전체의 승률이 아니다. 영웅 수6은스트레스 조건이지 함선 정원 확정이 아니다. 수비 경감+10%p도 민감도 입력이며 확정 버프가 아니다.

포드의 발사/도착/요격은 개별ID와시각으로 계산하지만 실제3D 궤도·시설 경로·쉴드 복구·외부 포격/영웅 HP·나포/구조/귀환의 전체 전투를 재현하지 않는다. 강습선 운송은 적 쉴드가 열려있다는 전제에서 대함 무장만 스트레스 입력한다. PD는 강습선을 피해 대상으로 취급하지 않는다.

규칙/출력 검사10개: 개인별10포드·시각·역할/손실, 강습선PD면역/대함 위협, 공격0/PD1, 초대형/다른 함체 불변, 원시 결과 해시·표본수, 실제 생존 역할 연결, 같은 편성 배수 통제.
