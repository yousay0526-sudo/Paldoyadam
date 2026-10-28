# 길목 이정표 원화 · 2026-10-28

사용자 요청: 탐식굴로 가는 마을 길의 닫힌 문처럼 보이는 부분을 열고, 원형 이동 표식을 조선시대 분위기의 사물로 바꾼다.

- `village-open-east-generated.png`: 내장 ImageGen으로 기존 마을 원화를 참조해 동쪽 문짝을 지우고 열린 흙길로 편집한 전체 생성 결과. 게임에는 이 전체 이미지를 덮어쓰지 않는다.
- `waypost-generated.png`: 내장 ImageGen으로 만든 투명 목제 이정표/새끼줄/등롱/밑돌 원본. 글씨는 런타임에서 목적지에 맞춰 그린다.
- `scripts/prepare_map_waypoints.ps1 -OriginalVillage <approved-original.png>` (Windows/System.Drawing): main `535a13e`의 승인 원본 해시를 확인한 뒤 x1330..1535, y545..719 안의 생성 편집만 16px 경계 혼합으로 적용한다. 이 범위 밖 픽셀은 동일하다. 이정표는 256×256 RGBA로 축소한다. 원본 입력은 출력 파일과 별도 경로여야 한다.
- 실제 생성 크기: 마을 1536×1024, 이정표 1280×1280. 저장소 입력·출력 해시는 [provenance.json](../provenance.json)에 기록한다.
- 국적/시대 고증의 학술 재현물을 주장하지 않는다. 기존 게임 미술에 맞춘 목제 길안내 소품이다.
