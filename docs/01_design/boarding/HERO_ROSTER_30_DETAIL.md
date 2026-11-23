# 영웅 로스터 34인 상세 기획 (고유 30인 + 기본 일반·적군 4인 · UE 5.8 C++ 실구현 검증 완료)

- 기준 원본: `docs/01_design/boarding/HERO_SYSTEMS.md` (v0.5 기준안 · D114 (세력 전체 지휘 코스트), D115 (영웅 정체성·원천), D116 (4유형·스킬·성장·장비))
- 고유 영웅 구성 (30명): 근거리 10 · 원거리 10 · 지원 5 · 지휘 5
- 기본(일반·적군 공용) 영웅 (4명): 근거리(G01) · 원거리(G02) · 지원(G03) · 지휘(G04) — 고유 패시브·시그니처 없음
- 전장 축 분포 (고유 30인): 함대전 특화 13명 · 함대전/선내 양면 6명 · 선내 강습/수호 11명
- C++ 실구현 검증: `codex/phase1-playground` (`OVInteriorSubsystem`, `OVCombatSubsystem`, `OVPointDefenseSubsystem`, `OVCarrierSubsystem`, `OVShipSystemsComponent`, `OVShip`, `OVTransportSubsystem`) 실제 구현 변수 100% 정렬 (미구현 화재 확산·쉴드 개시 지연·PD 표적 전환 지연·함선 회피율·선내 경직 전량 제거).
- 핵심 가드레일: 영웅 1명 = 지휘 코스트 1 (세력 한도 3/6/10/15, 로스터 30명) · 경감 상한 65% · 장비+연구 상한 40% · 명중 합산 +8%p(빔 98%) · 연사 +30% · 이속/선회 +20% · 부활/무자원 회복/순간이동/개인 대함 공격 금지

## 1. 4대 유형 기준 스탯 요약 (L1 → L20)

| 유형 | L1 HP | L1 공격 (DPS) | 사거리 | 이동 | 경감 / 회피 | L20 배율 (HP / 공격) | 함선 보직 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 근거리형 | 300 | 25 (25.0) | 1.8m | 3.6m/s | 18% / 8% | 2.14x / 2.52x | 보직 없음 (선내 수호·엄호 전담) |
| 원거리형 | 240 | 28 (23.3) | 18m | 3m/s | 10% / 12% | 2.14x / 2.52x | 무기제어실 (Weapon Control) |
| 지원형 | 240 | 12 (8.6) | 11m | 3m/s | 8% / 10% | 2.14x / 2.52x | 정비 노드 (Engineering / Repair Node) |
| 지휘형 | 280 | 22 (17.6) | 10m | 3.1m/s | 15% / 10% | 2.14x / 2.52x | 함교 (Bridge) |

## 2. 기본 영웅 4인 — 일반 영입 및 적군 영웅 공용 (고유 패시브·시그니처 없음)

| 코드 | 명칭 | 유형 | 원천 | 보직 | 고유 패시브 | 고유 시그니처 | 용도 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `G01` | 표준 근거리 장교 | 근거리형 | 강화인간 | 보직 없음 (표준 강습·수호 전열) | 없음 | 없음 | 일반 영입 · 적군 공용 |
| `G02` | 표준 원거리 사수 | 원거리형 | 강화인간 | 무기제어실 (표준 명중 +2~4%p · 연사 +15~25%) | 없음 | 없음 | 일반 영입 · 적군 공용 |
| `G03` | 표준 지원 정비관 | 지원형 | 초능력자 | 정비 노드 (표준 재난 -15~30% · 선체 복구 0.5~1.0%) | 없음 | 없음 | 일반 영입 · 적군 공용 |
| `G04` | 표준 함교 지휘관 | 지휘형 | 초능력자 | 함교 (표준 속도/선회 +5~9% · 급선회 +10~20%) | 없음 | 없음 | 일반 영입 · 적군 공용 |

### [G01] 표준 근거리 장교 — 기본 근거리형 (Standard Melee Officer)
- **분류**: 기본(일반·적군 공용) · 근거리형 · **보직**: 보직 없음 (표준 강습·수호 전열)
- **고유 패시브 / 시그니처**: 없음 (v0.5 공용 4스킬 및 표준 보직 규칙만 사용)
- **운용 설명**: 함대 아카데미 정규 근접전 과정을 수료한 표준 근거리 영웅. 일반 영입 풀 및 적군 함대 강습·수호 지휘관의 기준 템플릿으로 사용된다.
- **권장 빌드**: 역장 방어 R3 · 역장 엄호 R2 · 강화 돌입 R2 · 생존 한계 해방 R1 / 강화 외피 + 이동 보조
- **참조 화풍**: 강화인간 · 남성 기준

### [G02] 표준 원거리 사수 — 기본 원거리형 (Standard Ranged Officer)
- **분류**: 기본(일반·적군 공용) · 원거리형 · **보직**: 무기제어실 (표준 명중 +2~4%p · 연사 +15~25%)
- **고유 패시브 / 시그니처**: 없음 (v0.5 공용 4스킬 및 표준 보직 규칙만 사용)
- **운용 설명**: 함대 사격 통제 및 선내 중거리 교전 교범을 이수한 표준 원거리 영웅. 일반 영입 풀 및 적군 주력함 무기제어실 보직 영웅의 기준 모델이다.
- **권장 빌드**: 초감각 조준 R3 · 약점 감지 R2 · 전자기 제압 R2 · 인지 가속 R1 / 초점 장비 + 이동 보조
- **참조 화풍**: 강화인간 · 여성 기준

### [G03] 표준 지원 정비관 — 기본 지원형 (Standard Support Officer)
- **분류**: 기본(일반·적군 공용) · 지원형 · **보직**: 정비 노드 (표준 재난 -15~30% · 선체 복구 0.5~1.0%)
- **고유 패시브 / 시그니처**: 없음 (v0.5 공용 4스킬 및 표준 보직 규칙만 사용)
- **운용 설명**: 생체 촉매 의무 처치와 정비 노드 응급 복구 과정을 수료한 표준 지원 영웅. 일반 영입 풀 및 적군 함대 지원 영웅의 기준 템플릿이다.
- **권장 빌드**: 생체 재구성 R3 · 생존장 전개 R2 · 재생 공명장 R2 · 촉매 동조 R1 / 강화 외피 + 생체 회복 보조
- **참조 화풍**: 초능력자 · 여성 기준

### [G04] 표준 함교 지휘관 — 기본 지휘형 (Standard Command Officer)
- **분류**: 기본(일반·적군 공용) · 지휘형 · **보직**: 함교 (표준 속도/선회 +5~9% · 급선회 +10~20%)
- **고유 패시브 / 시그니처**: 없음 (v0.5 공용 4스킬 및 표준 보직 규칙만 사용)
- **운용 설명**: 함교 조타 지휘 및 분대 신경 공명 표준 교리를 수료한 일반 지휘 영웅. 일반 영입 풀 및 적군 기함 함교 보직 지휘관의 기준 모델이다.
- **권장 빌드**: 신경 공명 R3 · 신경 접속 R2 · 전투 동조 R2 · 집단 기동 예지 R1 / 강화 외피 + 점령 보조
- **참조 화풍**: 초능력자 · 남성 기준


## 3. 고유 영웅 상세 기획 시트 (30명 — C++ 실구현 검증 완료)

### [M01] 카엘 반 — 격벽 돌파자 (Breach Vanguard)
- **분류**: 근거리형 · 강화인간 (남성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 1티어 · 보직 없음 (1차 강습 포드 선봉)
- **함대전 역할/보직 특화**: 외부 함대전 보직 없음 — 적 주력함 강습 돌파 성공률을 극대화하는 순수 선봉형.
- **배경 서사**: 구 제국 중장갑 공병대 출신. 척추 보조 외골격과 고밀도 충격 역장 코어를 이식받아 좁은 함선 복도 돌파에 특화되었다.
- **성격 / 화법**: 과묵하고 실리적. 아군 분대원의 생환을 최우선으로 둔다.
- **고유 패시브 (선봉 격벽 압박)**: 강습 포드 착륙 직후 6초간 역장 엄호 반경이 3m → 4.5m로 확장 (경감률 상한 65% 준수).
- **시그니처 스킬 (충격파 감속 돌파)**: 강화 돌입 착탄 시 2m 내 적 일반 병력 2.5초간 이동속도 -25% 감속 (영웅 대상 1.2초, OVInteriorSubsystem SlowUntil/SlowFraction 연동).
- **권장 스킬·장비 빌드**: 역장 방어 R3 · 역장 엄호 R3 · 강화 돌입 R2 / 강화 외피 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: v0.5 경감 상한 65% 준수. 미구현 경직(Stun) 대신 실구현 SlowUntil/SlowFraction 감속으로 정렬.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 카엘 반 (격벽 돌파자 (Breach Vanguard)), an elite male orbital fleet hero.
Appearance & Outfit: rugged short dark-brown hair, sharp determined jaw with faint cheek scar, heavy charcoal breach-plate chest armor with crimson inner waist coat and bulky cyan-lit cybernetic left arm. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a heavy kinetic breach-shield integrated onto the cybernetic left forearm and a short high-frequency thermal baton in the right hand, braced vanguard stance. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 카엘 반]
Convert the character "카엘 반" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (rugged short dark-brown hair, sharp determined jaw with faint cheek scar, heavy charcoal breach-plate chest armor with crimson inner waist coat and bulky cyan-lit cybernetic left arm).
Action Pose: charging forward behind a glowing cyan kinetic forearm shield while swinging a compact thermal shock baton. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M02] 이리스 베인 — 위상 검객 (Phase Duelist)
- **분류**: 근거리형 · 복합형 (여성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 2티어 · 보직 없음 (적 방어 영웅 제거 특화)
- **함대전 역할/보직 특화**: 외부 함대전 보직 없음 — 적 기함 내부 중장갑 수호 영웅을 직접 제거하는 대영웅 결투자.
- **배경 서사**: 사이버네틱 근섬유와 초능력 공명 결정 검을 결합한 결투자. 중장갑 수호 영웅이 지키는 함교 앞 방어선을 절단한다.
- **성격 / 화법**: 자신감 있고 날카로운 승부사 기질.
- **고유 패시브 (고경감 파쇄)**: 피해 경감이 20% 이상인 적 대상 근접 공격 시 선내 피해 경감 8%p 무시.
- **시그니처 스킬 (위상 잔영 베기)**: 역장 방어(4초) 종료 시 축적된 방어 에너지를 전방 2.5m 호형으로 방출해 기본 공격 1.4배 피해.
- **권장 스킬·장비 빌드**: 역장 방어 R3 · 강화 돌입 R3 · 생존 한계 해방 R2 / 초점 장비 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 원거리 약점 감지(8~16%p)와 겹치지 않도록 고경감(20%+) 대상 조건부 근접 파쇄로 분리.
- **참조 화풍**: 복합형 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 이리스 베인 (위상 검객 (Phase Duelist)), an elite female orbital fleet hero.
Appearance & Outfit: flowing silver-lilac high ponytail, luminous violet eyes, holographic purple halo rings, sleek white-and-violet longcoat with silver-magenta cybernetic right arm and left leg. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: gripping a crystalline violet psionic energy blade downward in the cybernetic right hand while orbiting purple shards hover above the left palm. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 이리스 베인]
Convert the character "이리스 베인" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (flowing silver-lilac high ponytail, luminous violet eyes, holographic purple halo rings, sleek white-and-violet longcoat with silver-magenta cybernetic right arm and left leg).
Action Pose: lunging in a sleek fencing slash with a bright glowing violet crystal sword trailing magenta sparks. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M03] 도리안 크루거 — 격실 방벽 (Bulkhead Sentinel)
- **분류**: 근거리형 · 강화인간 (남성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 수호
- **영입 티어 / 보직**: 아카데미 1티어 · 보직 없음 (아군 주력함 내부 격실 수호)
- **함대전 역할/보직 특화**: 외부 함대전 보직 없음 — 적의 나포 강습으로부터 아군 항모·전함을 지키는 수비 전담.
- **배경 서사**: 함내 보안대 총교관 출신의 베테랑. 좁은 복도와 격벽 출입구에서 적 강습팀의 진입을 원천 봉쇄하는 데 능숙하다.
- **성격 / 화법**: 묵직하고 규율을 중시하는 교관 어조.
- **고유 패시브 (방어 거점 저지선)**: 아군 함선 내부에서 방어(Defend) 명령 대기 중 같은 슬롯에 진입한 적 강습원에게 2초간 이동속도 -20% 감속 부여 (SlowUntil/SlowFraction).
- **시그니처 스킬 (강철 봉쇄선)**: 역장 방어 발동 중 같은 슬롯 아군 일반 분대원이 받는 선내 피해 추가 -6% (Cover 보정, 경감 상한 65% 준수).
- **권장 스킬·장비 빌드**: 역장 방어 R3 · 역장 엄호 R3 · 생존 한계 해방 R2 / 강화 외피 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 미구현 방 크기(1/1·2/2) 조건 제거 → 실제 C++ Order==Defend 및 동일 슬롯 판정으로 정렬.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 도리안 크루거 (격실 방벽 (Bulkhead Sentinel)), an elite male orbital fleet hero.
Appearance & Outfit: salt-and-pepper cropped hair, square weathered face with cyan ocular strip across left temple, broad matte-gunmetal security exo-plate with amber-and-cyan hazard trim. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: planting a heavy collapsible riot tower shield on the deck with both reinforced cybernetic gauntlets, immovable guardian posture. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 도리안 크루거]
Convert the character "도리안 크루거" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (salt-and-pepper cropped hair, square weathered face with cyan ocular strip across left temple, broad matte-gunmetal security exo-plate with amber-and-cyan hazard trim).
Action Pose: bracing firmly behind a wide gunmetal riot shield with a glowing cyan barrier edge, blocking incoming fire. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M04] 세린 카이 — 유성 기동수 (Meteor Skirmisher)
- **분류**: 근거리형 · 초능력자 (여성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 2티어 · 보직 없음 (적함 후열 지원·원거리 교란)
- **함대전 역할/보직 특화**: 외부 함대전 보직 없음 — 적함 내부 후열 지원형·원거리형을 끊어내는 암살형 기동수.
- **배경 서사**: 국소 중력 왜곡 초능력으로 복도를 미끄러지듯 돌파하는 고속 근접 요원. 적의 후열 지원형과 원거리형을 우선 타격한다.
- **성격 / 화법**: 경쾌하고 속도감 있는 말투. 느린 지시를 싫어한다.
- **고유 패시브 (가속 회피 흐름)**: 이동속도 버프(강화 돌입·장비·오라) 적용 중 개인 회피율 +6%p 상승.
- **시그니처 스킬 (유성 연쇄 돌입)**: 강화 돌입(6~8m) 경로상에 스치는 적 최대 2명에게 기본 공격 0.65배 추가 타격.
- **권장 스킬·장비 빌드**: 강화 돌입 R3 · 역장 방어 R2 · 생존 한계 해방 R3 / 초점 장비 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 순간이동 금지 원칙 준수. 실제 이동 경로 충돌 판정으로만 다중 타격 발생.
- **참조 화풍**: 초능력자 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 세린 카이 (유성 기동수 (Meteor Skirmisher)), an elite female orbital fleet hero.
Appearance & Outfit: layered midnight-violet bob cut with glowing cyan hair clips, slender agile silhouette in a cropped white-and-violet naval jacket, obsidian stealth leggings with glowing violet boot thrusters. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: poised lightly on the balls of her feet holding twin curved violet psionic tonfa blades, swirling lavender energy ribbons around ankles. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 세린 카이]
Convert the character "세린 카이" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (layered midnight-violet bob cut with glowing cyan hair clips, slender agile silhouette in a cropped white-and-violet naval jacket, obsidian stealth leggings with glowing violet boot thrusters).
Action Pose: dashing forward at high speed with twin glowing violet tonfa blades and starlight motion trails. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M05] 볼칸 드라그 — 설비 파쇄공 (Reactor Breaker)
- **분류**: 근거리형 · 강화인간 (남성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 3티어 · 보직 없음 (적함 원자로·엔진 파괴 공작)
- **함대전 역할/보직 특화**: 간접 함대전 기여 — 적 전함의 주포/엔진을 파괴 공작으로 정지시켜 아군 함대 교전을 유리하게 전환.
- **배경 서사**: 중공업 해체선 출신 사이보그 공작원. 적함 나포 대신 원자로·무기제어실을 빠르게 파괴 공작해 적 주력함을 무력화한다.
- **성격 / 화법**: 호탕하고 직설적. 함선 구조를 약점 도면처럼 읽는다.
- **고유 패시브 (교전 중 공작 유지)**: 시설 파괴 공작(30초) 수행 중 근접 교전이 벌어져도 공작 게이지가 50% 속도로 유지 (최종 단축 50% 상한 준수).
- **시그니처 스킬 (열압 파쇄타)**: 적 설비 방어·보직 중인 적 영웅 및 승조원에게 근접 공격 피해 +12%, 공작 체크포인트(1/3·2/3) 도달 시 같은 슬롯 적 2.5초간 이동속도 -25% 감속.
- **권장 스킬·장비 빌드**: 역장 방어 R3 · 강화 돌입 R2 · 역장 엄호 R3 / 강화 외피 T4 + 점령/공작 보조
- **밸런스 · C++ 연동 메모**: 미구현 경직 대신 실구현 SlowFraction(-25%)과 Sabotage Checkpoint(1/3·2/3)에 직접 연동. 대함 소총 피해 금지.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 볼칸 드라그 (설비 파쇄공 (Reactor Breaker)), an elite male orbital fleet hero.
Appearance & Outfit: spiky iron-grey hair, rugged industrial beard, heavy welded charcoal armor with heat-vented copper-orange and cyan hydraulic piston arms, demolition satchel across chest. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: resting a heavy plasma-heated hydraulic breaching hammer across his right shoulder, left cybernetic fist venting white steam. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 볼칸 드라그]
Convert the character "볼칸 드라그" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (spiky iron-grey hair, rugged industrial beard, heavy welded charcoal armor with heat-vented copper-orange and cyan hydraulic piston arms, demolition satchel across chest).
Action Pose: heaving a glowing cyan-and-orange hydraulic breaching hammer overhead for a heavy smash. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M06] 라일라 녹스 — 역강습 추적자 (Counter-Boarder)
- **분류**: 근거리형 · 초능력자 (여성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 수호
- **영입 티어 / 보직**: 아카데미 3티어 · 보직 없음 (함내 신속 대응·포드 요격)
- **함대전 역할/보직 특화**: 외부 함대전 보직 없음 — 적 강습 포드 착륙 직후 초동 제압에 특화된 방어 기동대.
- **배경 서사**: 적 강습 포드의 선체 충격파를 감응해 즉각 현장에 도착하는 신속 대응 초능력자. 적의 선제 강습 템포를 무너뜨린다.
- **성격 / 화법**: 침착하고 경계심이 높다. 침입 경보에 즉각 반응한다.
- **고유 패시브 (침입 감응 급행)**: 아군 함선에 적 강습 포드가 착륙하면 8초간 해당 격실 방향 이동속도 +12% (총 이속 버프 20% 상한 준수).
- **시그니처 스킬 (염동 반발장)**: 역장 방어 발동 시 반경 2.5m 내 적 원거리 사격의 명중률 -8%p (3초간, 별도 쉴드 게이지 미생성).
- **권장 스킬·장비 빌드**: 역장 방어 R3 · 강화 돌입 R3 · 역장 엄호 R2 / 강화 외피 T3 + 이동 보조
- **밸런스 · C++ 연동 메모**: 미공개 적함 투시 금지 원칙 준수. 아군 함선 내부 침입 경보에만 반응.
- **참조 화풍**: 초능력자 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 라일라 녹스 (역강습 추적자 (Counter-Boarder)), an elite female orbital fleet hero.
Appearance & Outfit: long wavy obsidian-indigo hair tied back with a silver star clasp, luminous lavender eyes, crisp white-and-violet tactical longcoat with dark armored gauntlets and orbiting psionic barrier shards. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: holding a sleek psionic halberd-spear diagonally across her body while lavender telekinetic rings shimmer around her left palm. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 라일라 녹스]
Convert the character "라일라 녹스" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long wavy obsidian-indigo hair tied back with a silver star clasp, luminous lavender eyes, crisp white-and-violet tactical longcoat with dark armored gauntlets and orbiting psionic barrier shards).
Action Pose: spinning a glowing violet psionic spear in a defensive sweep surrounded by small floating crystal shards. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M07] 가렌 호크 — 기관실 수호자 (Core Warden)
- **분류**: 근거리형 · 강화인간 (남성) · **주력 전장**: 함대전·선내 양면 · **주 임무**: 수호
- **영입 티어 / 보직**: 아카데미 2티어 · 엔진실·원자로 구역 선내 상주 수호
- **함대전 역할/보직 특화**: 함대전 생존 보조 — 피격 파손이 잦은 주력함 엔진실·코어의 재난 피해를 억제하며 적 강습까지 동시 차단.
- **배경 서사**: 기관실과 코어 구역의 파괴 구역 재난 복구 및 방어를 겸하던 중장갑 함내 방재 요원. 시설이 피격 파손되어 재난 피해가 터지는 상황에서도 끝까지 버틴다.
- **성격 / 화법**: 우직하고 믿음직한 베테랑 기관병 스타일.
- **고유 패시브 (재난 격실 통제)**: 아군 함선 엔진실(Engine)·코어(Core) 슬롯 상주 시 해당 슬롯 파괴 구역 재난(Disasters, 20초) 지속시간 -25%(15초) 및 본인 재난 틱 피해 -25% (진공·완파 면역 불가).
- **시그니처 스킬 (위급 엄호 방벽)**: 역장 방어(4초) 발동 중 같은 슬롯의 HP 50% 이하 아군에게 적용되는 역장 엄호(Cover) 피해 경감 효과를 1.5배로 증폭 (총 경감 상한 65% 준수).
- **권장 스킬·장비 빌드**: 역장 방어 R3 · 역장 엄호 R3 · 생존 한계 해방 R2 / 강화 외피 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 미구현 화재 확산·스킬 대리 피격 제거 → 실구현 OVInteriorSubsystem Disasters(20초, 2 HP/s) 및 Cover 경감으로 정렬.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 가렌 호크 (기관실 수호자 (Core Warden)), an elite male orbital fleet hero.
Appearance & Outfit: short cropped chestnut hair, thick brow, reinforced slate-black thermal-resistant armor coat with glowing cyan coolant tubes along the collar and heavy cybernetic shoulders. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a compact thermal-shield buckler on the left arm and a pressurized cryo-axe in the right hand, steady protective stance. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 가렌 호크]
Convert the character "가렌 호크" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (short cropped chestnut hair, thick brow, reinforced slate-black thermal-resistant armor coat with glowing cyan coolant tubes along the collar and heavy cybernetic shoulders).
Action Pose: holding up a cyan-edged thermal buckler with one hand while swinging a compact cryo-axe. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M08] 미라 솔라리스 — 임계 결투자 (Apex Duelist)
- **분류**: 근거리형 · 복합형 (여성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 4티어 · 보직 없음 (후반 최정예 강습 에이스)
- **함대전 역할/보직 특화**: 외부 함대전 보직 없음 — 적 기함 함교를 지키는 고레벨 적 영웅을 단독 제압하는 최종 티어 결투자.
- **배경 서사**: 초능력 과부하 억제기와 사이버네틱 심장 보조기를 동시에 장착한 고위험 결투자. 위기 순간에 폭발적인 반격 화력을 낸다.
- **성격 / 화법**: 서늘하고 단호함. 한계에 몰릴수록 침착해진다.
- **고유 패시브 (임계 반격 가속)**: 생존 한계 해방(L15, HP<25%) 발동 시 4초간 자신의 공격속도 +15% 및 공격력 +10% (경감 상한 65% 준수).
- **시그니처 스킬 (결투장 고립선)**: 단일 적 영웅을 지정해 5초간 교전 시, 대상이 받는 외부 아군 오라 효율을 50% 약화.
- **권장 스킬·장비 빌드**: 생존 한계 해방 R3 · 역장 방어 R3 · 강화 돌입 R2 / 초점 장비 T4 + 강화 외피 T4
- **밸런스 · C++ 연동 메모**: 부활 및 불사 효과 없음. HP<25% 진입 시 외상 1단계 위험을 감수하는 하이리스크 설계.
- **참조 화풍**: 복합형 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 미라 솔라리스 (임계 결투자 (Apex Duelist)), an elite female orbital fleet hero.
Appearance & Outfit: long silver-white hair with dark magenta underlayer, intense violet eyes, regal white-and-violet high-collar naval coat with sleek dark cybernetic arms and glowing magenta spine stabilizer. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: wielding a long double-edged crimson-violet psionic glaive held effortlessly across her shoulders, floating purple crystal halo behind her. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 미라 솔라리스]
Convert the character "미라 솔라리스" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long silver-white hair with dark magenta underlayer, intense violet eyes, regal white-and-violet high-collar naval coat with sleek dark cybernetic arms and glowing magenta spine stabilizer).
Action Pose: poised in a fierce low stance swinging a glowing magenta-violet psionic glaive with crackling energy. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M09] 타릭 오르 — 함교 친위대장 (Bridge Protector)
- **분류**: 근거리형 · 초능력자 (남성) · **주력 전장**: 함대전·선내 양면 · **주 임무**: 수호
- **영입 티어 / 보직**: 아카데미 3티어 · 함교·무기제어실 보직 영웅 전담 호위
- **함대전 역할/보직 특화**: 함대전 간접 핵심 — 적 강습으로 인한 아군 기함의 무기제어/함교 버프 공백을 최소화.
- **배경 서사**: 기함 함교와 무기제어실의 핵심 보직 영웅을 전담 경호하는 친위 초능력자. 적 강습팀이 보직실에 들이닥쳐도 함선 지휘가 끊기지 않게 지킨다.
- **성격 / 화법**: 격식 있고 충성스러운 기사단 어조.
- **고유 패시브 (보직자 밀착 경호)**: 같은 방에 함선 보직 중인 아군 영웅(원거리/지원/지휘)이 있으면 그 보직 영웅이 받는 선내 피해 -10% (경감 상한 65%).
- **시그니처 스킬 (염동 지휘 유지막)**: 보직 아군 영웅이 선내 교전으로 자리를 비웠다가 복귀할 때 3초 인계 지연을 1초로 단축하고 역장 엄호 적용.
- **권장 스킬·장비 빌드**: 역장 엄호 R3 · 역장 방어 R3 · 생존 한계 해방 R2 / 강화 외피 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 함체 상주 한도(대형 4/초대형 6) 내에서 보직 영웅 + 친위 근접 영웅 조합의 전략적 가치를 창출.
- **참조 화풍**: 초능력자 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 남성 기준).
Subject: 타릭 오르 (함교 친위대장 (Bridge Protector)), an elite male orbital fleet hero.
Appearance & Outfit: neatly swept jet-black hair with subtle violet sheen, aristocratic stoic expression, pristine white-and-royal-purple naval dress coat with gold star epaulettes and glowing violet forearm barrier emitters. Incorporate signature faction design cues: white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards.
Equipment & Pose: standing upright with a ceremonial violet psionic energy saber drawn in the right hand and a translucent hexagonal violet ward floating over his left hand. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 타릭 오르]
Convert the character "타릭 오르" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (neatly swept jet-black hair with subtle violet sheen, aristocratic stoic expression, pristine white-and-royal-purple naval dress coat with gold star epaulettes and glowing violet forearm barrier emitters).
Action Pose: guarding upright with a bright violet psionic saber raised and a hexagonal purple ward glowing in front. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M10] 헤라 크림슨 — 나포 선봉장 (Capture Raider)
- **분류**: 근거리형 · 복합형 (여성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 점령
- **영입 티어 / 보직**: 아카데미 4티어 · 보직 없음 (적함 함교 신속 나포팀 리더)
- **함대전 역할/보직 특화**: 전략적 함대전 전환 — 적 대형함 완파 대신 온전한 나포를 노리는 고위험 강습 편대의 핵심.
- **배경 서사**: 적 방어선을 뚫고 즉각 점령 작전으로 전환하는 공격적 나포 전문가. 강습선 12인 편대의 돌파 속도를 극한으로 끌어올린다.
- **성격 / 화법**: 대담하고 카리스마 넘치는 돌격 지휘 말투.
- **고유 패시브 (돌파 후 점령 가속)**: 적 영웅을 무력화하거나 분대를 전멸시킨 직후 10초간 같은 방 아군의 시설/함교 점령 속도 +12% (최종 단축 50% 상한).
- **시그니처 스킬 (혈로 연쇄 돌입)**: 강화 돌입 사용 후 5초 내 적 전투원을 처치하면 강화 돌입 쿨타임 6초 단축 (1회 한정).
- **권장 스킬·장비 빌드**: 강화 돌입 R3 · 역장 방어 R3 · 역장 엄호 R2 / 초점 장비 T4 + 점령 보조
- **밸런스 · C++ 연동 메모**: 지휘형 신경 접속과 합산되어도 점령 시간 단축 상한 50%를 절대 초과하지 않음.
- **참조 화풍**: 복합형 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 헤라 크림슨 (나포 선봉장 (Capture Raider)), an elite female orbital fleet hero.
Appearance & Outfit: sweeping silver-ash hair with crimson-violet hair ribbon, fierce amber-violet eyes, sharp white-and-violet assault coat with blackened cybernetic right arm and left leg plating. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: holding a plasma-violet boarding cutlass pointed forward in her cybernetic right hand, swirling violet energy sparks trailing from the blade. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 헤라 크림슨]
Convert the character "헤라 크림슨" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (sweeping silver-ash hair with crimson-violet hair ribbon, fierce amber-violet eyes, sharp white-and-violet assault coat with blackened cybernetic right arm and left leg plating).
Action Pose: leaping forward pointing a glowing magenta-violet boarding cutlass ahead in a daring raid pose. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R01] 세라 린 — 궤도 사수 (Orbital Deadeye)
- **분류**: 원거리형 · 초능력자 (여성) · **주력 전장**: 함대전 특화 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 1티어 · 무기제어실 (순양함·전함 주포 초탄 정밀 사격)
- **함대전 역할/보직 특화**: 함대전 주포 초탄 명중률 극대화 — 교전 초반 적 쉴드 조기 붕괴를 노리는 함대 사격 통제관.
- **배경 서사**: 탄도 예측 초감각을 지닌 함대 사격 통제관. 교전 개시와 동시에 적 기함의 약점에 주포 초탄을 정확히 꽂아 넣는다.
- **성격 / 화법**: 냉철하고 정밀한 숫자 중심 화법.
- **고유 패시브 (초탄 탄도 동기화)**: 무기제어실 보직 시 교전 개시 첫 8초간 탑승 함선 주포 명중 분산 -12% 및 명중 +2%p 추가 보정 (영웅 합산 +8%p·빔 98% 상한).
- **시그니처 스킬 (고정밀 점사)**: 선내 전투에서 인지 가속(4초) 완료 대상에게 전자기 제압 적중 시 감속 지속시간 +1.5초.
- **권장 스킬·장비 빌드**: 초감각 조준 R3 · 약점 감지 R3 · 전자기 제압 R2 / 초점 장비 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 무기제어실 영웅 합산 명중 +8%p, 빔 98% 상한 엄격 준수. 개인 대함 직접 공격 금지.
- **참조 화풍**: 초능력자 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 세라 린 (궤도 사수 (Orbital Deadeye)), an elite female orbital fleet hero.
Appearance & Outfit: long sleek midnight-indigo hair pinned with a cyan targeting ornament, calm piercing violet eyes, white-and-violet naval officer longcoat with gold epaulettes and translucent violet trajectory rings around her visor. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: cradling a sleek long-barreled psionic rail-carbine with glowing cyan-violet coils in both hands, poised ballistic stance. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 세라 린]
Convert the character "세라 린" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long sleek midnight-indigo hair pinned with a cyan targeting ornament, calm piercing violet eyes, white-and-violet naval officer longcoat with gold epaulettes and translucent violet trajectory rings around her visor).
Action Pose: aiming down a glowing cyan-and-violet sci-fi sniper rail-carbine with a tiny purple targeting reticle floating near her eye. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R02] 마커스 케인 — 방공망 통제관 (Aegis Gunner)
- **분류**: 원거리형 · 강화인간 (남성) · **주력 전장**: 함대전 특화 · **주 임무**: 수호
- **영입 티어 / 보직**: 아카데미 1티어 · 무기제어실 (방공함·구축함 PD 대공 방어)
- **함대전 역할/보직 특화**: 함대전 대함재기 핵심 — 방공함 1~3척의 항모 함재기 편대 저지 능력을 실구현 PD Interval·Accuracy 기반으로 강화.
- **배경 서사**: 대함재기 방공전 전문가. 신경 가속 사격 통제 임플란트로 방공함의 점방어(PD) 포탑이 적 함재기 편대를 쉼 없이 추적하도록 조율한다.
- **성격 / 화법**: 간결하고 빠른 교신 어조. 공중 위협 경보를 즉시 전파한다.
- **고유 패시브 (PD 연사 주기 최적화)**: 무기제어실 보직 시 탑승 함선 점방어(PD) 포탑의 발사 간격(FOVPDProfile.Interval) -10% 단축 및 함재기·미사일 요격 명중(Accuracy) +2%p (영웅 합산 +8%p 상한).
- **시그니처 스킬 (대공 탄막 동기화)**: L10 보직모드(8초) 발동 시 주포 연사 버프와 함께 탑승 함선 PD 포탑의 쿨타임 회복 속도를 +15% 추가 가속 (총 연사 +30% 상한 준수).
- **권장 스킬·장비 빌드**: 초감각 조준 R3 · 전자기 제압 R3 · 약점 감지 R2 / 초점 장비 T3 + 강화 외피 T3
- **밸런스 · C++ 연동 메모**: 미구현 PD 표적 전환 지연(C++ 주석: instantaneous acquisition) 제거 → 실구현 OVPointDefenseSubsystem Interval·Accuracy로 정렬.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 마커스 케인 (방공망 통제관 (Aegis Gunner)), an elite male orbital fleet hero.
Appearance & Outfit: short tousled dark-brown hair, cybernetic optical scanner mounted over left eye, dark gunmetal tactical jacket with crimson lining, cybernetic left forearm linked to a holo-radar wrist display. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a compact rapid-fire pulse PD carbine with glowing cyan barrel vents in his right hand, tracking targets calmly. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 마커스 케인]
Convert the character "마커스 케인" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (short tousled dark-brown hair, cybernetic optical scanner mounted over left eye, dark gunmetal tactical jacket with crimson lining, cybernetic left forearm linked to a holo-radar wrist display).
Action Pose: firing rapid bursts from a compact cyan-lit pulse carbine while a tiny holographic radar ring glows beside his visor. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R03] 엘레나 보스 — 장거리 포격수 (Longbow Marksman)
- **분류**: 원거리형 · 복합형 (여성) · **주력 전장**: 함대전 특화 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 2티어 · 무기제어실 (전함·순양함 장거리 빔/레일건)
- **함대전 역할/보직 특화**: 함대전 장거리 포격전 특화 — 전함·장거리 순양함의 원거리 화력 교환비를 우위로 이끄는 포격 에이스.
- **배경 서사**: 사이버네틱 시신경과 초능력 공간 감각을 결합한 초장거리 포격 전문가. 사거리 끝자락에서 적 주력함을 정밀 타격한다.
- **성격 / 화법**: 여유롭고 도도한 엘리트 사격수 말투.
- **고유 패시브 (원거리 락온 보정)**: 무기제어실 보직 시 최대 사거리 70% 이상 원거리 표적에 대한 주포 명중 +3%p (합산 +8%p·빔 98% 상한 준수).
- **시그니처 스킬 (지속 조준 연사)**: 보직 중 동일 대형·초대형 적함을 10초 이상 지속 사격 시 탑승 함선 주포 연사 +6% (총 연사 +30% 상한 준수).
- **권장 스킬·장비 빌드**: 초감각 조준 R3 · 인지 가속 R3 · 전자기 제압 R2 / 초점 장비 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: R01(초반 8초 초탄)과 대비되는 장기 지속 사격형. 상한(+8%p 명중, +30% 연사) 내 설계.
- **참조 화풍**: 복합형 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 엘레나 보스 (장거리 포격수 (Longbow Marksman)), an elite female orbital fleet hero.
Appearance & Outfit: sleek silver-violet half-up hair, cool amethyst eyes with a glowing cyan cyber-reticle lens on her left cheek, white-and-violet longcoat with chrome-violet cybernetic right arm. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: standing tall with an elongated precision energy lance-rifle resting vertically beside her, orbiting violet prism rings along the barrel. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 엘레나 보스]
Convert the character "엘레나 보스" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (sleek silver-violet half-up hair, cool amethyst eyes with a glowing cyan cyber-reticle lens on her left cheek, white-and-violet longcoat with chrome-violet cybernetic right arm).
Action Pose: kneeling slightly to fire a long glowing violet-and-cyan precision beam rifle with crisp recoil sparks. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R04] 제이드 하퍼 — 복도 저격수 (Corridor Marksman)
- **분류**: 원거리형 · 강화인간 (남성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 수호
- **영입 티어 / 보직**: 아카데미 2티어 · 무기제어실 겸 선내 대형방·복도 저격 수호
- **함대전 역할/보직 특화**: 선내 수호 중심 — 일반 무기제어실 보직을 수행하다 적 강습 시 즉시 복도 장거리 저격으로 전환.
- **배경 서사**: 함내 긴 복도와 대형 격실의 사선 통제에 특화된 정밀 사수. 적 공작원이 설비에 접근하기 전에 원거리에서 발을 묶는다.
- **성격 / 화법**: 느긋하지만 사격 순간엔 냉정하게 집중한다.
- **고유 패시브 (장사선 확보)**: 선내 전투에서 기본 사거리 18m → 20m 확장, 12m 이상 거리에서 사격 시 명중 보정 +3%p.
- **시그니처 스킬 (공작 저지 제압탄)**: 전자기 제압(3연사) 적중 시 대상이 진행 중인 점령·파괴 공작을 2.5초간 중단(체크포인트는 유지).
- **권장 스킬·장비 빌드**: 전자기 제압 R3 · 초감각 조준 R3 · 약점 감지 R2 / 초점 장비 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 선내 전투 특화 원거리형으로 함대전 전용 영웅(R01~R03)과 명확히 구분.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 제이드 하퍼 (복도 저격수 (Corridor Marksman)), an elite male orbital fleet hero.
Appearance & Outfit: messy dark-teal hair, sharp slate eyes, charcoal-and-white tactical reconnaissance coat with cyan knee armor and cybernetic aiming brace on right forearm. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: resting a bullpup electromagnetic marksman rifle across his chest, fingers poised near the cyan trigger guard. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 제이드 하퍼]
Convert the character "제이드 하퍼" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (messy dark-teal hair, sharp slate eyes, charcoal-and-white tactical reconnaissance coat with cyan knee armor and cybernetic aiming brace on right forearm).
Action Pose: crouching in a sharp sniper stance firing an electromagnetic bullpup rifle with bright blue muzzle flash. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R05] 노아 발렌틴 — 일제사격 지휘사수 (Salvo Coordinator)
- **분류**: 원거리형 · 초능력자 (남성) · **주력 전장**: 함대전 특화 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 3티어 · 무기제어실 (미사일·어뢰 구축함/순양함)
- **함대전 역할/보직 특화**: 함대전 미사일 화력 극대화 — 미사일 구축함·순양함의 유도탄 물리 명중률과 일제사격 결정력을 강화.
- **배경 서사**: 다중 유도탄 궤적을 동시에 연산하는 초능력 사격 통제관. 적 방공망을 뚫는 미사일·어뢰 일제사격 타이밍을 설계한다.
- **성격 / 화법**: 우아하고 계산적. 탄막을 악보처럼 묘사한다.
- **고유 패시브 (유도탄 발사 주기 동기화)**: 무기제어실 보직 시 탑승 함선의 미사일 발사관(EOVWeaponType::Missile) 재장전 간격(Mount.Profile.Interval) -8% 단축 (총 연사 버프 +30% 상한 준수).
- **시그니처 스킬 (유도 궤적 선회 보정)**: L10 보직모드(8초) 동안 탑승 함선 유도 미사일의 추적 선회율(Weapon.GuidanceTurnRate) +25% 및 비빔 분산각(AimSpreadDegrees) -15% 감소.
- **권장 스킬·장비 빌드**: 전자기 제압 R3 · 초감각 조준 R3 · 인지 가속 R2 / 초점 장비 T3 + 강화 외피 T3
- **밸런스 · C++ 연동 메모**: 미구현 적함 기동 회피 확률 판정 제거 → 실구현 OVCombatSubsystem GuidanceTurnRate·AimSpreadDegrees·Interval에 직접 연동.
- **참조 화풍**: 초능력자 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 남성 기준).
Subject: 노아 발렌틴 (일제사격 지휘사수 (Salvo Coordinator)), an elite male orbital fleet hero.
Appearance & Outfit: wavy dark-indigo hair parted neatly, luminous violet eyes, white-and-purple officer longcoat with gold braided epaulettes and holographic purple missile trajectory arcs floating beside his raised hand. Incorporate signature faction design cues: white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards.
Equipment & Pose: holding a sleek psionic target-designator pistol in his right hand while guiding glowing violet lock-on crystals with his left. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 노아 발렌틴]
Convert the character "노아 발렌틴" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (wavy dark-indigo hair parted neatly, luminous violet eyes, white-and-purple officer longcoat with gold braided epaulettes and holographic purple missile trajectory arcs floating beside his raised hand).
Action Pose: pointing a glowing violet designator pistol forward as tiny purple lock-on rings orbit around him. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R06] 클라라 벡 — 후열 관통 사수 (Breach Marksman)
- **분류**: 원거리형 · 강화인간 (여성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 2티어 · 보직 겸 적함 강습 후열 화력 지원
- **함대전 역할/보직 특화**: 선내 강습 화력 전담 — 강습팀 12인 편대에서 아군 근접 영웅과 합을 맞춰 적 수호 영웅을 빠르게 제거.
- **배경 서사**: 강습팀 후열에서 아군 근접 영웅 너머로 적 지휘관과 지원형을 정확히 관통 사격하는 강습 전문 사수.
- **성격 / 화법**: 단도직입적이고 실전적. 아군 전열과 호흡을 맞춘다.
- **고유 패시브 (전열 연계 집중 사격)**: 선내 강습 중 같은 슬롯에서 아군 근접 영웅이 교전 중인 적 표적(EnemyId 일치)에게 사격 시 최종 피해 +10%.
- **시그니처 스킬 (고속 철갑 초사격)**: 약점 감지(경감 8~16%p 무시) 적용 상태에서 초감각 조준 배율 +0.2x 증가 (2.2 / 2.45 / 2.7배).
- **권장 스킬·장비 빌드**: 초감각 조준 R3 · 약점 감지 R3 · 인지 가속 R2 / 초점 장비 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 미구현 후열 슬롯·혼전 차폐 패널티 제거 → 실구현 동일 슬롯 근접 영웅 교전 표적(EnemyId) 연계 조건으로 정렬.
- **참조 화풍**: 강화인간 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 여성 기준).
Subject: 클라라 벡 (후열 관통 사수 (Breach Marksman)), an elite female orbital fleet hero.
Appearance & Outfit: high dark-teal ponytail with loose bangs, crisp azure eyes, white tactical naval coat with cyan inner lining, cybernetic silver-and-cyan left leg, tactical ammo canisters on belt. Incorporate signature faction design cues: sleek white-outer teal-lined tactical naval coat over black-and-cyan armored undersuit, silver-and-cyan cybernetic prosthetic limb with glowing circular joint cores, tactical holsters and precision sci-fi weaponry.
Equipment & Pose: holding a compact heavy-caliber cyan-cell sidearm in the right hand with a folded assault rail-carbine slung cleanly across her back. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 클라라 벡]
Convert the character "클라라 벡" from the previous full-body illustration (or reference style: 강화인간 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (high dark-teal ponytail with loose bangs, crisp azure eyes, white tactical naval coat with cyan inner lining, cybernetic silver-and-cyan left leg, tactical ammo canisters on belt).
Action Pose: stepping back on her cybernetic leg while firing a high-powered cyan rail-pistol with bright muzzle sparks. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R07] 시온 아르카 — 편대 관제 사수 (Strike Wing Controller)
- **분류**: 원거리형 · 복합형 (남성) · **주력 전장**: 함대전 특화 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 3티어 · 무기제어실 (항공모함·전투항모 함재기 화력 관제)
- **함대전 역할/보직 특화**: 함대전 항모 화력 특화 — 일제 출격한 함재기 편대의 물리 탄속·생존력과 집중 타격 연사력을 강화.
- **배경 서사**: 초능력 신경 링크와 사이버네틱 관제 단말로 항모 함재기 편대의 표적 지정과 공격 리듬을 조율하는 관제 에이스.
- **성격 / 화법**: 차분하고 넓은 전장 시야를 가진 관제관 화법.
- **고유 패시브 (편대 탄속·생존 관제)**: 항공모함·전투항모 무기제어실 보직 시 아군 함재기의 볼트 투사체 속도(Profile.BoltSpeed) +12% 및 함재기 회피율(Evasion) +3%p(0.60→0.63).
- **시그니처 스킬 (관제 집중 연사)**: L10 보직모드(8초) 발동 시 탑승 항모 방어 포탑 연사 버프와 함께 출격 중인 아군 함재기의 공격 주기(Profile.Period, 1.2초) -10% 단축 (총 연사 +30% 상한).
- **권장 스킬·장비 빌드**: 초감각 조준 R3 · 전자기 제압 R3 · 인지 가속 R2 / 초점 장비 T3 + 강화 외피 T3
- **밸런스 · C++ 연동 메모**: 미구현 목표 전환 지연·함재기 명중률 롤 제거 → 실구현 OVCarrierSubsystem BoltSpeed·Evasion·Period에 직접 연동.
- **참조 화풍**: 복합형 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 남성 기준).
Subject: 시온 아르카 (편대 관제 사수 (Strike Wing Controller)), an elite male orbital fleet hero.
Appearance & Outfit: textured silver-ash hair, glowing violet cyber-monocle over left eye, white-and-violet officer coat with chrome-blue cybernetic right arm and floating purple wing-telemetry crystals. Incorporate signature faction design cues: white-and-violet naval officer longcoat with gold star epaulettes, chrome-and-cyan cybernetic prosthetic arm, glowing violet cyber-ocular monocle, floating purple psionic crystal halo and energy rings.
Equipment & Pose: holding a compact neural pulse carbine low in the cybernetic right hand while manipulating a floating violet squadron hologram with the left. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 시온 아르카]
Convert the character "시온 아르카" from the previous full-body illustration (or reference style: 복합형 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (textured silver-ash hair, glowing violet cyber-monocle over left eye, white-and-violet officer coat with chrome-blue cybernetic right arm and floating purple wing-telemetry crystals).
Action Pose: aiming a compact pulse carbine while tiny glowing violet fighter-wing holo-markers orbit above his hand. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R08] 비비안 록 — 신경 교란 저격수 (Disruptor Sentinel)
- **분류**: 원거리형 · 초능력자 (여성) · **주력 전장**: 함대전·선내 양면 · **주 임무**: 수호
- **영입 티어 / 보직**: 아카데미 3티어 · 무기제어실 (적 강습 대응·스킬 교란)
- **함대전 역할/보직 특화**: 양면 운용 — 평소 무기제어실 보직을 유지하며 적 정예 강습 영웅 침투 시 스킬 사이클을 무너뜨림.
- **배경 서사**: 적 영웅의 스킬 발동 파장을 감지해 사격으로 신경 집중을 끊어내는 방어형 초능력 사수.
- **성격 / 화법**: 조용하고 날카로운 통찰력을 드러내는 어조.
- **고유 패시브 (돌입 선제 견제)**: 선내 전투에서 반경 12m 내로 돌입(강화 돌입 등)하는 적 영웅에게 선제 반응 사격을 가해 돌입 첫 타격 피해 -15%.
- **시그니처 스킬 (신경 교란탄)**: 초감각 조준 적중 시 대상 적 영웅의 대기 중인 액티브 스킬 쿨타임 +2초 지연 (보직 중에도 기본 L1/L10 보직 정상 가동).
- **권장 스킬·장비 빌드**: 초감각 조준 R3 · 전자기 제압 R3 · 약점 감지 R2 / 초점 장비 T4 + 강화 외피 T3
- **밸런스 · C++ 연동 메모**: 완전 침묵 대신 쿨타임 +2초 지연으로 과도한 CC 락을 방지.
- **참조 화풍**: 초능력자 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 비비안 록 (신경 교란 저격수 (Disruptor Sentinel)), an elite female orbital fleet hero.
Appearance & Outfit: long cascading dark-violet hair with silver geometric hairpin, serene amethyst gaze, white-and-violet naval dress coat with obsidian bodysuit and soft violet neural pulse rings. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: holding a sleek harmonic psionic sniper pistol with extended violet crystal barrel in both hands, poised graceful stance. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 비비안 록]
Convert the character "비비안 록" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long cascading dark-violet hair with silver geometric hairpin, serene amethyst gaze, white-and-violet naval dress coat with obsidian bodysuit and soft violet neural pulse rings).
Action Pose: aiming a crystal-barreled violet sniper pistol releasing concentric purple disruption waves. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R09] 렉스 벤토 — 근접 포격 통제관 (Brawler Gunner)
- **분류**: 원거리형 · 강화인간 (남성) · **주력 전장**: 함대전 특화 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 4티어 · 무기제어실 (돌격 순양함·구축함 근접 난전 부포)
- **함대전 역할/보직 특화**: 함대전 근접 돌격전 특화 — 고속 구축함·돌격 순양함이 적 측면으로 파고들어 폭딜을 넣을 때 최적.
- **배경 서사**: 근거리 함대 난전에서 포탑 회전 속도와 부포 집중 화력을 극한까지 몰아붙이는 공격형 사격 통제관.
- **성격 / 화법**: 거칠고 화끈한 포격수 기질. 근거리 교전을 즐긴다.
- **고유 패시브 (근접 포탑 선회 가속)**: 무기제어실 보직 시 최대 사거리 50% 이하 근거리 교전에서 탑승 함선 주포·부포 선회 속도 +20%.
- **시그니처 스킬 (포화 집중 연장)**: L10 보직모드(발사속도 +15~25%) 지속시간이 8초 → 10초로 연장 (쿨타임 60초·연사 상한 +30% 유지).
- **권장 스킬·장비 빌드**: 전자기 제압 R3 · 초감각 조준 R3 · 약점 감지 R2 / 초점 장비 T4 + 강화 외피 T4
- **밸런스 · C++ 연동 메모**: R03(원거리 70%+)과 정반대인 근거리(50% 이하) 포격전 특화로 역할이 전혀 겹치지 않음.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 렉스 벤토 (근접 포격 통제관 (Brawler Gunner)), an elite male orbital fleet hero.
Appearance & Outfit: swept-back jet-black hair with shaved sides, confident smirk, heavy matte-charcoal gunnery armor with crimson sash accent and reinforced cyan-hydraulic cybernetic left arm. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: gripping a bulky double-barreled electromagnetic assault rifle at hip height, glowing cyan power drum attached. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 렉스 벤토]
Convert the character "렉스 벤토" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (swept-back jet-black hair with shaved sides, confident smirk, heavy matte-charcoal gunnery armor with crimson sash accent and reinforced cyan-hydraulic cybernetic left arm).
Action Pose: bracing wide and unleashing heavy cyan blasts from a bulky twin-barrel electromagnetic rifle. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R10] 아리아 실바 — 종결 탄도사 (Finisher Marksman)
- **분류**: 원거리형 · 복합형 (여성) · **주력 전장**: 함대전·선내 양면 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 4티어 · 무기제어실 (적 파손 함선 마무리·선내 추격)
- **함대전 역할/보직 특화**: 양면 마무리 특화 — 함대전에서 쉴드 붕괴 적함을 확실히 격침하고, 강습전에서는 빈사 적 영웅을 마무리.
- **배경 서사**: 파손된 장갑 틈새와 체력이 떨어진 표적을 놓치지 않는 마무리 전문가. 함대전과 선내전 모두에서 약해진 적을 신속히 종결한다.
- **성격 / 화법**: 차가운 정확성과 절제된 우아함.
- **고유 패시브 (파손 구역 정밀 포착)**: 무기제어실 보직 시 쉴드가 붕괴된 적함 대상 명중 +3%p(합산 +8%p 상한), 선내에서는 HP 40% 이하 표적에게 인지 가속 대기시간 4초 → 2.5초 단축.
- **시그니처 스킬 (영점 종결 사격)**: 인지 가속이 완료된 표적에게 초감각 조준 적중 시 대상의 남은 HP 비율이 35% 이하이면 피해 +15%.
- **권장 스킬·장비 빌드**: 인지 가속 R3 · 초감각 조준 R3 · 약점 감지 R2 / 초점 장비 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 즉사기 금지. 조건부 +15% 마무리 보정으로 깔끔한 밸런스 유지.
- **참조 화풍**: 복합형 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 아리아 실바 (종결 탄도사 (Finisher Marksman)), an elite female orbital fleet hero.
Appearance & Outfit: flowing platinum-silver hair with violet tips, sharp magenta-violet eyes, fitted white-and-violet longcoat with sleek chrome-violet cybernetic right arm and glowing left thigh armor. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: holding a sleek crystal-chambered violet particle rifle aimed slightly upward, subtle purple targeting prism hovering near her right hand. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 아리아 실바]
Convert the character "아리아 실바" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (flowing platinum-silver hair with violet tips, sharp magenta-violet eyes, fitted white-and-violet longcoat with sleek chrome-violet cybernetic right arm and glowing left thigh armor).
Action Pose: locked onto a target with a glowing violet crystal rifle firing a concentrated magenta finisher beam. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S01] 유나 하야시 — 생명선 관리자 (Lifeline Keeper)
- **분류**: 지원형 · 복합형 (여성) · **주력 전장**: 함대전·선내 양면 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 1티어 · 정비 노드 (대형함 생존장·강습 생체 회복)
- **함대전 역할/보직 특화**: 양면 생존 보조 — 정비 노드 생존장으로 재난 승조원 손실을 막고 강습전에서는 위급 생체 회복 담당.
- **배경 서사**: 초능력 생체 감응과 나노 의료 주입기를 결합한 복합형 의무·정비 장교. 시설 파손으로 파괴 구역 재난이 발생한 구역에서도 아군 분대를 끝까지 유지한다.
- **성격 / 화법**: 온화하지만 위기 시 단호하게 퇴각과 치료를 지시한다.
- **고유 패시브 (위급 촉매 공명)**: HP 30% 미만 아군에게 생체 재구성 시전 시 회복량 +10% (무자원 회복 금지, 촉매 1개 정상 소모).
- **시그니처 스킬 (긴급 재난 차폐장)**: 재생 공명장(6m, 5초) 발동 중 같은 슬롯의 파괴 구역 재난(Disasters) 틱 피해 추가 -10%p 경감 (진공·완파 면역 불가).
- **권장 스킬·장비 빌드**: 생체 재구성 R3 · 생존장 전개 R2 · 촉매 동조 R3 / 강화 외피 T3 + 생체 회복 보조
- **밸런스 · C++ 연동 메모**: C++ OVInteriorSubsystem Disasters Protection 변수와 직접 연동. 부활 및 무료 촉매 리필 금지 준수.
- **참조 화풍**: 복합형 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 유나 하야시 (생명선 관리자 (Lifeline Keeper)), an elite female orbital fleet hero.
Appearance & Outfit: soft silver-lavender ponytail with cyan medical hairpin, warm empathetic violet eyes, white-and-violet officer coat with cyan-lit medical vials on belt and sleek silver cybernetic left forearm. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: cradling a glowing cyan-and-violet bio-resonance healing orb in her left palm while holding a compact medical pulse emitter in her right hand. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 유나 하야시]
Convert the character "유나 하야시" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (soft silver-lavender ponytail with cyan medical hairpin, warm empathetic violet eyes, white-and-violet officer coat with cyan-lit medical vials on belt and sleek silver cybernetic left forearm).
Action Pose: casting a bright cyan-violet healing wave from a hovering bio-crystal orb with medical cross-ring sparkles. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S02] 오웬 크래프트 — 선체 응급 정비관 (Hull Fixer)
- **분류**: 지원형 · 강화인간 (남성) · **주력 전장**: 함대전 특화 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 2티어 · 정비 노드 (전함·순양함 함대전 긴급 선체 복구)
- **함대전 역할/보직 특화**: 함대전 장기전 핵심 — 전함·방공함의 교전 지속력과 예비 자원 수리 효율을 극대화.
- **배경 서사**: 장기전 함대 교전에서 반파 직전의 주력함을 살려내는 전설적 수석 기관장. 정비 증폭기의 자원 손실을 줄이고 모듈 붕괴를 늦춘다.
- **성격 / 화법**: 투박하지만 유쾌한 수석 엔지니어 말투.
- **고유 패시브 (예비 자재 절감 정비)**: 정비 노드 보직 시 L10 선체 복구 보직모드(최대 HP 0.5~1% 10초 복구)의 함선 예비 자원 소모 배율을 기지 견적 1.5배 → 1.25배로 절감.
- **시그니처 스킬 (핵심 모듈 비상 보강)**: 정비 노드 보직 중 탑승 함선의 손상된 엔진·무기제어실의 기능 저하 페널티를 12% 완화 (파괴된 모듈 복구 불가).
- **권장 스킬·장비 빌드**: 재생 공명장 R3 · 생존장 전개 R3 · 생체 재구성 R2 / 강화 외피 T4 + 생체 회복 보조
- **밸런스 · C++ 연동 메모**: 무자원 수리 금지 준수(1.25배 실자원 소모 유지). 파괴 모듈 부활 불가.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 오웬 크래프트 (선체 응급 정비관 (Hull Fixer)), an elite male orbital fleet hero.
Appearance & Outfit: short tousled brown hair, practical amber-tinted cyber-goggles pushed on forehead, charcoal-and-white engineering armor coat with heavy articulated cybernetic repair arm and cyan welding torch. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a compact arc-repair tool pistol in his right hand and projecting a cyan structural wireframe schematic from his cybernetic left gauntlet. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 오웬 크래프트]
Convert the character "오웬 크래프트" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (short tousled brown hair, practical amber-tinted cyber-goggles pushed on forehead, charcoal-and-white engineering armor coat with heavy articulated cybernetic repair arm and cyan welding torch).
Action Pose: activating a glowing cyan hull-repair holographic wrench beam from his cybernetic gauntlet. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S03] 셀레네 루 — 역장 안정 조율사 (Shield Harmonizer)
- **분류**: 지원형 · 초능력자 (여성) · **주력 전장**: 함대전 특화 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 2티어 · 정비 노드 (방공함·순양함 비활성 쉴드 회복·재난 억제)
- **함대전 역할/보직 특화**: 함대전 방어막·생존 특화 — D116 비활성 쉴드 점진 회복 사이클을 가속해 방공함·순양함의 연전 생존력 강화.
- **배경 서사**: 함선 방어막의 에너지 파동을 조율하는 초능력 조율사. 붕괴된 쉴드의 재충전 회전율을 끌어올리고 파손 구역 재난 피해를 억제한다.
- **성격 / 화법**: 차분하고 맑은 어조. 에너지 흐름의 균형을 강조한다.
- **고유 패시브 (방어막 위상 재생)**: 정비 노드 보직 시 비활성 쉴드 전투 회복률(ShieldRegenFraction) +15%(0.60%→0.69%/s) 및 비전투 고속 회복 전환 간격(NoHitRecoveryInterval) 15초→12초 단축.
- **시그니처 스킬 (공명 방재장)**: L10 보직모드(10초) 발동 중 탑승 함선 전 슬롯의 파괴 구역 재난(Disasters) 틱 피해 -25% (생존장 Protection과 합산, 진공·완파 면역 불가).
- **권장 스킬·장비 빌드**: 생존장 전개 R3 · 재생 공명장 R3 · 촉매 동조 R2 / 강화 외피 T3 + 생체 회복 보조
- **밸런스 · C++ 연동 메모**: D116 폐기된 ShieldRecoveryDelay·미구현 외부 피격 감압/화재 제거 → 실구현 ShieldRegenFraction·NoHitRecoveryInterval·Disasters로 정렬.
- **참조 화풍**: 초능력자 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 셀레네 루 (역장 안정 조율사 (Shield Harmonizer)), an elite female orbital fleet hero.
Appearance & Outfit: long luminous dark-indigo hair with floating violet halo rings, serene violet eyes, pristine white-and-violet naval officer longcoat with gold star brooch and glowing cyan-violet energy filaments. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: holding both hands gracefully around a hovering translucent violet-and-cyan harmonic shield crystal sphere. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 셀레네 루]
Convert the character "셀레네 루" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long luminous dark-indigo hair with floating violet halo rings, serene violet eyes, pristine white-and-violet naval officer longcoat with gold star brooch and glowing cyan-violet energy filaments).
Action Pose: floating slightly while expanding a soft glowing violet-and-cyan protective dome aura around herself. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S04] 하준 킴 — 격납고 보급 주임 (Hangar Quartermaster)
- **분류**: 지원형 · 강화인간 (남성) · **주력 전장**: 함대전 특화 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 3티어 · 정비 노드 (항공모함·전투항모 함재기 재생산·재무장)
- **함대전 역할/보직 특화**: 함대전 항모 지속전 핵심 — 실구현 ReplacementKits 생산 타이머와 착함 회전 속도를 단축하는 항모 정비 장교.
- **배경 서사**: 항공모함 자원 적재실과 격납고 생산 라인을 총괄하는 베테랑 정비 장교. 손실된 함재기를 빠르게 조립·재무장해 다음 일제 출격을 준비한다.
- **성격 / 화법**: 빈틈없고 신속한 군수·정비 브리핑 말투.
- **고유 패시브 (적재실 함재기 조립 가속)**: 항공모함·전투항모 정비 노드 보직 시 자원 적재실(ReplacementKits) 기반 함재기 재생산 시간(ProductionPeriod) -10% 단축 (실제 키트 정상 소모).
- **시그니처 스킬 (신속 착함·격납고 유지)**: 귀환 함재기의 격납고 착함 시간(Profile.DockSeconds, 1초) -35%(0.65초) 단축 및 L10 보직모드(10초) 중 손상된 격납고(Hangar)의 승조원 효율 저하 20% 완화.
- **권장 스킬·장비 빌드**: 재생 공명장 R3 · 촉매 동조 R3 · 생존장 전개 R2 / 강화 외피 T3 + 이동 보조
- **밸런스 · C++ 연동 메모**: 미구현 귀환 재무장/자동정비 제거 → 실구현 OVCarrierSubsystem ProductionPeriod·DockSeconds·Hangar 효율로 정렬.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 하준 킴 (격납고 보급 주임 (Hangar Quartermaster)), an elite male orbital fleet hero.
Appearance & Outfit: neat short black hair, tactical cyan headset microphone, crisp charcoal-and-white naval deck-crew coat with cybernetic right hand holding a glowing cyan hangar logistics pad. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a compact repair-pulse sidearm at his belt while raising a glowing cyan holographic carrier hangar launch checklist. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 하준 킴]
Convert the character "하준 킴" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (neat short black hair, tactical cyan headset microphone, crisp charcoal-and-white naval deck-crew coat with cybernetic right hand holding a glowing cyan hangar logistics pad).
Action Pose: waving a glowing cyan launch-signal wand while tiny holographic aircraft icons spin beside his wrist pad. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S05] 나디아 벨 — 야전 외상 전문의 (Trauma Specialist)
- **분류**: 지원형 · 복합형 (여성) · **주력 전장**: 선내 강습·수호 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 4티어 · 보직 겸 원정 강습팀·장기전 야전 의무 전담
- **함대전 역할/보직 특화**: 원정 작전 지속력 특화 — 교전 중 치유 회전율과 외상 예방으로 주력 영웅의 전력 공백을 최소화.
- **배경 서사**: 전선에서 중상 영웅의 생명을 붙들고 영구적 후유증(외상)을 최소화하는 최고위 의무관. 장기 원정 강습 작전의 버팀목이다.
- **성격 / 화법**: 차분하고 프로페셔널한 외과의 어조.
- **고유 패시브 (신속 촉매 투여)**: S1 생체 재구성 쿨타임 -1초(6/5.5/5초→5/4.5/4초, 촉매 1개 정상 소모) 및 S3 재생 공명장 치유 반경 6m→8m 확장.
- **시그니처 스킬 (외상 1단계 예방 처치)**: 작전당 1회, 같은 슬롯 아군 영웅 1명이 HP 25% 미만(bWasCriticallyInjured) 생환 시 외상 1단계(bWounded, HP/공격 -10%) 부여를 방지.
- **권장 스킬·장비 빌드**: 생체 재구성 R3 · 촉매 동조 R3 · 재생 공명장 R2 / 강화 외피 T4 + 생체 회복 보조
- **밸런스 · C++ 연동 메모**: C++ 미구현 비전투 야전 회복(2%/s) 제거 → 실구현 SupportCooldown·재생 공명장 반경(600→800cm)·bWounded로 정렬.
- **참조 화풍**: 복합형 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 나디아 벨 (야전 외상 전문의 (Trauma Specialist)), an elite female orbital fleet hero.
Appearance & Outfit: elegant platinum-lilac bob cut, focused violet eyes, spotless white-and-violet long medical-naval coat with gold epaulettes, sleek chrome-magenta cybernetic right arm carrying a glowing bio-stasis canister. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: holding a precision violet bio-beam applicator in her cybernetic right hand and a glowing translucent catalyst vial in her left. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 나디아 벨]
Convert the character "나디아 벨" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (elegant platinum-lilac bob cut, focused violet eyes, spotless white-and-violet long medical-naval coat with gold epaulettes, sleek chrome-magenta cybernetic right arm carrying a glowing bio-stasis canister).
Action Pose: projecting dual cyan-and-violet healing beams from her precision medical applicator with floating catalyst vials. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C01] 마르쿠스 솔 — 함교 전술가 (Bridge Tactician)
- **분류**: 지휘형 · 초능력자 (남성) · **주력 전장**: 함대전·선내 양면 · **주 임무**: 점령
- **영입 티어 / 보직**: 아카데미 1티어 · 함교 (기함 기동 지휘 · 적함 함교 나포 주도)
- **함대전 역할/보직 특화**: 양면 지휘관 — 함교 보직(속도/선회 +5~9%)으로 함선을 지휘하다 결정적 순간 적함 함교 나포를 직접 지휘.
- **배경 서사**: 집단 신경 공명으로 12인 강습팀과 함선 승조원의 리듬을 맞추는 베테랑 지휘관. 함교 나포전과 함대 기동 지휘를 겸한다.
- **성격 / 화법**: 차분한 카리스마와 명확한 전술 브리핑.
- **고유 패시브 (나포 지휘 엄호)**: 함교 점령(Capture, 36초) 수행 중 같은 슬롯 아군 일반 분대원의 선내 피해 추가 -8% 경감 (경감 상한 65%·점령 단축 상한 50% 준수).
- **시그니처 스킬 (전술 재편성 명령)**: 전투 동조(8초) 발동 시 14m 내 아군의 후퇴·방어 재배치 이동속도 추가 +5%p (총 이동 버프 20% 상한 준수).
- **권장 스킬·장비 빌드**: 신경 공명 R3 · 신경 접속 R3 · 전투 동조 R2 / 초점 장비 T3 + 점령 보조
- **밸런스 · C++ 연동 메모**: 미구현 피격 경직 제거 → 실구현 Capture 수행 중 일반 병력 선내 피해 경감으로 정렬.
- **참조 화풍**: 초능력자 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 남성 기준).
Subject: 마르쿠스 솔 (함교 전술가 (Bridge Tactician)), an elite male orbital fleet hero.
Appearance & Outfit: wavy dark-indigo hair, poised commanding violet eyes, sharp white-outer and deep-violet-inner fleet commander longcoat with gold star epaulettes and silver braided cords. Incorporate signature faction design cues: white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards.
Equipment & Pose: right hand tucked calmly into his uniform coat pocket while his left hand projects an orbital violet tactical ring sphere with floating shards. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 마르쿠스 솔]
Convert the character "마르쿠스 솔" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (wavy dark-indigo hair, poised commanding violet eyes, sharp white-outer and deep-violet-inner fleet commander longcoat with gold star epaulettes and silver braided cords).
Action Pose: pointing briskly ahead with a glowing violet tactical command ring hovering around his outstretched hand. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C02] 발레리아 스턴 — 기동 돌파 제독 (Vanguard Commodore)
- **분류**: 지휘형 · 강화인간 (여성) · **주력 전장**: 함대전 특화 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 2티어 · 함교 (순양함·구축함 고속 측면 돌파 기동)
- **함대전 역할/보직 특화**: 함대전 기동전 핵심 — 실구현 OVShip 물리 가속·제동·횡가속을 끌어올려 측면 돌파와 포격 사선 이탈을 극대화.
- **배경 서사**: 신경 직결 조타 인터페이스로 함선을 전투기처럼 몰아붙이는 기동전 전문가. 적 주포 사각을 파고드는 측면 기동에 능하다.
- **성격 / 화법**: 결단력 있고 박력 있는 함교 명령 어조.
- **고유 패시브 (조타 추력 응답 강화)**: 함교 보직 시 탑승 함선의 가속·제동(OVShip Acceleration·Braking) +12% 강화 (총 이동·선회 버프 상한 +20% 엄격 준수).
- **시그니처 스킬 (횡기동 추력 과부하)**: L10 보직모드(선회 +10~20%, 10초) 발동 중 탑승 함선의 횡가속(LateralAcceleration) +18% 및 감속 응답성 추가 강화.
- **권장 스킬·장비 빌드**: 신경 공명 R3 · 전투 동조 R3 · 집단 기동 예지 R2 / 강화 외피 T3 + 이동 보조
- **밸런스 · C++ 연동 메모**: 미구현 함선 회피율(Evasion) 스탯 제거 → 실구현 OVShip Acceleration·Braking·LateralAcceleration 물리 기동으로 정렬.
- **참조 화풍**: 강화인간 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 여성 기준).
Subject: 발레리아 스턴 (기동 돌파 제독 (Vanguard Commodore)), an elite female orbital fleet hero.
Appearance & Outfit: slicked-back dark-teal hair in a tight tactical ponytail, sharp ice-blue eyes with cyan neural temple implants, white-and-teal commander coat over black armored flight suit, cybernetic left leg. Incorporate signature faction design cues: sleek white-outer teal-lined tactical naval coat over black-and-cyan armored undersuit, silver-and-cyan cybernetic prosthetic limb with glowing circular joint cores, tactical holsters and precision sci-fi weaponry.
Equipment & Pose: standing firmly with a sleek tactical command sidearm holstered and right hand gesturing across a glowing cyan ship-vector projection. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 발레리아 스턴]
Convert the character "발레리아 스턴" from the previous full-body illustration (or reference style: 강화인간 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (slicked-back dark-teal hair in a tight tactical ponytail, sharp ice-blue eyes with cyan neural temple implants, white-and-teal commander coat over black armored flight suit, cybernetic left leg).
Action Pose: leaning forward with her coat billowing, thrusting a glowing cyan fleet-vector baton ahead. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C03] 루시안 베일 — 함대 방진 통제관 (Phalanx Admiral)
- **분류**: 지휘형 · 초능력자 (남성) · **주력 전장**: 함대전 특화 · **주 임무**: 수호
- **영입 티어 / 보직**: 아카데미 3티어 · 함교 (전함·방공함대 방어 진형 연계)
- **함대전 역할/보직 특화**: 함대전 함대 시너지 — 전함+방공함 방어 편대의 레이더 공유 정밀도와 선회 기동을 연계 강화.
- **배경 서사**: 집단 신경 공명을 함대 단위로 확장해 호위함과 방공함의 진형 유지를 동기화하는 방어전 사령관.
- **성격 / 화법**: 무게감 있고 흔들림 없는 사령관 화법.
- **고유 패시브 (함대 레이더 공유 증폭)**: 함교 보직 시 탑승 함선 반경 400m 내 아군 함선이 받는 일반 레이더 공유 정확도 계수(OrdinaryShareCoefficient) +20% 보강 (합산 명중 +8%p·빔 98% 상한).
- **시그니처 스킬 (연계 선회 방진)**: L10 보직모드 발동 시 10초간 반경 400m 내 가장 가까운 아군 주력함 1척에게도 선회(TurnBonus) +8% 공유 (총 +20% 상한).
- **권장 스킬·장비 빌드**: 신경 공명 R3 · 전투 동조 R3 · 집단 기동 예지 R2 / 강화 외피 T4 + 이동 보조
- **밸런스 · C++ 연동 메모**: 미구현 표적 공유 지연 제거 → 실구현 OVCombatSubsystem GetDetection OrdinaryShareCoefficient 및 인접 1척 선회 공유로 정렬.
- **참조 화풍**: 초능력자 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 남성 기준).
Subject: 루시안 베일 (함대 방진 통제관 (Phalanx Admiral)), an elite male orbital fleet hero.
Appearance & Outfit: neatly combed obsidian hair with silver streaks at the temples, piercing amethyst eyes, ornate white-and-royal-violet admiral longcoat with double gold star epaulettes and luminous purple orbital halo. Incorporate signature faction design cues: white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards.
Equipment & Pose: standing tall with a sleek ceremonial violet command scepter-baton in his right hand and a geometric fleet-formation sigil floating in his left. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 루시안 베일]
Convert the character "루시안 베일" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (neatly combed obsidian hair with silver streaks at the temples, piercing amethyst eyes, ornate white-and-royal-violet admiral longcoat with double gold star epaulettes and luminous purple orbital halo).
Action Pose: raising a glowing violet command baton as a shimmering hexagonal purple phalanx emblem appears around him. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C04] 소피아 렌 — 항모 항공단장 (Air Wing Commander)
- **분류**: 지휘형 · 복합형 (여성) · **주력 전장**: 함대전 특화 · **주 임무**: 지원
- **영입 티어 / 보직**: 아카데미 3티어 · 함교 (항공모함·전투항모 일제 출격 지휘)
- **함대전 역할/보직 특화**: 함대전 항모 운용 핵심 — 출격 편대의 동시 공격 집중도와 귀환 회전 속도를 극대화.
- **배경 서사**: 보충된 함재기를 성급히 보내지 않고 완편 편대로 집결시켜 일제 출격시키는 항공모함 지휘 전문가.
- **성격 / 화법**: 명쾌하고 리듬감 있는 항공 작전 지휘 어조.
- **고유 패시브 (편대 돌입 시차 압축)**: 항공모함·전투항모 함교 보직 시 출격 함재기 편대의 공격 시차(ManeuverProfile.SectionDelay, 0.12초) -25% 및 재집결 시간(ReformSeconds) -12% 단축.
- **시그니처 스킬 (편대 귀환·재집결 호령)**: L10 보직모드(10초) 발동 시 귀환 중인 아군 함재기의 귀환 이동속도(Profile.ReturnSpeed) +10% 가속 (총 이동 버프 +20% 상한).
- **권장 스킬·장비 빌드**: 신경 공명 R3 · 전투 동조 R3 · 신경 접속 R2 / 초점 장비 T3 + 강화 외피 T3
- **밸런스 · C++ 연동 메모**: 미구현 출격 발진 간격 타이머 제거 → 실구현 OVCarrierSubsystem SectionDelay·ReformSeconds·ReturnSpeed로 정렬 (S04·R07과 0% 중복).
- **참조 화풍**: 복합형 · 여성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 소피아 렌 (항모 항공단장 (Air Wing Commander)), an elite female orbital fleet hero.
Appearance & Outfit: long flowing silver-lavender hair swept to one side with holographic violet halo rings, bright violet eyes, tailored white-and-violet carrier commander coat with chrome-violet cybernetic right arm. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: extending her left hand beneath a vivid orbiting purple squadron formation crystal while holding a sleek neural flight-baton in her cybernetic right hand. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 소피아 렌]
Convert the character "소피아 렌" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long flowing silver-lavender hair swept to one side with holographic violet halo rings, bright violet eyes, tailored white-and-violet carrier commander coat with chrome-violet cybernetic right arm).
Action Pose: commanding aloft with a glowing magenta-violet flight baton as tiny luminous crystal wings swirl behind her. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C05] 빅터 칼리온 — 기함 총사령관 (Flagship Overlord)
- **분류**: 지휘형 · 강화인간 (남성) · **주력 전장**: 함대전 특화 · **주 임무**: 강습
- **영입 티어 / 보직**: 아카데미 4티어 · 함교 (초대형 기함·강습 모함 통합 지휘)
- **함대전 역할/보직 특화**: 함대전·강습 통합 최종 지휘관 — 초대형 기함의 기동력과 포드/강습선 투입 속도를 동시에 끌어올림.
- **배경 서사**: 함대 포격과 12인 강습팀 투사를 단일 작전 시계로 통합 지휘하는 최고위 사령관. 초대형 기함 함교에서 전장을 지배한다.
- **성격 / 화법**: 압도적이고 절제된 최고사령관의 카리스마.
- **고유 패시브 (강습 수송 고속 투사)**: 대형·초대형함 함교 보직 시 탑승 함선에서 발진하는 강습 포드 비행속도(PodSpeed) +12% 및 소속 강습선 도킹 시간(DockingSeconds, 3초→2.4초) -20% 단축.
- **시그니처 스킬 (기함 총공세 신호)**: L10 보직모드(선회 +10~20%, 10초) 발동 시 탑승 함선 내부 교전 중인 아군 전체에게 전투 동조(공속 버프) 효과의 50%를 동시 전파.
- **권장 스킬·장비 빌드**: 신경 공명 R3 · 전투 동조 R3 · 신경 접속 R2 / 강화 외피 T4 + 점령 보조
- **밸런스 · C++ 연동 메모**: C++ 미구현 포드 30초 웨이브 쿨타임 제거 → 실구현 OVTransportSubsystem PodSpeed·DockingSeconds로 정렬.
- **참조 화풍**: 강화인간 · 남성 기준

#### ChatGPT 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 빅터 칼리온 (기함 총사령관 (Flagship Overlord)), an elite male orbital fleet hero.
Appearance & Outfit: commanding silver-streaked dark hair, sharp angular face with subtle glowing cyan neural temple band, heavy black-and-white flagship admiral greatcoat with crimson inner lining and dark chrome cybernetic left arm. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: standing in a commanding posture with his cybernetic left hand resting on a tactical belt holster and right hand raised beside a glowing cyan flagship fleet map. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### ChatGPT 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 빅터 칼리온]
Convert the character "빅터 칼리온" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (commanding silver-streaked dark hair, sharp angular face with subtle glowing cyan neural temple band, heavy black-and-white flagship admiral greatcoat with crimson inner lining and dark chrome cybernetic left arm).
Action Pose: standing tall with greatcoat flaring out, raising a glowing cyan-and-gold fleet command signal holo-banner. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

## 4. R34 / v0.6 후속 결정 과제

### R34-A. 고유 영웅 개성 · 시그니처 스킬 (v0.6 확장) (기획 필요)
- 일반 영입 영웅(4유형 공용 16스킬)과 네임드 고유 영웅의 차이를 고유 패시브 1종으로 둘지, L15 대체 스킬로 둘지?
- 고유 능력이 v0.5 상한(경감 65%, 장비/연구 40%, 지휘 코스트 1)을 우회하지 않도록 가드레일을 어떻게 둘지?

### R34-B. 원천(초능력자 · 강화인간 · 복합형) 차별화 범위 (검토 중)
- v0.5처럼 순수 서사/연출 구분을 유지할지, 아카데미 연구·전용 장비·외상 저항 등 소프트 시너지를 부여할지?
- 복합형 영웅에게 높은 고점 대신 외상 치료비 증가 같은 운용 페널티를 부여할지?

### R34-C. 캐릭터 서사 · 2D 애니 연출 · 함내 컷씬 (초안 단계)
- 2D 애니 화풍 초상화·보이스 라인(출격/보직/점령/부상/긴급 탈출)과 함선 단면도 미니 스프라이트 연동 규격은?
- 포로 구조·사망(영구 손실) 시 전략 HUD 및 캠페인 연대기 로그 연출은 어떻게 처리할지?

### R34-D. 영입 · 스카우트 · 캠페인 보상 루프 (R34 연동)
- 10분마다 갱신되는 4명 영입 풀 외에 캠페인 성계 해방·포로 구출·적 영웅 귀순 이벤트로 특수 영웅을 영입할지?
- 초반 L1 시작 영웅 1명 선택 시 4유형 대표 스타터 캐릭터를 고정 제공할지?


## 5. 전투 시뮬레이션 밸런스 및 C++ 실구현 매핑 검증표

- 검증 대상: 고유 영웅 30명 + 기본(일반·적군 공용) 영웅 4명
- 미구현/폐기 기능 참조: **0건** · v0.5 상한 위반: **0건** · 전체 밸런스 판정: **ALL PASS**
- 고유 영웅 주력 전장 우위 범위: **+5.1% ~ +10.5%** (평균 +8.1%, 기준선 `G01`~`G04` = 0.0%)

| 코드 | 영웅 | 유형 | 선내 우위 | 함대전 우위 | 주력 우위 | 연동 C++ 변수 | 판정 |
| --- | --- | --- | ---: | ---: | ---: | --- | --- |
| `M01` | 카엘 반 | 근거리형 | +8.4% | 0% | +8.4% | `Cover (300->450cm), SlowUntil, SlowFraction (0.25)` | BALANCED |
| `M02` | 이리스 베인 | 근거리형 | +6.2% | 0% | +6.2% | `Enemy->Armor (>=0.20), Ignore (+0.08), SkillActiveUntil` | BALANCED |
| `M03` | 도리안 크루거 | 근거리형 | +7.6% | 0% | +7.6% | `EOVInteriorOrder::Defend, SlowFraction (0.20), Cover (+0.06)` | BALANCED |
| `M04` | 세린 카이 | 근거리형 | +5.1% | 0% | +5.1% | `Evasion (+0.06), Skill3ActiveUntil (Path Hit 0.65x)` | BALANCED |
| `M05` | 볼칸 드라그 | 근거리형 | +7.8% | 0% | +7.8% | `AdvanceOperations(Sabotage 50%), O.Checkpoint, SlowFraction (0.25)` | BALANCED |
| `M06` | 라일라 녹스 | 근거리형 | +6.8% | 0% | +6.8% | `EntryEvents, Speed (+12%), HitChance (-0.08 vs Ranged)` | BALANCED |
| `M07` | 가렌 호크 | 근거리형 | +6.7% | +4.5% | +7.3% | `Disasters (Duration -25%, Tick -25%), Cover (1.5x for <50% HP)` | BALANCED |
| `M08` | 미라 솔라리스 | 근거리형 | +5.5% | 0% | +5.5% | `Skill4ActiveUntil (Period -15%, Damage +10%), Enemy Aura/Cover * 0.5` | BALANCED |
| `M09` | 타릭 오르 | 근거리형 | +6.8% | +5.5% | +8% | `bDutyActive (-10% DR), DutyReadyAt (3s -> 1s)` | BALANCED |
| `M10` | 헤라 크림슨 | 근거리형 | +8.6% | 0% | +8.6% | `AdvanceOperations (+12% Capture), Skill3ReadyAt (-6s on kill)` | BALANCED |
| `R01` | 세라 린 | 원거리형 | +3.5% | +7.5% | +7.5% | `AimSpreadDegrees (-12%), AccuracyBonus (+2%p), TrackedSince, SlowUntil (+1.5s)` | BALANCED |
| `R02` | 마커스 케인 | 원거리형 | +2% | +10.5% | +10.5% | `FOVPDProfile.Interval (-10%), FOVPDProfile.Accuracy (+2%p), PD Cooldown (+15% on L10)` | BALANCED |
| `R03` | 엘레나 보스 | 원거리형 | +2% | +8.5% | +8.5% | `R.Distance >= 0.7*Range (+3%p), FireRateBonus (+6% sustained)` | BALANCED |
| `R04` | 제이드 하퍼 | 원거리형 | +7.1% | 0% | +7.1% | `Range (1800->2000cm), Hit (+3%p @ 12m+), BurstRemaining (Interrupt 2.5s)` | BALANCED |
| `R05` | 노아 발렌틴 | 원거리형 | +2% | +9.2% | +9.2% | `EOVWeaponType::Missile Interval (-8%), GuidanceTurnRate (+25%), AimSpreadDegrees (-15%)` | BALANCED |
| `R06` | 클라라 벡 | 원거리형 | +5.7% | 0% | +5.7% | `Ally Melee EnemyId (+10% Damage), Weakness + RangedMultiplier (+0.2x)` | BALANCED |
| `R07` | 시온 아르카 | 원거리형 | +2% | +8.8% | +8.8% | `Profile.BoltSpeed (+12%), Aircraft.Evasion (+3%p), Profile.Period (-10% on L10)` | BALANCED |
| `R08` | 비비안 록 | 원거리형 | +8.1% | +2% | +8.1% | `Enemy Skill3ActiveUntil (-15% first hit), SkillReadyAt/Skill3ReadyAt (+2s)` | BALANCED |
| `R09` | 렉스 벤토 | 원거리형 | +2% | +8.6% | +8.6% | `R.Distance <= 0.5*Range, TraverseSpeed (+20%), Skill3ActiveUntil (8s->10s)` | BALANCED |
| `R10` | 아리아 실바 | 원거리형 | +5.5% | +5.5% | +7.2% | `!bShieldActive (+3%p), TrackedSince (4s->2.5s @ HP<40%), Finisher (+15% @ HP<35%)` | BALANCED |
| `S01` | 유나 하야시 | 지원형 | +8% | +4% | +8% | `SupportHeal (+10% @ HP<30%), Disasters Protection (+10%p during Skill3)` | BALANCED |
| `S02` | 오웬 크래프트 | 지원형 | +2.5% | +9% | +9% | `DevRepairBaseCost (1.5x -> 1.25x), GetEfficiency(Engine/WeaponControl +12% mitigation)` | BALANCED |
| `S03` | 셀레네 루 | 지원형 | +3% | +9.5% | +9.5% | `ShieldRegenFraction (+15%), NoHitRecoveryInterval (15s->12s), Disasters (-25% on L10)` | BALANCED |
| `S04` | 하준 킴 | 지원형 | +2% | +9.6% | +9.6% | `ProductionPeriod (-10%), DockSeconds (1.0s->0.65s), Hangar Crew Efficiency (+20% on L10)` | BALANCED |
| `S05` | 나디아 벨 | 지원형 | +9.5% | 0% | +9.5% | `SupportCooldown (-1.0s), Skill3 Radius (600->800cm), bWounded Prevention (1/op)` | BALANCED |
| `C01` | 마르쿠스 솔 | 지휘형 | +8% | +3.5% | +8% | `Capture Bridge (Trooper DR +8%p), Skill3ActiveUntil (Retreat/Defend Speed +5%p)` | BALANCED |
| `C02` | 발레리아 스턴 | 지휘형 | +2% | +9.2% | +9.2% | `OVShip.Acceleration (+12%), OVShip.Braking (+12%), LateralAcceleration (+18% on L10)` | BALANCED |
| `C03` | 루시안 베일 | 지휘형 | +2% | +9.3% | +9.3% | `OrdinaryShareCoefficient (+20% @ 400m), TurnBonus (+8% shared to 1 ally)` | BALANCED |
| `C04` | 소피아 렌 | 지휘형 | +2% | +9.4% | +9.4% | `ManeuverProfile.SectionDelay (-25%), ReformSeconds (-12%), Profile.ReturnSpeed (+10% on L10)` | BALANCED |
| `C05` | 빅터 칼리온 | 지휘형 | +5.5% | +8.5% | +8.5% | `FOVTransportProfile.PodSpeed (+12%), DockingSeconds (3.0s->2.4s), L10 Shipwide 50% Skill3 Aura` | BALANCED |
| `G01` | 표준 근거리 장교 | 근거리형 | 0% | 0% | 0% | `EOVInteriorRole::Melee, SkillReadyAt, Skill3ReadyAt, Skill4ReadyAt` | BASELINE |
| `G02` | 표준 원거리 사수 | 원거리형 | 0% | 0% | 0% | `EOVInteriorRole::Ranged, FOVHeroDutyBonuses.AccuracyBonus, FireRateBonus` | BASELINE |
| `G03` | 표준 지원 정비관 | 지원형 | 0% | 0% | 0% | `EOVInteriorRole::Support, Medkits, Disasters, AmplifierCharges` | BASELINE |
| `G04` | 표준 함교 지휘관 | 지휘형 | 0% | 0% | 0% | `EOVInteriorRole::Commander, FOVHeroDutyBonuses.SpeedBonus, TurnBonus` | BASELINE |
