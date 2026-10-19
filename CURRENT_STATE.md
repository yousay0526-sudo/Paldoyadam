# CURRENT_STATE.md — 팔도야담 개발 상태

최종 갱신: 2026-10-19

## 기준
- GitHub 저장소: `yousay0526-sudo/Paldoyadam`
- 현재 버전: `0.8.0` (C11/SDL2 기반 게임 및 Emscripten 웹 빌드)
- 정본: `src/*.c`, `src/*.h`; 주요 문서: `README.md`, `docs/CONTROLS.md`, `docs/VALIDATION.md`, `docs/WEB.md`

## 이번 작업: 모바일 화면 비율 왜곡
- iPhone 등에서 브라우저 UI 높이가 달라질 때 게임 캔버스 화면이 비균등하게 확대되는 현상에 대응.
- `src/main.c`: 캔버스 CSS 크기와 SDL 렌더 백버퍼 크기를 렌더링 프레임마다 대조·동기화.
- `web/shell.html`: `object-fit: contain` 및 `ResizeObserver`로 뷰포트 변화 대응.
- `tests/browser_smoke.cjs`: 모바일 가로 화면 높이 변경 전후에 캔버스 종횡비 일치 검사 추가.
- PR #3을 `main`에 병합 (squash 커밋 `013c59b`).
- GitHub Actions 실행 `39221610671`: 네이티브 검사, WebAssembly 빌드, Pages 배포 모두 성공.


## 별도 작업 브랜치: 마을 배경 선명도 보정 후보
- 브랜치: `graphics/village-clarity-20261019` (병합/배포 전).
- `src/draw.c`에서 마을 텍스처 로딩 시 명도 기반 경계 대비를 소폭 보정하며, 원본 PNG 파일·맵 크기·충돌 및 가림 좌표는 그대로 유지한다.
- `tests/test_hero_render.c`에 마을 크기 및 최근접 샘플링 확인을 추가했다.
- 로컬 SDL2 환경이 없어 현재 단계에서는 전체 게임 실행 및 모바일 시각 검증을 수행하지 않았다. PR CI와 실기기 전후 비교 필요.
- 혈흔처럼 보이는 공격 에셋 수정은 이 브랜치와 별개로 진행되므로 이 작업에서 변경하지 않았다.
- 사용자 리뷰 및 병합·배포 승인 대기.

## 확인되지 않은 부분과 다음 작업
- iPhone Safari 및 실제 Android 기기의 주소창/안전 영역/회전 시 비율은 실기기에서 확인 필요.
- `tests/browser_smoke.cjs` 브라우저 회귀 테스트는 CI 자동 실행 대상이 아니므로 실행 검증이 남아 있음.
- 사용자가 기기에서 비율 정상 표시 여부 확인 후, 필요한 경우 모바일 렌더링만 후속 보정.
- 다른 채팅의 전투·그래픽·게임 설정을 이번 변경에서 수정하지 않음.
