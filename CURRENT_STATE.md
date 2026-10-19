# CURRENT_STATE.md — 팔도야담 개발 상태

최종 갱신: 2026-10-19

## 기준
- GitHub 저장소: `yousay0526-sudo/Paldoyadam`
- 현재 버전: `0.8.0` (C11/SDL2 기반 게임 및 Emscripten 웹 빌드)
- 정본: `src/*.c`, `src/*.h`; 주요 문서: `README.md`, `docs/CONTROLS.md`, `docs/VALIDATION.md`, `docs/WEB.md`

## 이번 작업: 승인된 마을 배경 원화 리메이크 1차 · 리뷰 대기
- 최신 원격 `main` 확인 기준: `3accab5b8a4024421a01082ef31cd2f558d55a38` (PR #6 포함).
- 별도 브랜치: `graphics/village-hires-remake-pass1-20261019-review`.
- 리뷰 PR: [#10](https://github.com/yousay0526-sudo/Paldoyadam/pull/10). 코드 커밋 `f3b37b6`의 [CI 39244411655](https://github.com/yousay0526-sudo/Paldoyadam/actions/runs/39244411655)에서 Linux 전체 네이티브 CTest와 Emscripten 웹/단일 HTML 빌드 성공, deploy 작업은 skipped. 병합하지 않았다.
- 내장 ImageGen으로 기존 배경과 승인된 주인공 원화를 참조해 기와·초가·석축·수목의 재질 묘사를 다시 제작했다. 생성 원본과 전체 프롬프트는 `art/village/`에 보존하고 `assets/village.png`에 동일 파일을 적용했다.
- 실제 생성 결과는 1536×1024 RGB 불투명 이미지다. 프롬프트의 3072×2048 요청과 달리 원본과 같은 크기이며, 확대·샤픈으로 고해상도라고 표현하지 않는다. 원화 묘사 개선을 검토하는 1차 후보다.
- `src/draw.c`에서 이전 PR #4의 로딩 시 언샵 처리를 제거했다. 최근접 샘플링, 2304×1536 월드, 1.20배 줌, 충돌·가림 좌표, 카메라·퀘스트·전투·입력은 유지했다.
- Windows / Zig 0.14.1 / SDL 2.32.10 직접 빌드 성공. 게임 528, 지형 142, 조작 317, UI 1,518, 렌더링 109,272 검사 통과. 전후 SDL 스모크 16장 성공. 로딩 픽셀 원본 일치 회귀 검사 추가.
- 배치 보조 검사: 주요 지점 16개 중 15개 이동량 0px, 대장간 지붕 1개 수직 -1px. 전체 실루엣의 픽셀 단위 동일성을 뜻하지 않는다. 기존 경계 오버레이와 게임의 지붕·나뭇가지 가림 화면을 함께 검토했다.
- [전후 원화](docs/village-remake-pass1-comparison.png), [게임 화면](docs/village-remake-pass1-gameplay.png), [충돌·가림 비교](docs/village-remake-pass1-geometry.png), [상세 기록](docs/VILLAGE_REMAKE_PASS1.md).
- 실제 iPhone/Android의 표시 품질·성능은 미검증. 이번 작업에서 병합·배포·정식 버전 변경을 수행하지 않는다. `main` 병합은 Pages 배포로 이어질 수 있으므로 각각 사용자 승인이 필요하다.
- `GAME_BIBLE.md`는 저장소에 없어 생성하지 않았다. 아래 이전 작업 상태는 당시 기록이며 이번 테스트 결과와 구분한다.

## 진행 중: 모바일 전방향 콤보 · 공격 버튼 배치 (PR 검증 대기)
- 작업 브랜치: `fix/mobile-omni-combos-main-sync` (main 반영·배포 전).
- 전투: 기술 애니메이션 초반에 들어오는 후속 방향도 단일 입력 버퍼에 보존하도록 `src/game.c` 변경. 네 방향 서로 다른 12가지 2연계 조합 검증 코드 추가.
- 화면: `src/main.c`에서 터치 공격 버튼을 화면 가장자리에서 44px씩 안쪽으로 이동. 무공·소모품과 상호작용 버튼 배치 분리.
- 그래픽: `src/draw.c/h`의 고해상도 원형 도형으로 공격 버튼 외곽선·원판 반경 일치.
- `tests/test_controls.c`, `tests/test_game.c`, `tests/test_ui.c` 수정. 실제 테스트 실행 및 브라우저/휴대폰 검증은 아직 확인 전.
- 미구현: 회전 제스처 무공, 활 당기기/차지 사격. 사용자 승인 없는 구현·배포 금지.
- `GAME_BIBLE.md`는 현재 저장소에 없어 생성하지 않음.

## 이번 작업: 모바일 화면 비율 왜곡
- iPhone 등에서 브라우저 UI 높이가 달라질 때 게임 캔버스 화면이 비균등하게 확대되는 현상에 대응.
- `src/main.c`: 캔버스 CSS 크기와 SDL 렌더 백버퍼 크기를 렌더링 프레임마다 대조·동기화.
- `web/shell.html`: `object-fit: contain` 및 `ResizeObserver`로 뷰포트 변화 대응.
- `tests/browser_smoke.cjs`: 모바일 가로 화면 높이 변경 전후에 캔버스 종횡비 일치 검사 추가.
- PR #3을 `main`에 병합 (squash 커밋 `013c59b`).
- GitHub Actions 실행 `39221610671`: 네이티브 검사, WebAssembly 빌드, Pages 배포 모두 성공.


## 이전 작업: 마을 배경 선명도 보정
- PR #4가 `0b53729`로 main에 반영되어 이번 작업 기준에 포함돼 있다. 아래는 당시 구현 및 검증 제한 기록이다.
- 당시 `src/draw.c`에서 마을 텍스처 로딩 시 명도 기반 경계 대비를 소폭 보정했으며, 이번 원화 리메이크에서는 그 처리를 제거했다.
- `tests/test_hero_render.c`에 마을 크기 및 최근접 샘플링 확인을 추가했다.
- 로컬 SDL2 환경이 없어 현재 단계에서는 전체 게임 실행 및 모바일 시각 검증을 수행하지 않았다. PR CI와 실기기 전후 비교 필요.
- 혈흔처럼 보이는 공격 에셋 수정은 이 브랜치와 별개로 진행되므로 이 작업에서 변경하지 않았다.
- 이번 원화 리메이크 후보는 별도로 사용자 리뷰 및 병합·배포 승인 대기.

## 확인되지 않은 부분과 다음 작업
- iPhone Safari 및 실제 Android 기기의 주소창/안전 영역/회전 시 비율은 실기기에서 확인 필요.
- `tests/browser_smoke.cjs` 브라우저 회귀 테스트는 CI 자동 실행 대상이 아니므로 실행 검증이 남아 있음.
- 사용자가 기기에서 비율 정상 표시 여부 확인 후, 필요한 경우 모바일 렌더링만 후속 보정.
- 다른 채팅의 전투·그래픽·게임 설정을 이번 변경에서 수정하지 않음.
