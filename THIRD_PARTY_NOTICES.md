# 외부 구성요소와 에셋

## SDL2

- 프로젝트: https://github.com/libsdl-org/SDL
- 라이선스: zlib. Android 준비 스크립트가 공식 소스와 함께 LICENSE.txt를 가져옵니다.
- Android 고정 버전: 2.30.12
- 아카이브 SHA-256: `ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea`
- 데스크톱은 시스템에 설치된 SDL2 2.0.18 이상을 사용합니다.

## stb_image / stb_truetype

- 원본: https://github.com/nothings/stb
- `third_party/stb/`에 원본 헤더와 포함된 라이선스 전문을 그대로 보존했습니다.
- 제공 라이선스: MIT 또는 Public Domain 선택.
- stb_image.h SHA-256: `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`
- stb_truetype.h SHA-256: `ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab`

## 나눔명조 / 나눔붓글씨

변형하지 않은 원본 TTF를 포함합니다. SIL Open Font License 1.1이며 각 폰트의 저작권과 라이선스 전문을 함께 보존했습니다.

- `NanumMyeongjo-Bold.ttf`: https://github.com/google/fonts/tree/main/ofl/nanummyeongjo
  - SHA-256 `bc9ed8e60d93fe6db054b8fb988481b625f2eef8cb2317ad0e9834681b8fe3f3`
  - 라이선스: `assets/fonts/nanummyeongjo-OFL.txt`
- `NanumBrushScript-Regular.ttf`: https://github.com/google/fonts/tree/main/ofl/nanumbrushscript
  - SHA-256 `27ceaf578c96f594cdf07fe0181b251790acbb746a164e45c1f6473f89911a31`
  - 라이선스: `assets/fonts/nanumbrushscript-OFL.txt`

## 게임 PNG

2026-10-19 마을 배경 리메이크 1차: 기존 사용자 배경과 승인된 주인공 원화를 참조한 내장 ImageGen 생성물로 `assets/village.png`를 교체했습니다. 생성 원본과 프롬프트는 `art/village/`에 보존합니다. 이는 별도 외부 게임 원화나 새로운 에셋 라이선스의 도입이 아닙니다.

사용자와 제작한 기존 ‘증강: 팔도야담’ 시제품의 AI 생성 그래픽을 기반으로 합니다. 맵·장군·백정·변이한 멧돼지·마을 인물 PNG를 포함하며, 개선한 원화는 해당 제작 기록을 보존합니다. 외부 상용 게임에서 가져온 그래픽은 포함하지 않습니다. 이 문서는 게임 자체나 사용자 에셋에 새로운 오픈소스 라이선스를 지정하지 않습니다.

## 사용자 제공 프롤로그 이미지

2026-10-03 사용자가 다른 앱에서 제작하여 제공한 PNG 원본을 사용했습니다. 게임 내 표시를 위한 파일명만 바꾸었으며 원본 이미지 파일의 픽셀·비율·해상도는 유지했습니다.

- `1.png` → `assets/prologue/01-homeward.png`: 장군과 무사들의 귀향길
- `2.png` → `assets/prologue/02-hunger.png`: 사과와 시냇물, 허기와 갈증
- `download.png` → `assets/prologue/03-reflection.png`: 시냇물에 비친 늑대

## Gradle

Android 준비 과정에서 공식 SDL 배포본의 Gradle Wrapper를 복사합니다. Gradle은 Apache License 2.0을 사용하며 실행 스크립트에 라이선스 고지가 포함됩니다. 공식 라이선스: https://github.com/gradle/gradle/blob/master/LICENSE
