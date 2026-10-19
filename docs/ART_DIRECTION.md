# 팔도야담 미술 기준 · 주인공 1단계

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
| `assets/warrior.png` | 기존 원본. 전령 표시에도 사용하므로 보존 |
| `scripts/prepare_hero_asset.py` | Pillow로 실행용 해상도만 변환하는 제작 도구 |

제작 방식은 내장 ImageGen이며 CLI/API 대체 경로를 사용하지 않았다.
참조는 기존 `assets/warrior.png`(인물)와 `docs/native-village.png`(세계관 색감)다.

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
