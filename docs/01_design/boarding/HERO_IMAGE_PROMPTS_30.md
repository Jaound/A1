# 영웅 34인 (고유 30인 + 기본 일반·적군 4인) ChatGPT 이미지 생성 프롬프트 카탈로그

- 기준 화풍: 반실사 2D 애니메이션 스탠딩 (3:4) + 선내전투용 데포르메 / SD 캐릭터 변환 (1:1, 순백 배경 `#FFFFFF`)
- 구성: 고유 영웅 30명 (근10·원10·지5·지휘5) + 기본 표준 영웅 4명 (G01~G04, 일반 영입·적군 공용)

## 1. 6종 원천×성별 참조 화풍 기준

| 기준 ID | 분류 | 스타일 핵심 큐 |
| --- | --- | --- |
| `ref-psionic-m` | 초능력자 · 남성 기준 | white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards |
| `ref-psionic-f` | 초능력자 · 여성 기준 | tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards |
| `ref-augmented-m` | 강화인간 · 남성 기준 | heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware |
| `ref-augmented-f` | 강화인간 · 여성 기준 | sleek white-outer teal-lined tactical naval coat over black-and-cyan armored undersuit, silver-and-cyan cybernetic prosthetic limb with glowing circular joint cores, tactical holsters and precision sci-fi weaponry |
| `ref-hybrid-m` | 복합형 · 남성 기준 | white-and-violet naval officer longcoat with gold star epaulettes, chrome-and-cyan cybernetic prosthetic arm, glowing violet cyber-ocular monocle, floating purple psionic crystal halo and energy rings |
| `ref-hybrid-f` | 복합형 · 여성 기준 | white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon |

## 2. 기본 영웅 4인 (일반 영입 · 적군 공용 — G01 ~ G04)

### [G01] 표준 근거리 장교 — 기본 근거리형 (Standard Melee Officer)
- 유형: 근거리형 · 참조 화풍: 강화인간 · 남성 기준 · 고유 패시브/시그니처 없음

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 기본(일반·적군 공용) 영웅 · 참조: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, clean lineart, painterly cel-shading, and cool rim lighting of the attached reference image (강화인간 · 남성 기준).
Subject: 표준 근거리 장교 (기본 근거리형 (Standard Melee Officer)), a standardized mass-commissioned orbital fleet officer usable for both standard allied recruits and enemy faction hero units.
Appearance & Outfit: standard-issue dark charcoal fleet vanguard exo-armor with clean white shoulder trim, opaque glowing cyan tactical half-visor helmet, symmetrical armored gauntlets and reinforced greaves. Standardized military fleet uniform with sleek tactical visor/half-helmet or clean standard insignia (no unique personal ornaments so multiple units and enemy color-swaps read naturally; swap cyan accents to crimson/amber for enemy faction variants).
Equipment & Pose: holding a standard-issue compact kinetic buckler on the left forearm and a regulation high-frequency shock baton in the right hand, disciplined guard stance. Disciplined full-body standing composition from head to boots, centered with slight low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with angled steel struts, cool strip lights, reflective metallic deck floor, and a viewport showing warships above a blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime tactical game unit art, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 기본 영웅 표준 근거리 장교]
Convert the standard fleet hero "표준 근거리 장교" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for instant silhouette readability on a cutaway ship deck, clean anime cel-shading.
Preserved Identity: Keep the standardized fleet helmet/visor, uniform silhouette, and clean faction trim (standard-issue dark charcoal fleet vanguard exo-armor with clean white shoulder trim, opaque glowing cyan tactical half-visor helmet, symmetrical armored gauntlets and reinforced greaves). Designed for easy allied (cyan/white) vs enemy (crimson/charcoal) palette distinction.
Action Pose: bracing forward behind a clean standard cyan-edged forearm buckler with a regulation shock baton raised. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar background, no floor tiles, no text, and no extra props for clean sprite sheet extraction. 1:1 square aspect ratio.
```

### [G02] 표준 원거리 사수 — 기본 원거리형 (Standard Ranged Officer)
- 유형: 원거리형 · 참조 화풍: 강화인간 · 여성 기준 · 고유 패시브/시그니처 없음

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 기본(일반·적군 공용) 영웅 · 참조: 강화인간 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, clean lineart, painterly cel-shading, and cool rim lighting of the attached reference image (강화인간 · 여성 기준).
Subject: 표준 원거리 사수 (기본 원거리형 (Standard Ranged Officer)), a standardized mass-commissioned orbital fleet officer usable for both standard allied recruits and enemy faction hero units.
Appearance & Outfit: standard white-and-slate naval gunnery coat over black armored undersuit, sleek cyan targeting mono-visor across eyes, regulation tactical belt pouches and clean fleet insignia. Standardized military fleet uniform with sleek tactical visor/half-helmet or clean standard insignia (no unique personal ornaments so multiple units and enemy color-swaps read naturally; swap cyan accents to crimson/amber for enemy faction variants).
Equipment & Pose: holding a standard-issue electromagnetic fleet carbine diagonally across her chest with disciplined two-handed trigger posture. Disciplined full-body standing composition from head to boots, centered with slight low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with angled steel struts, cool strip lights, reflective metallic deck floor, and a viewport showing warships above a blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime tactical game unit art, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 기본 영웅 표준 원거리 사수]
Convert the standard fleet hero "표준 원거리 사수" from the previous full-body illustration (or reference style: 강화인간 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for instant silhouette readability on a cutaway ship deck, clean anime cel-shading.
Preserved Identity: Keep the standardized fleet helmet/visor, uniform silhouette, and clean faction trim (standard white-and-slate naval gunnery coat over black armored undersuit, sleek cyan targeting mono-visor across eyes, regulation tactical belt pouches and clean fleet insignia). Designed for easy allied (cyan/white) vs enemy (crimson/charcoal) palette distinction.
Action Pose: aiming a clean regulation cyan-lit electromagnetic fleet carbine in a steady mid-range firing stance. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar background, no floor tiles, no text, and no extra props for clean sprite sheet extraction. 1:1 square aspect ratio.
```

### [G03] 표준 지원 정비관 — 기본 지원형 (Standard Support Officer)
- 유형: 지원형 · 참조 화풍: 초능력자 · 여성 기준 · 고유 패시브/시그니처 없음

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 기본(일반·적군 공용) 영웅 · 참조: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, clean lineart, painterly cel-shading, and cool rim lighting of the attached reference image (초능력자 · 여성 기준).
Subject: 표준 지원 정비관 (기본 지원형 (Standard Support Officer)), a standardized mass-commissioned orbital fleet officer usable for both standard allied recruits and enemy faction hero units.
Appearance & Outfit: standard clean white-and-navy fleet medical-engineering coat with luminous cyan cross-stripe trim, translucent tactical comms visor, standard catalyst canisters clipped neatly to belt. Standardized military fleet uniform with sleek tactical visor/half-helmet or clean standard insignia (no unique personal ornaments so multiple units and enemy color-swaps read naturally; swap cyan accents to crimson/amber for enemy faction variants).
Equipment & Pose: holding a regulation cyan bio-repair emitter wand in her right hand and a standard holographic triage tablet in her left. Disciplined full-body standing composition from head to boots, centered with slight low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with angled steel struts, cool strip lights, reflective metallic deck floor, and a viewport showing warships above a blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime tactical game unit art, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 기본 영웅 표준 지원 정비관]
Convert the standard fleet hero "표준 지원 정비관" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for instant silhouette readability on a cutaway ship deck, clean anime cel-shading.
Preserved Identity: Keep the standardized fleet helmet/visor, uniform silhouette, and clean faction trim (standard clean white-and-navy fleet medical-engineering coat with luminous cyan cross-stripe trim, translucent tactical comms visor, standard catalyst canisters clipped neatly to belt). Designed for easy allied (cyan/white) vs enemy (crimson/charcoal) palette distinction.
Action Pose: projecting a clean glowing cyan bio-healing pulse from a regulation medical emitter with a floating triage ring. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar background, no floor tiles, no text, and no extra props for clean sprite sheet extraction. 1:1 square aspect ratio.
```

### [G04] 표준 함교 지휘관 — 기본 지휘형 (Standard Command Officer)
- 유형: 지휘형 · 참조 화풍: 초능력자 · 남성 기준 · 고유 패시브/시그니처 없음

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 기본(일반·적군 공용) 영웅 · 참조: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, clean lineart, painterly cel-shading, and cool rim lighting of the attached reference image (초능력자 · 남성 기준).
Subject: 표준 함교 지휘관 (기본 지휘형 (Standard Command Officer)), a standardized mass-commissioned orbital fleet officer usable for both standard allied recruits and enemy faction hero units.
Appearance & Outfit: standard crisp white-and-dark-slate fleet bridge officer longcoat with clean silver shoulder bars, sleek translucent cyan command visor, regulation utility belt and tactical sidearm holster. Standardized military fleet uniform with sleek tactical visor/half-helmet or clean standard insignia (no unique personal ornaments so multiple units and enemy color-swaps read naturally; swap cyan accents to crimson/amber for enemy faction variants).
Equipment & Pose: standing upright in disciplined command posture, right hand gesturing forward beside a clean cyan holographic tactical ring while left hand rests on belt. Disciplined full-body standing composition from head to boots, centered with slight low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with angled steel struts, cool strip lights, reflective metallic deck floor, and a viewport showing warships above a blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime tactical game unit art, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 기본 영웅 표준 함교 지휘관]
Convert the standard fleet hero "표준 함교 지휘관" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for instant silhouette readability on a cutaway ship deck, clean anime cel-shading.
Preserved Identity: Keep the standardized fleet helmet/visor, uniform silhouette, and clean faction trim (standard crisp white-and-dark-slate fleet bridge officer longcoat with clean silver shoulder bars, sleek translucent cyan command visor, regulation utility belt and tactical sidearm holster). Designed for easy allied (cyan/white) vs enemy (crimson/charcoal) palette distinction.
Action Pose: pointing cleanly forward with a standard glowing cyan tactical command baton and crisp command aura ring. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar background, no floor tiles, no text, and no extra props for clean sprite sheet extraction. 1:1 square aspect ratio.
```

## 3. 고유 영웅 30인 (M01 ~ C05)

### [M01] 카엘 반 — 격벽 돌파자 (Breach Vanguard)
- 분류: 근거리형 · 강화인간 (남성) · 선내 강습·수호 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 카엘 반 (격벽 돌파자 (Breach Vanguard)), an elite male orbital fleet hero.
Appearance & Outfit: rugged short dark-brown hair, sharp determined jaw with faint cheek scar, heavy charcoal breach-plate chest armor with crimson inner waist coat and bulky cyan-lit cybernetic left arm. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a heavy kinetic breach-shield integrated onto the cybernetic left forearm and a short high-frequency thermal baton in the right hand, braced vanguard stance. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 카엘 반]
Convert the character "카엘 반" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (rugged short dark-brown hair, sharp determined jaw with faint cheek scar, heavy charcoal breach-plate chest armor with crimson inner waist coat and bulky cyan-lit cybernetic left arm).
Action Pose: charging forward behind a glowing cyan kinetic forearm shield while swinging a compact thermal shock baton. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M02] 이리스 베인 — 위상 검객 (Phase Duelist)
- 분류: 근거리형 · 복합형 (여성) · 선내 강습·수호 · 참조 화풍: 복합형 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 이리스 베인 (위상 검객 (Phase Duelist)), an elite female orbital fleet hero.
Appearance & Outfit: flowing silver-lilac high ponytail, luminous violet eyes, holographic purple halo rings, sleek white-and-violet longcoat with silver-magenta cybernetic right arm and left leg. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: gripping a crystalline violet psionic energy blade downward in the cybernetic right hand while orbiting purple shards hover above the left palm. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 이리스 베인]
Convert the character "이리스 베인" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (flowing silver-lilac high ponytail, luminous violet eyes, holographic purple halo rings, sleek white-and-violet longcoat with silver-magenta cybernetic right arm and left leg).
Action Pose: lunging in a sleek fencing slash with a bright glowing violet crystal sword trailing magenta sparks. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M03] 도리안 크루거 — 격실 방벽 (Bulkhead Sentinel)
- 분류: 근거리형 · 강화인간 (남성) · 선내 강습·수호 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 도리안 크루거 (격실 방벽 (Bulkhead Sentinel)), an elite male orbital fleet hero.
Appearance & Outfit: salt-and-pepper cropped hair, square weathered face with cyan ocular strip across left temple, broad matte-gunmetal security exo-plate with amber-and-cyan hazard trim. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: planting a heavy collapsible riot tower shield on the deck with both reinforced cybernetic gauntlets, immovable guardian posture. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 도리안 크루거]
Convert the character "도리안 크루거" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (salt-and-pepper cropped hair, square weathered face with cyan ocular strip across left temple, broad matte-gunmetal security exo-plate with amber-and-cyan hazard trim).
Action Pose: bracing firmly behind a wide gunmetal riot shield with a glowing cyan barrier edge, blocking incoming fire. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M04] 세린 카이 — 유성 기동수 (Meteor Skirmisher)
- 분류: 근거리형 · 초능력자 (여성) · 선내 강습·수호 · 참조 화풍: 초능력자 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 세린 카이 (유성 기동수 (Meteor Skirmisher)), an elite female orbital fleet hero.
Appearance & Outfit: layered midnight-violet bob cut with glowing cyan hair clips, slender agile silhouette in a cropped white-and-violet naval jacket, obsidian stealth leggings with glowing violet boot thrusters. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: poised lightly on the balls of her feet holding twin curved violet psionic tonfa blades, swirling lavender energy ribbons around ankles. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 세린 카이]
Convert the character "세린 카이" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (layered midnight-violet bob cut with glowing cyan hair clips, slender agile silhouette in a cropped white-and-violet naval jacket, obsidian stealth leggings with glowing violet boot thrusters).
Action Pose: dashing forward at high speed with twin glowing violet tonfa blades and starlight motion trails. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M05] 볼칸 드라그 — 설비 파쇄공 (Reactor Breaker)
- 분류: 근거리형 · 강화인간 (남성) · 선내 강습·수호 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 볼칸 드라그 (설비 파쇄공 (Reactor Breaker)), an elite male orbital fleet hero.
Appearance & Outfit: spiky iron-grey hair, rugged industrial beard, heavy welded charcoal armor with heat-vented copper-orange and cyan hydraulic piston arms, demolition satchel across chest. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: resting a heavy plasma-heated hydraulic breaching hammer across his right shoulder, left cybernetic fist venting white steam. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 볼칸 드라그]
Convert the character "볼칸 드라그" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (spiky iron-grey hair, rugged industrial beard, heavy welded charcoal armor with heat-vented copper-orange and cyan hydraulic piston arms, demolition satchel across chest).
Action Pose: heaving a glowing cyan-and-orange hydraulic breaching hammer overhead for a heavy smash. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M06] 라일라 녹스 — 역강습 추적자 (Counter-Boarder)
- 분류: 근거리형 · 초능력자 (여성) · 선내 강습·수호 · 참조 화풍: 초능력자 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 라일라 녹스 (역강습 추적자 (Counter-Boarder)), an elite female orbital fleet hero.
Appearance & Outfit: long wavy obsidian-indigo hair tied back with a silver star clasp, luminous lavender eyes, crisp white-and-violet tactical longcoat with dark armored gauntlets and orbiting psionic barrier shards. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: holding a sleek psionic halberd-spear diagonally across her body while lavender telekinetic rings shimmer around her left palm. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 라일라 녹스]
Convert the character "라일라 녹스" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long wavy obsidian-indigo hair tied back with a silver star clasp, luminous lavender eyes, crisp white-and-violet tactical longcoat with dark armored gauntlets and orbiting psionic barrier shards).
Action Pose: spinning a glowing violet psionic spear in a defensive sweep surrounded by small floating crystal shards. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M07] 가렌 호크 — 기관실 수호자 (Core Warden)
- 분류: 근거리형 · 강화인간 (남성) · 함대전·선내 양면 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 가렌 호크 (기관실 수호자 (Core Warden)), an elite male orbital fleet hero.
Appearance & Outfit: short cropped chestnut hair, thick brow, reinforced slate-black thermal-resistant armor coat with glowing cyan coolant tubes along the collar and heavy cybernetic shoulders. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a compact thermal-shield buckler on the left arm and a pressurized cryo-axe in the right hand, steady protective stance. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 가렌 호크]
Convert the character "가렌 호크" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (short cropped chestnut hair, thick brow, reinforced slate-black thermal-resistant armor coat with glowing cyan coolant tubes along the collar and heavy cybernetic shoulders).
Action Pose: holding up a cyan-edged thermal buckler with one hand while swinging a compact cryo-axe. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M08] 미라 솔라리스 — 임계 결투자 (Apex Duelist)
- 분류: 근거리형 · 복합형 (여성) · 선내 강습·수호 · 참조 화풍: 복합형 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 미라 솔라리스 (임계 결투자 (Apex Duelist)), an elite female orbital fleet hero.
Appearance & Outfit: long silver-white hair with dark magenta underlayer, intense violet eyes, regal white-and-violet high-collar naval coat with sleek dark cybernetic arms and glowing magenta spine stabilizer. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: wielding a long double-edged crimson-violet psionic glaive held effortlessly across her shoulders, floating purple crystal halo behind her. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 미라 솔라리스]
Convert the character "미라 솔라리스" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long silver-white hair with dark magenta underlayer, intense violet eyes, regal white-and-violet high-collar naval coat with sleek dark cybernetic arms and glowing magenta spine stabilizer).
Action Pose: poised in a fierce low stance swinging a glowing magenta-violet psionic glaive with crackling energy. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M09] 타릭 오르 — 함교 친위대장 (Bridge Protector)
- 분류: 근거리형 · 초능력자 (남성) · 함대전·선내 양면 · 참조 화풍: 초능력자 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 남성 기준).
Subject: 타릭 오르 (함교 친위대장 (Bridge Protector)), an elite male orbital fleet hero.
Appearance & Outfit: neatly swept jet-black hair with subtle violet sheen, aristocratic stoic expression, pristine white-and-royal-purple naval dress coat with gold star epaulettes and glowing violet forearm barrier emitters. Incorporate signature faction design cues: white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards.
Equipment & Pose: standing upright with a ceremonial violet psionic energy saber drawn in the right hand and a translucent hexagonal violet ward floating over his left hand. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 타릭 오르]
Convert the character "타릭 오르" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (neatly swept jet-black hair with subtle violet sheen, aristocratic stoic expression, pristine white-and-royal-purple naval dress coat with gold star epaulettes and glowing violet forearm barrier emitters).
Action Pose: guarding upright with a bright violet psionic saber raised and a hexagonal purple ward glowing in front. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [M10] 헤라 크림슨 — 나포 선봉장 (Capture Raider)
- 분류: 근거리형 · 복합형 (여성) · 선내 강습·수호 · 참조 화풍: 복합형 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 헤라 크림슨 (나포 선봉장 (Capture Raider)), an elite female orbital fleet hero.
Appearance & Outfit: sweeping silver-ash hair with crimson-violet hair ribbon, fierce amber-violet eyes, sharp white-and-violet assault coat with blackened cybernetic right arm and left leg plating. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: holding a plasma-violet boarding cutlass pointed forward in her cybernetic right hand, swirling violet energy sparks trailing from the blade. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 헤라 크림슨]
Convert the character "헤라 크림슨" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (sweeping silver-ash hair with crimson-violet hair ribbon, fierce amber-violet eyes, sharp white-and-violet assault coat with blackened cybernetic right arm and left leg plating).
Action Pose: leaping forward pointing a glowing magenta-violet boarding cutlass ahead in a daring raid pose. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R01] 세라 린 — 궤도 사수 (Orbital Deadeye)
- 분류: 원거리형 · 초능력자 (여성) · 함대전 특화 · 참조 화풍: 초능력자 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 세라 린 (궤도 사수 (Orbital Deadeye)), an elite female orbital fleet hero.
Appearance & Outfit: long sleek midnight-indigo hair pinned with a cyan targeting ornament, calm piercing violet eyes, white-and-violet naval officer longcoat with gold epaulettes and translucent violet trajectory rings around her visor. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: cradling a sleek long-barreled psionic rail-carbine with glowing cyan-violet coils in both hands, poised ballistic stance. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 세라 린]
Convert the character "세라 린" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long sleek midnight-indigo hair pinned with a cyan targeting ornament, calm piercing violet eyes, white-and-violet naval officer longcoat with gold epaulettes and translucent violet trajectory rings around her visor).
Action Pose: aiming down a glowing cyan-and-violet sci-fi sniper rail-carbine with a tiny purple targeting reticle floating near her eye. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R02] 마커스 케인 — 방공망 통제관 (Aegis Gunner)
- 분류: 원거리형 · 강화인간 (남성) · 함대전 특화 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 마커스 케인 (방공망 통제관 (Aegis Gunner)), an elite male orbital fleet hero.
Appearance & Outfit: short tousled dark-brown hair, cybernetic optical scanner mounted over left eye, dark gunmetal tactical jacket with crimson lining, cybernetic left forearm linked to a holo-radar wrist display. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a compact rapid-fire pulse PD carbine with glowing cyan barrel vents in his right hand, tracking targets calmly. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 마커스 케인]
Convert the character "마커스 케인" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (short tousled dark-brown hair, cybernetic optical scanner mounted over left eye, dark gunmetal tactical jacket with crimson lining, cybernetic left forearm linked to a holo-radar wrist display).
Action Pose: firing rapid bursts from a compact cyan-lit pulse carbine while a tiny holographic radar ring glows beside his visor. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R03] 엘레나 보스 — 장거리 포격수 (Longbow Marksman)
- 분류: 원거리형 · 복합형 (여성) · 함대전 특화 · 참조 화풍: 복합형 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 엘레나 보스 (장거리 포격수 (Longbow Marksman)), an elite female orbital fleet hero.
Appearance & Outfit: sleek silver-violet half-up hair, cool amethyst eyes with a glowing cyan cyber-reticle lens on her left cheek, white-and-violet longcoat with chrome-violet cybernetic right arm. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: standing tall with an elongated precision energy lance-rifle resting vertically beside her, orbiting violet prism rings along the barrel. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 엘레나 보스]
Convert the character "엘레나 보스" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (sleek silver-violet half-up hair, cool amethyst eyes with a glowing cyan cyber-reticle lens on her left cheek, white-and-violet longcoat with chrome-violet cybernetic right arm).
Action Pose: kneeling slightly to fire a long glowing violet-and-cyan precision beam rifle with crisp recoil sparks. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R04] 제이드 하퍼 — 복도 저격수 (Corridor Marksman)
- 분류: 원거리형 · 강화인간 (남성) · 선내 강습·수호 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 제이드 하퍼 (복도 저격수 (Corridor Marksman)), an elite male orbital fleet hero.
Appearance & Outfit: messy dark-teal hair, sharp slate eyes, charcoal-and-white tactical reconnaissance coat with cyan knee armor and cybernetic aiming brace on right forearm. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: resting a bullpup electromagnetic marksman rifle across his chest, fingers poised near the cyan trigger guard. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 제이드 하퍼]
Convert the character "제이드 하퍼" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (messy dark-teal hair, sharp slate eyes, charcoal-and-white tactical reconnaissance coat with cyan knee armor and cybernetic aiming brace on right forearm).
Action Pose: crouching in a sharp sniper stance firing an electromagnetic bullpup rifle with bright blue muzzle flash. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R05] 노아 발렌틴 — 일제사격 지휘사수 (Salvo Coordinator)
- 분류: 원거리형 · 초능력자 (남성) · 함대전 특화 · 참조 화풍: 초능력자 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 남성 기준).
Subject: 노아 발렌틴 (일제사격 지휘사수 (Salvo Coordinator)), an elite male orbital fleet hero.
Appearance & Outfit: wavy dark-indigo hair parted neatly, luminous violet eyes, white-and-purple officer longcoat with gold braided epaulettes and holographic purple missile trajectory arcs floating beside his raised hand. Incorporate signature faction design cues: white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards.
Equipment & Pose: holding a sleek psionic target-designator pistol in his right hand while guiding glowing violet lock-on crystals with his left. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 노아 발렌틴]
Convert the character "노아 발렌틴" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (wavy dark-indigo hair parted neatly, luminous violet eyes, white-and-purple officer longcoat with gold braided epaulettes and holographic purple missile trajectory arcs floating beside his raised hand).
Action Pose: pointing a glowing violet designator pistol forward as tiny purple lock-on rings orbit around him. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R06] 클라라 벡 — 후열 관통 사수 (Breach Marksman)
- 분류: 원거리형 · 강화인간 (여성) · 선내 강습·수호 · 참조 화풍: 강화인간 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 여성 기준).
Subject: 클라라 벡 (후열 관통 사수 (Breach Marksman)), an elite female orbital fleet hero.
Appearance & Outfit: high dark-teal ponytail with loose bangs, crisp azure eyes, white tactical naval coat with cyan inner lining, cybernetic silver-and-cyan left leg, tactical ammo canisters on belt. Incorporate signature faction design cues: sleek white-outer teal-lined tactical naval coat over black-and-cyan armored undersuit, silver-and-cyan cybernetic prosthetic limb with glowing circular joint cores, tactical holsters and precision sci-fi weaponry.
Equipment & Pose: holding a compact heavy-caliber cyan-cell sidearm in the right hand with a folded assault rail-carbine slung cleanly across her back. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 클라라 벡]
Convert the character "클라라 벡" from the previous full-body illustration (or reference style: 강화인간 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (high dark-teal ponytail with loose bangs, crisp azure eyes, white tactical naval coat with cyan inner lining, cybernetic silver-and-cyan left leg, tactical ammo canisters on belt).
Action Pose: stepping back on her cybernetic leg while firing a high-powered cyan rail-pistol with bright muzzle sparks. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R07] 시온 아르카 — 편대 관제 사수 (Strike Wing Controller)
- 분류: 원거리형 · 복합형 (남성) · 함대전 특화 · 참조 화풍: 복합형 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 남성 기준).
Subject: 시온 아르카 (편대 관제 사수 (Strike Wing Controller)), an elite male orbital fleet hero.
Appearance & Outfit: textured silver-ash hair, glowing violet cyber-monocle over left eye, white-and-violet officer coat with chrome-blue cybernetic right arm and floating purple wing-telemetry crystals. Incorporate signature faction design cues: white-and-violet naval officer longcoat with gold star epaulettes, chrome-and-cyan cybernetic prosthetic arm, glowing violet cyber-ocular monocle, floating purple psionic crystal halo and energy rings.
Equipment & Pose: holding a compact neural pulse carbine low in the cybernetic right hand while manipulating a floating violet squadron hologram with the left. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 시온 아르카]
Convert the character "시온 아르카" from the previous full-body illustration (or reference style: 복합형 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (textured silver-ash hair, glowing violet cyber-monocle over left eye, white-and-violet officer coat with chrome-blue cybernetic right arm and floating purple wing-telemetry crystals).
Action Pose: aiming a compact pulse carbine while tiny glowing violet fighter-wing holo-markers orbit above his hand. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R08] 비비안 록 — 신경 교란 저격수 (Disruptor Sentinel)
- 분류: 원거리형 · 초능력자 (여성) · 함대전·선내 양면 · 참조 화풍: 초능력자 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 비비안 록 (신경 교란 저격수 (Disruptor Sentinel)), an elite female orbital fleet hero.
Appearance & Outfit: long cascading dark-violet hair with silver geometric hairpin, serene amethyst gaze, white-and-violet naval dress coat with obsidian bodysuit and soft violet neural pulse rings. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: holding a sleek harmonic psionic sniper pistol with extended violet crystal barrel in both hands, poised graceful stance. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 비비안 록]
Convert the character "비비안 록" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long cascading dark-violet hair with silver geometric hairpin, serene amethyst gaze, white-and-violet naval dress coat with obsidian bodysuit and soft violet neural pulse rings).
Action Pose: aiming a crystal-barreled violet sniper pistol releasing concentric purple disruption waves. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R09] 렉스 벤토 — 근접 포격 통제관 (Brawler Gunner)
- 분류: 원거리형 · 강화인간 (남성) · 함대전 특화 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 렉스 벤토 (근접 포격 통제관 (Brawler Gunner)), an elite male orbital fleet hero.
Appearance & Outfit: swept-back jet-black hair with shaved sides, confident smirk, heavy matte-charcoal gunnery armor with crimson sash accent and reinforced cyan-hydraulic cybernetic left arm. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: gripping a bulky double-barreled electromagnetic assault rifle at hip height, glowing cyan power drum attached. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 렉스 벤토]
Convert the character "렉스 벤토" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (swept-back jet-black hair with shaved sides, confident smirk, heavy matte-charcoal gunnery armor with crimson sash accent and reinforced cyan-hydraulic cybernetic left arm).
Action Pose: bracing wide and unleashing heavy cyan blasts from a bulky twin-barrel electromagnetic rifle. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [R10] 아리아 실바 — 종결 탄도사 (Finisher Marksman)
- 분류: 원거리형 · 복합형 (여성) · 함대전·선내 양면 · 참조 화풍: 복합형 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 아리아 실바 (종결 탄도사 (Finisher Marksman)), an elite female orbital fleet hero.
Appearance & Outfit: flowing platinum-silver hair with violet tips, sharp magenta-violet eyes, fitted white-and-violet longcoat with sleek chrome-violet cybernetic right arm and glowing left thigh armor. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: holding a sleek crystal-chambered violet particle rifle aimed slightly upward, subtle purple targeting prism hovering near her right hand. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 아리아 실바]
Convert the character "아리아 실바" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (flowing platinum-silver hair with violet tips, sharp magenta-violet eyes, fitted white-and-violet longcoat with sleek chrome-violet cybernetic right arm and glowing left thigh armor).
Action Pose: locked onto a target with a glowing violet crystal rifle firing a concentrated magenta finisher beam. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S01] 유나 하야시 — 생명선 관리자 (Lifeline Keeper)
- 분류: 지원형 · 복합형 (여성) · 함대전·선내 양면 · 참조 화풍: 복합형 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 유나 하야시 (생명선 관리자 (Lifeline Keeper)), an elite female orbital fleet hero.
Appearance & Outfit: soft silver-lavender ponytail with cyan medical hairpin, warm empathetic violet eyes, white-and-violet officer coat with cyan-lit medical vials on belt and sleek silver cybernetic left forearm. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: cradling a glowing cyan-and-violet bio-resonance healing orb in her left palm while holding a compact medical pulse emitter in her right hand. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 유나 하야시]
Convert the character "유나 하야시" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (soft silver-lavender ponytail with cyan medical hairpin, warm empathetic violet eyes, white-and-violet officer coat with cyan-lit medical vials on belt and sleek silver cybernetic left forearm).
Action Pose: casting a bright cyan-violet healing wave from a hovering bio-crystal orb with medical cross-ring sparkles. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S02] 오웬 크래프트 — 선체 응급 정비관 (Hull Fixer)
- 분류: 지원형 · 강화인간 (남성) · 함대전 특화 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 오웬 크래프트 (선체 응급 정비관 (Hull Fixer)), an elite male orbital fleet hero.
Appearance & Outfit: short tousled brown hair, practical amber-tinted cyber-goggles pushed on forehead, charcoal-and-white engineering armor coat with heavy articulated cybernetic repair arm and cyan welding torch. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a compact arc-repair tool pistol in his right hand and projecting a cyan structural wireframe schematic from his cybernetic left gauntlet. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 오웬 크래프트]
Convert the character "오웬 크래프트" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (short tousled brown hair, practical amber-tinted cyber-goggles pushed on forehead, charcoal-and-white engineering armor coat with heavy articulated cybernetic repair arm and cyan welding torch).
Action Pose: activating a glowing cyan hull-repair holographic wrench beam from his cybernetic gauntlet. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S03] 셀레네 루 — 역장 안정 조율사 (Shield Harmonizer)
- 분류: 지원형 · 초능력자 (여성) · 함대전 특화 · 참조 화풍: 초능력자 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 여성 기준).
Subject: 셀레네 루 (역장 안정 조율사 (Shield Harmonizer)), an elite female orbital fleet hero.
Appearance & Outfit: long luminous dark-indigo hair with floating violet halo rings, serene violet eyes, pristine white-and-violet naval officer longcoat with gold star brooch and glowing cyan-violet energy filaments. Incorporate signature faction design cues: tailored white-and-violet naval officer longcoat with gold star brooch and silver cords, obsidian tech bodysuit with cyan accent lines, luminous violet psionic halo behind head and orbiting geometric crystal shards.
Equipment & Pose: holding both hands gracefully around a hovering translucent violet-and-cyan harmonic shield crystal sphere. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 셀레네 루]
Convert the character "셀레네 루" from the previous full-body illustration (or reference style: 초능력자 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long luminous dark-indigo hair with floating violet halo rings, serene violet eyes, pristine white-and-violet naval officer longcoat with gold star brooch and glowing cyan-violet energy filaments).
Action Pose: floating slightly while expanding a soft glowing violet-and-cyan protective dome aura around herself. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S04] 하준 킴 — 격납고 보급 주임 (Hangar Quartermaster)
- 분류: 지원형 · 강화인간 (남성) · 함대전 특화 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 하준 킴 (격납고 보급 주임 (Hangar Quartermaster)), an elite male orbital fleet hero.
Appearance & Outfit: neat short black hair, tactical cyan headset microphone, crisp charcoal-and-white naval deck-crew coat with cybernetic right hand holding a glowing cyan hangar logistics pad. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: holding a compact repair-pulse sidearm at his belt while raising a glowing cyan holographic carrier hangar launch checklist. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 하준 킴]
Convert the character "하준 킴" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (neat short black hair, tactical cyan headset microphone, crisp charcoal-and-white naval deck-crew coat with cybernetic right hand holding a glowing cyan hangar logistics pad).
Action Pose: waving a glowing cyan launch-signal wand while tiny holographic aircraft icons spin beside his wrist pad. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [S05] 나디아 벨 — 야전 외상 전문의 (Trauma Specialist)
- 분류: 지원형 · 복합형 (여성) · 선내 강습·수호 · 참조 화풍: 복합형 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 나디아 벨 (야전 외상 전문의 (Trauma Specialist)), an elite female orbital fleet hero.
Appearance & Outfit: elegant platinum-lilac bob cut, focused violet eyes, spotless white-and-violet long medical-naval coat with gold epaulettes, sleek chrome-magenta cybernetic right arm carrying a glowing bio-stasis canister. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: holding a precision violet bio-beam applicator in her cybernetic right hand and a glowing translucent catalyst vial in her left. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 나디아 벨]
Convert the character "나디아 벨" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (elegant platinum-lilac bob cut, focused violet eyes, spotless white-and-violet long medical-naval coat with gold epaulettes, sleek chrome-magenta cybernetic right arm carrying a glowing bio-stasis canister).
Action Pose: projecting dual cyan-and-violet healing beams from her precision medical applicator with floating catalyst vials. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C01] 마르쿠스 솔 — 함교 전술가 (Bridge Tactician)
- 분류: 지휘형 · 초능력자 (남성) · 함대전·선내 양면 · 참조 화풍: 초능력자 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 남성 기준).
Subject: 마르쿠스 솔 (함교 전술가 (Bridge Tactician)), an elite male orbital fleet hero.
Appearance & Outfit: wavy dark-indigo hair, poised commanding violet eyes, sharp white-outer and deep-violet-inner fleet commander longcoat with gold star epaulettes and silver braided cords. Incorporate signature faction design cues: white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards.
Equipment & Pose: right hand tucked calmly into his uniform coat pocket while his left hand projects an orbital violet tactical ring sphere with floating shards. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 마르쿠스 솔]
Convert the character "마르쿠스 솔" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (wavy dark-indigo hair, poised commanding violet eyes, sharp white-outer and deep-violet-inner fleet commander longcoat with gold star epaulettes and silver braided cords).
Action Pose: pointing briskly ahead with a glowing violet tactical command ring hovering around his outstretched hand. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C02] 발레리아 스턴 — 기동 돌파 제독 (Vanguard Commodore)
- 분류: 지휘형 · 강화인간 (여성) · 함대전 특화 · 참조 화풍: 강화인간 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 여성 기준).
Subject: 발레리아 스턴 (기동 돌파 제독 (Vanguard Commodore)), an elite female orbital fleet hero.
Appearance & Outfit: slicked-back dark-teal hair in a tight tactical ponytail, sharp ice-blue eyes with cyan neural temple implants, white-and-teal commander coat over black armored flight suit, cybernetic left leg. Incorporate signature faction design cues: sleek white-outer teal-lined tactical naval coat over black-and-cyan armored undersuit, silver-and-cyan cybernetic prosthetic limb with glowing circular joint cores, tactical holsters and precision sci-fi weaponry.
Equipment & Pose: standing firmly with a sleek tactical command sidearm holstered and right hand gesturing across a glowing cyan ship-vector projection. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 발레리아 스턴]
Convert the character "발레리아 스턴" from the previous full-body illustration (or reference style: 강화인간 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (slicked-back dark-teal hair in a tight tactical ponytail, sharp ice-blue eyes with cyan neural temple implants, white-and-teal commander coat over black armored flight suit, cybernetic left leg).
Action Pose: leaning forward with her coat billowing, thrusting a glowing cyan fleet-vector baton ahead. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C03] 루시안 베일 — 함대 방진 통제관 (Phalanx Admiral)
- 분류: 지휘형 · 초능력자 (남성) · 함대전 특화 · 참조 화풍: 초능력자 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 초능력자 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (초능력자 · 남성 기준).
Subject: 루시안 베일 (함대 방진 통제관 (Phalanx Admiral)), an elite male orbital fleet hero.
Appearance & Outfit: neatly combed obsidian hair with silver streaks at the temples, piercing amethyst eyes, ornate white-and-royal-violet admiral longcoat with double gold star epaulettes and luminous purple orbital halo. Incorporate signature faction design cues: white-outer and royal-violet-inner naval officer longcoat with gold star epaulettes and silver aiguillettes, dark obsidian armored undersuit with subtle cyan seam glow, violet orbital psionic rings and levitating crystal shards.
Equipment & Pose: standing tall with a sleek ceremonial violet command scepter-baton in his right hand and a geometric fleet-formation sigil floating in his left. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 루시안 베일]
Convert the character "루시안 베일" from the previous full-body illustration (or reference style: 초능력자 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (neatly combed obsidian hair with silver streaks at the temples, piercing amethyst eyes, ornate white-and-royal-violet admiral longcoat with double gold star epaulettes and luminous purple orbital halo).
Action Pose: raising a glowing violet command baton as a shimmering hexagonal purple phalanx emblem appears around him. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C04] 소피아 렌 — 항모 항공단장 (Air Wing Commander)
- 분류: 지휘형 · 복합형 (여성) · 함대전 특화 · 참조 화풍: 복합형 · 여성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 복합형 · 여성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (복합형 · 여성 기준).
Subject: 소피아 렌 (항모 항공단장 (Air Wing Commander)), an elite female orbital fleet hero.
Appearance & Outfit: long flowing silver-lavender hair swept to one side with holographic violet halo rings, bright violet eyes, tailored white-and-violet carrier commander coat with chrome-violet cybernetic right arm. Incorporate signature faction design cues: white-and-violet naval coat with gold shoulder insignia, sleek silver cybernetic arm and leg with glowing magenta-violet energy nodes, holographic violet halo rings and shimmering psionic crystal energy weapon.
Equipment & Pose: extending her left hand beneath a vivid orbiting purple squadron formation crystal while holding a sleek neural flight-baton in her cybernetic right hand. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 소피아 렌]
Convert the character "소피아 렌" from the previous full-body illustration (or reference style: 복합형 · 여성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (long flowing silver-lavender hair swept to one side with holographic violet halo rings, bright violet eyes, tailored white-and-violet carrier commander coat with chrome-violet cybernetic right arm).
Action Pose: commanding aloft with a glowing magenta-violet flight baton as tiny luminous crystal wings swirl behind her. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```

### [C05] 빅터 칼리온 — 기함 총사령관 (Flagship Overlord)
- 분류: 지휘형 · 강화인간 (남성) · 함대전 특화 · 참조 화풍: 강화인간 · 남성 기준

#### 반실사 2D 애니 스탠딩 프롬프트
```text
[ChatGPT 반실사 2D 애니 스탠딩 프롬프트 — 참조 이미지: 강화인간 · 남성 기준]
Create a full-body sci-fi naval character illustration matching the exact semi-realistic 2D anime art style, crisp clean lineart, painterly cel-shading, cool rim lighting, and color harmony of the attached reference image (강화인간 · 남성 기준).
Subject: 빅터 칼리온 (기함 총사령관 (Flagship Overlord)), an elite male orbital fleet hero.
Appearance & Outfit: commanding silver-streaked dark hair, sharp angular face with subtle glowing cyan neural temple band, heavy black-and-white flagship admiral greatcoat with crimson inner lining and dark chrome cybernetic left arm. Incorporate signature faction design cues: heavy charcoal gunmetal tactical exo-armor with crimson-lined waist coat tails, articulated cybernetic mechanical limb with glowing cyan actuators, cyber-optic temple visor, rugged military sci-fi hardware.
Equipment & Pose: standing in a commanding posture with his cybernetic left hand resting on a tactical belt holster and right hand raised beside a glowing cyan flagship fleet map. Confident full-body standing composition from head to boots, centered with slight heroic low-angle perspective.
Background: Spacious sci-fi starship orbital hangar deck with tall angled steel struts, glowing cool cyan strip lights, reflective polished metallic floor, and a panoramic viewport showing allied warships floating above a sunlit blue planet horizon.
Technical: 3:4 vertical portrait aspect ratio, ultra-clean anime game character key art, sharp facial features, rich fabric and metal rendering, zero text, no UI overlays, no watermarks.
```

#### 선내전투용 데포르메 / SD 변환 프롬프트
```text
[ChatGPT 선내전투용 데포르메 / SD 캐릭터 변환 프롬프트 — 빅터 칼리온]
Convert the character "빅터 칼리온" from the previous full-body illustration (or reference style: 강화인간 · 남성 기준) into a deformed SD / chibi 2D tactical game unit sprite for spaceship interior boarding combat.
Proportion & Style: 2.2-head chibi proportion, crisp bold dark-navy outer lineart for high readability when scaled down on a ship cutaway floorplan, clean anime cel-shading with bright specular highlights.
Preserved Identity: Keep the exact hairstyle, hair color, eyes, coat/armor color accents, cybernetic parts, and energy glow palette (commanding silver-streaked dark hair, sharp angular face with subtle glowing cyan neural temple band, heavy black-and-white flagship admiral greatcoat with crimson inner lining and dark chrome cybernetic left arm).
Action Pose: standing tall with greatcoat flaring out, raising a glowing cyan-and-gold fleet command signal holo-banner. Dynamic 3/4 side-view combat stance facing right, full body including boots clearly visible, grounded shadow directly beneath feet.
Background & Output: Isolated on a pure flat solid white background (#FFFFFF) with no hangar scenery, no floor tiles, no text, no badges, and no extra props so it can be cleanly extracted into an in-game boarding sprite sheet. 1:1 square aspect ratio.
```
