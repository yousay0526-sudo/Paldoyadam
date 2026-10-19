# CURRENT_STATE.md — 팔도야담 개발 상태

최종 갱신: 2026-10-19

## 기준
- GitHub 저장소: `yousay0526-sudo/Paldoyadam`
- 현재 버전: `0.8.0` (C11/SDL2 기반 게임 및 Emscripten 웹 빌드)
- 정본: `src/*.c`, `src/*.h`; 주요 문서: `README.md`, `docs/CONTROLS.md`, `docs/VALIDATION.md`, `docs/WEB.md`

## 진행 중: 모바일 전방향 콤보 · 공격 버튼 배치 (PR 검증 대기)
- 작업 브랜치: `feat/mobile-omnidirectional-combos` (main 반영·배포 전).
- 전투: 기술 애니메이션 초반에 들어오는 후속 방향도 단일 입력 버퍼에 보존하도록 `src/game.c` 변경. 네 방향 서로 다른 12가지 2연계 조합 검증 코드 추가.
- 화면: `src/main.c`에서 터치 공격 버튼을 화면 가장자리에서 44px씩 안쪽으로 이동. 무공·소모품과 상호작용 버튼 배치 분리.
- 그래픽: `src/draw.c/h`의 고해상도 원형 도형으로 공격 버튼 외곽선·원판 반경 일치.
- `tests/test_controls.c`, `tests/test_game.c`, `tests/test_ui.c` 수정. 실제 테스트 실행 및 브라우저/휴대폰 검증은 아직 확인 전.
- 미구현: 회전 제스처 무공, 활 당기기/차지 사격. 사용자 승인 없는 구현·배포 금지.
- `GAME_BIBLE.md`는 현재 저장소에 없어 생성하지 않음.

## 이전 작업: 모바일 화면 비율 왜곡
- iPhone 등에서 브라우저 UI 높이가 달라질 때 게임 캔버스 화면이 비균등하게 확대되는 현상에 대응.
- `src/main.c`: 캔버스 CSS 크기와 SDL 렌더 백버퍼 크기를 렌더링 프레임마다 대조·동기화.
- `web/shell.html`: `object-fit: contain` 및 `ResizeObserver`로 뷰포트 변화 대응.
- `tests/browser_smoke.cjs`: 모바일 가로 화면 높이 변경 전후에 캔버스 종횡비 일치 검사 추가.
- PR #3을 `main`에 병합 (squash 커밋 `013c59b`).
- GitHub Actions 실행 `39221610671`: 네이티브 검사, WebAssembly 빌드, Pages 배포 모두 성공.

## 확인되지 않은 부분과 다음 작업
- iPhone Safari 및 실제 Android 기기의 주소창/안전 영역/회전 시 비율은 실기기에서 확인 필요.
- `tests/browser_smoke.cjs` 브라우저 회귀 테스트는 CI 자동 실행 대상이 아니므로 실행 검증이 남아 있음.
- 사용자가 기기에서 비율 정상 표시 여부 확인 후, 필요한 경우 모바일 렌더링만 후속 보정.
- 다른 채팅의 전투·그래픽·게임 설정을 이번 변경에서 수정하지 않음.
