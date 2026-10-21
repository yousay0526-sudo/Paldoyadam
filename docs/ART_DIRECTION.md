# 팔도야담 미술 기준 · 주인공 1단계

## 2026-10-20 · 승인된 오른손·8방향 정지·걷기 (PR 검토 대기)

새 방향 표현의 기준은 [HERO_DIRECTIONS.md](HERO_DIRECTIONS.md)에 정리했다. `art/hero/directions/eight-direction-master.png`는 승인 주인공에서 내장 ImageGen으로 제작한 8방향 정지 참고 원화다. 해부학적 오른손에 환도, 빈 왼손, 왼쪽 허리 칼집을 유지한다. 수평 반전으로 반대 방향을 만들지 않는다. 위쪽 3명은 등과 뒤통수, 아래쪽 3명은 정면/앞 대각선, 가운데 양옆은 옆모습이다. 기존 붉은 천 허리띠·남색/청록/금 갑옷·얼굴/상투를 유지하고 혈흔을 추가하지 않았다.

생성 초안의 북서 포즈가 왼손에 칼을 들어 해당 부분의 오른손/빈손/칼집을 교정했다. 보존한 master는 교정 후 결과다. 실제 크기는 1254×1254 RGBA이며 요청한 1536×1536과 다르다. 전체 초기 프롬프트와 교정 프롬프트를 함께 보존한다. 셀 간 여백·발 기준점은 아직 실행 규격으로 정렬하지 않았다. 단순 3×3 등분이나 수평 반전으로 실행 시트를 만들지 않는다. 이 한 장은 걷기/공격 애니메이션이 아니며 게임에서 읽지 않는다. 새로 보이는 등 갑옷 묘사 등은 원화 승인 후 정본으로 삼는다.

기준 원화는 사용자 승인을 받았으며 정지·걷기 8방향을 PR #15에 구현했다. 실행본은 `assets/warrior-idle-8dir-v1.png`와 방향별 `warrior-walk-{n,ne,e,se,s,sw,w,nw}-v1.png` 8장이다. 각 768×384 RGBA/4×2/192px 셀, 발 기준점 `(96,176)`, 56×56 상자/몸 높이 약 43 논리 픽셀. `Hero.dir`의 북쪽부터 시계방향 8구간으로 선택하고 정지 방향 유지, 반전·몸통 회전 없이 표시한다. 최근접 샘플링/줌/충돌/이동 거리 주기는 유지한다.

걷기 실제 생성 원본·전체 프롬프트·북쪽 여백 교정은 `art/hero/directions/walk/`에 보존한다. 1536×1024 5장, 1774×887 3장. 준비 스크립트는 연결된 알파 실루엣과 검/옷자락을 분리하고 알파 1..4 잡점만 제거, 비율 유지/알파 축소/몸통·장화 기준점 정렬 후 4px 이상 여백을 검사한다. 방향별 원화의 손·칼집 관계를 유지하고 수평 반전/빨간색 제거/프레임 보간을 하지 않는다. `preparation.json`에 SHA-256·실크기·알파·셀/기준점 기록.

[정지·걷기 표시 계약](HERO_DIRECTIONS.md), [SDL 걷기](hero-eight-direction-walk.gif), [게임 전후](hero-eight-direction-gameplay.png). 이번 단계는 정지·걷기 결과 검토 대기다. **평타·4무공의 기존 좌우 반전은 유지**하므로 공격 중 손/시점 전환이 남아 있다. 이동 결과 승인 후 별도 공격 원화 작업이 필요하다. main 병합·배포는 수행하지 않았다.

## 2026-10-20 · 기술별 전용 원화

사용자가 원화 신규 제작을 승인했다. `art/hero/warrior-clarity-master.png`는 인물/복장 기준, `assets/warrior-attack-v1.png`는 기존 작은 게임 그림·카메라·방향 기준으로 내장 ImageGen에 함께 전달했다. 생성 결과 4장은 1536×1024 RGBA이며 변경 없이 `art/hero/techniques/*-master.png`에 보존한다. 각 `*-prompt.txt`는 실제 사용한 전체 프롬프트다.

| Technique | 실행용 에셋 | 동작 |
| --- | --- | --- |
| `TECH_CLEAVE` | `assets/warrior-cleave-v1.png` | 머리 위로 들어 올려 수직 내려베기 |
| `TECH_SWEEP` | `assets/warrior-sweep-v1.png` | 허리 높이에서 넓게 가로베기 |
| `TECH_THRUST` | `assets/warrior-thrust-v1.png` | 낮은 준비에서 전진 베기/뻗기 |
| `TECH_RETREAT` | `assets/warrior-retreat-v1.png` | 앞을 베고 몸과 무게를 뒤로 거두기 |

상투·짧은 수염·남색/청록 갑옷·금 장식·흰 깃·붉은 **천 허리띠**·장화·은색 환도를 유지했다. 추가 혈흔·피 입자 없음. 검 잔상은 은색/옅은 청색이다. 기존 평타 그림은 보존한다.

실행 PNG 4장은 모두 576×384 RGBA, 3열×2행의 192픽셀 정사각 셀이다. 원화는 일부 검과 허리띠가 명목상 셀 경계를 넘으므로 단순 등분하지 않는다. `scripts/prepare_technique_assets.py`가 각 인물의 연결된 알파 실루엣과 검/옷자락 전체를 선택하고 원래 가장자리 알파를 보존해 0.27배로 축소·재배치한다. RGB 색상으로 빨간 영역을 지우지 않는다. 공통 발 기준점은 셀 내부 `(116,176)`이며 게임의 80×80 그림 상자에 연결한다. 네 방향 기술 선택과 월드 이동은 기존 시뮬레이션이 담당한다.

왼쪽을 향한 원화를 정본으로 삼고 오른쪽 공격은 기준점도 함께 반전한다. 생성된 물러베기 1번 준비 포즈만 오른쪽을 보아 실행본에서 한 번 반전했으며 master는 수정하지 않았다. 각 셀의 비어 있는 가장자리는 최소 4픽셀, 칼끝/장화/허리띠의 잘림을 검사한다. PNG의 완전 투명 픽셀 비율과 master/실행본 SHA-256, 선택 영역과 기준점은 `art/hero/techniques/preparation.json`에 기록한다.

재현: `python scripts/prepare_technique_assets.py` (Pillow 필요). 기존 평타의 크기·발 기준점은 유지한다. 전용 원화에는 기울기가 이미 있으므로 이전 기술용 임시 몸통 회전을 적용하지 않는다. 동작 0.64초/타격 0.24초 유지, 파고들기의 표시 키만 `0,0,1,2,3,4,5,5`로 연결해 준비 포즈와 타격 그림을 일치시켰다.

[24포즈·발 기준선 리뷰](technique-sprites-review.png), [실제 게임 전후 확대 비교](technique-gameplay-comparison.png). 작은 화면의 실제 기기 표시와 사용자의 미술 검토는 남아 있다.

## 이번 단계

2026-10-16, 사용자 승인에 따라 주인공의 정지 캐릭터 그림만 개선한다.
기존 세계관과 인물 디자인을 유지한다. 이동 모션·기본 공격 모션·배경 수정은
이 단계에 포함하지 않으며, 각 단계가 끝나면 사용자의 다음 진행 답변을 기다린다.

## 주인공의 고정 요소

- 조선 장군인 성인 한국 남성. 단정한 콧수염과 짧은 턱수염, 검은 상투와 금색 비녀.
- 남색·청록 계열 갑옷, 절제된 금색 테두리와 어깨 장식, 흰 안깃.
- 붉은 허리띠·술 장식, 남색 옷자락과 장화, 아래로 내린 환도.
- 기존 성인 비율과 정면에 가까운 사선 시점, 서 있는 자세를 유지한다.
- 작은 화면에서는 흰 깃·얼굴·갑옷·붉은 허리띠·칼의 큰 형태가 먼저 읽혀야 한다.
  미세한 반복 무늬보다 윤곽과 명암을 우선한다.

## 파일과 재현

| 경로 | 용도 |
| --- | --- |
| `art/hero/warrior-clarity-master.png` | ImageGen으로 제작한 1254×1254 RGBA 원본. 수정 없이 보존 |
| `art/hero/warrior-clarity-prompt.txt` | 제작에 사용한 전체 프롬프트 |
| `assets/warrior-v2.png` | 138×138 RGBA 실행용 캐릭터. 기존 정사각 캔버스와 투명 여백 비율 유지 |
| `art/characters/warrior.png` | 기존 원본(1254×1254) 보존. 전령 표시는 `scripts/prepare_character_assets.py`로 만든 160×160 `assets/warrior.png`를 쓴다 |
| `scripts/prepare_hero_asset.py` | Pillow로 실행용 해상도만 변환하는 제작 도구 |

제작 방식은 내장 ImageGen이며 CLI/API 대체 경로를 사용하지 않았다.
참조는 기존 `art/characters/warrior.png`(인물, 당시 경로 `assets/warrior.png`)와 `docs/native-village.png`(세계관 색감)다.

실행용 파일을 다시 만들려면 Pillow가 설치된 Python에서 다음을 실행한다.

```bash
python3 scripts/prepare_hero_asset.py
```

게임은 여전히 46×46 월드 좌표 상자 안에 캐릭터를 그린다. 138픽셀 텍스처는
브라우저 최대 3배 픽셀 밀도에 맞춘 원본 정보를 제공한다. 1·2·3배 크기에서
46/92/138픽셀 실행용 이미지와 선형·최근접 샘플링을 비교한 뒤 138픽셀과
최근접 샘플링을 선택했다. 작은 화면에서 윤곽을 흐리지 않는 방향이다.
다른 NPC·적·지도 텍스처의 샘플링은 변경하지 않는다.

## 이후 단계에 전달할 기준

- 이동·공격 프레임은 이 원본의 얼굴·복장·색상·신체 비율을 기준으로 제작한다.
- 인물은 전체가 보이고 투명 배경을 유지해야 한다. 엔진이 그림자를 그리므로 원본에 바닥 그림자를 넣지 않는다.
- 발 위치를 기준으로 이동·충돌·깊이 정렬과 가림 효과를 맞춘다.
- 배경과 후속 장의 NPC도 기존 조선 마을의 차분한 흙색·녹색과 어울려야 한다.
- 배경 오브젝트 위치가 달라지면 같은 단계에서 충돌과 가림 좌표도 수정한다.
- 2장 이후의 새 NPC와 맵 제작은 별도 요청 전까지 진행하지 않는다.


## 0.7.4: directions, scale and attacks

- The approved idle/walk artwork faces screen-left. Mirror for face=+1 (right), retain the source for face=-1 (left). Attack facing is captured independently at swing start.
- WORLD_ZOOM=1.20 enlarges the complete world, including actors, houses, roofs, tree canopies and collision overlays. Keep simulation coordinates unchanged; camera view dimensions and pointer inverse transforms use the same zoom. HUD/dialogue use the original renderer scale.
- assets/warrior-attack-v1.png: transparent 576x384 atlas, six 192px square cells, 3x2, row-major anticipation / raised sword / strike / follow-through / recovery / settle. A premultiplied-alpha area reduction preserves detail and avoids dark edges.
- Swing lasts 0.42 seconds; impact at 0.14 seconds. Validate the committed target, distance and unobstructed segment at impact. Menus freeze elapsed time. Recovery/retry clear pending attacks.
- Future armor, helmet and weapon artwork must use per-action layers with shared foot roots, cell dimensions, pose timing and the same facing convention. Store item appearance separately from combat stats; align equipment to the torso/head/hand anchors of each pose. This release records that contract; equipment appearance swaps are a later feature.
- Preserve the Joseon pixel-RPG palette: topknot, navy/teal armor, gold ornaments, red sash, curved silver hwando. Follow the same palette and proportions for later episodes.


## 0.8.0: 무공 동작

기존 공격 아틀라스의 발 기준점과 좌우 규칙을 유지하며, 기술별 8개 시간 키에 6개 포즈를 연결한다. 갈라베기는 들어 올려 내리기, 횡베기는 큰 기울기와 넓은 곡선, 파고들기는 짧은 준비와 직선 검 궤적, 물러베기는 반대 기울기와 후퇴로 구분한다. 0.64초 중 0.24초에 한 번 타격하며, 메뉴에서는 동작 시간이 멈춘다. 렌더러는 기술 동작 동안 가림 판정의 검 궤적 영역을 넓힌다.

장비창은 현재 정지 캐릭터를 보여 준다. 장비 장착/해제의 수치 효과와 소유 여부는 구현했으며, 몸·갑옷·투구·무기 레이어를 실제 포즈에 겹치는 외형 변경은 이후 별도 원화가 필요한 작업이다.

## 0.8.0 후속 · 마을 배경 선명도 보정 후보

아래는 이전 PR #4의 방식이다. 다음의 승인된 원화 리메이크 1차에서는 이 언샵 처리를 제거했다.

- `assets/village.png` 원본과 월드 좌표(`WORLD_W/H`), 건물·나무·길 및 충돌/가림 폴리곤은 변경하지 않는다.
- `src/draw.c`는 마을 그림을 텍스처로 불러올 때 RGB 밝기 경계에만 약한 언샵 처리를 1회 적용한다. 밝기 변화는 최대 ±24로 제한하며 투명도와 크기는 보존한다. 원본 파일은 그대로 남는다.
- 마을 텍스처는 명시적으로 최근접 샘플링을 사용한다. 주인공·NPC·몬스터·프롤로그 그림의 로딩·샘플링은 변경하지 않는다.
- 새 원화를 만든 것이 아니며, 원본에 없는 디테일을 복원하는 기능도 아니다. 실제 휴대폰에서 더 선명하게 보이는지는 화면 비교가 필요하다.
- 병합/배포 전 시각적 리뷰 대상으로 남겨둔다.

## 2026-10-19 · 승인된 마을 배경 원화 리메이크 1차

- 이전 단계의 배경 보류 이후 사용자가 배경 리메이크 1차를 명시적으로 승인했다. 기존 `assets/village.png`를 편집 대상으로, `art/hero/warrior-clarity-master.png`를 인물과 조화되는 묘사 기준으로 사용했다. 인물을 배경에 합성하지 않았다.
- 내장 ImageGen 사용. `art/village/village-remake-pass1-master.png`는 생성 결과 그대로, `village-remake-pass1-prompt.txt`는 전체 프롬프트다. 실행용 `assets/village.png`는 master의 바이트 단위 복사본이다. 별도 필터·리샘플·샤픈·색 보정은 적용하지 않았다.
- 생성 결과는 1536×1024 RGB 불투명 이미지다. 3072×2048을 요청했지만 해당 크기로 생성되지 않았으므로 2배 해상도·복원 완료로 주장하지 않는다. 그림의 기와·초가·나무·돌·잎의 재질 구분과 선 정리를 개선한 원화 후보로 검토한다.
- 건물·굴뚝·우물·작은 제단·수레·수목·울타리·길의 구도와 조선풍 흙색/청록 수목/따뜻한 등불을 유지한다. 색과 빛은 원본보다 밝고 깨끗해졌으며, 사용자 시각 검토 항목이다.
- 1536×1024 캔버스, 월드 2304×1536, 기존 최근접 샘플링과 줌을 유지한다. 충돌과 가림 좌표·표시 알고리즘은 수정하지 않는다. 앞으로 배치나 경계가 달라지는 원화는 별도 지형 검토가 필요하다.
- `src/draw.c`는 PNG 픽셀을 직접 읽으며 이전 언샵 함수와 임시 버퍼를 제거했다. 주인공·NPC·적 원화와 애니메이션은 변경하지 않았다.
- 비교 이미지·배치 보조 검사·실행 검증·미검증 항목은 [1차 리뷰 기록](VILLAGE_REMAKE_PASS1.md)에 모았다. 다음 독립 그래픽 작업, 병합 및 배포는 사용자 승인 후 진행한다.

## 적·NPC 실행용 축소본

`art/characters/`에 원본(백정·멧돼지·전령 1254×1254, NPC 띠 2172×724)을 보존하고, `scripts/prepare_character_assets.py`가 화면 크기의 약 3배(백정 320, 멧돼지 192, 전령 160, NPC 띠 543×181)로 Lanczos(알파 사전 곱) 축소한 실행본을 `assets/`에 만든다. 이 네 텍스처만 선형 샘플링으로 그린다. 그림 자체는 바꾸지 않았다. 비교 `docs/character-sampling-review.png`(왼쪽부터 기존 1.2배, 새 1.2배, 기존 2.4배, 새 2.4배).
