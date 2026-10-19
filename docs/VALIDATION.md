## 2026-10-19 · 모바일 전방향 연계와 원형 공격 버튼 — PR 검증 대기

- 변경 대상: `src/game.c`, `src/main.c`, `src/draw.c/h`, `tests/test_game.c`, `tests/test_controls.c`, `tests/test_ui.c`, 조작/상태 문서.
- 공격 동작 중 언제든 서로 다른 방향 입력을 한 칸 예약하고, 현재 동작 종료 시 실행하도록 버퍼 수명을 조정. 쿨타임과 단일 예약, 자동 반복 방지, 타격 시점의 충돌 판정은 유지.
- 모바일 전용 공격 버튼을 화면 안쪽으로 44 논리 픽셀씩 이동. 회복 물품/무공 버튼을 별도 열로 두고 작은 가로 화면의 상호작용 영역을 분리.
- 공격 버튼을 128분할 원판/외곽선으로 통일. 전투 이펙트·피 표현은 변경하지 않음.
- 추가 검증 코드: 방향별 기술 4×3=12개 서로 다른 순서의 조기 입력·연계, 터치 유지/해제 및 콤보 수치, 5종 화면 크기의 모바일/PC 버튼 겹침과 화면 경계.
- **이번 대화에서 로컬 빌드·CTest·SDL 실기기 테스트는 아직 실행하지 못했다.** 브랜치 PR의 CI 결과가 나오기 전에는 통과라고 기록하지 않는다.
- 실제 모바일 Safari/Android 터치 감각·화면 품질, 원형 경계의 렌더링은 기기별 확인이 필요.
- 사용자 승인 전 main 병합·Pages 배포 금지. 원형 회전 무공·활 차지 사격은 향후 아이디어로만 남김.

## 2026-10-19 · 모바일 브라우저 캔버스 비율 보정 (검증 대기)

- iPhone 등에서 브라우저 주소창·툴바 높이가 바뀔 때 CSS 표시 영역과 SDL 백버퍼 크기가 어긋나 화면 전체가 늘어날 수 있는 경로를 보정한다.
- `src/main.c`에서 현재 `#canvas`의 CSS 영역을 매 렌더링 프레임의 크기 계산에 반영하고, 실제 SDL 픽셀 버퍼 크기를 다시 동기화한다.
- `web/shell.html`에 `object-fit: contain`과 `ResizeObserver` 기반 브라우저 크기 변경 감지를 추가했다.
- `tests/browser_smoke.cjs`에서 가로 844×390 ↔ 844×320 크기 변경 시 캔버스 버퍼와 CSS 영역의 종횡비 일치 여부를 검사한다.
- **검증 상태 (2026-10-19):** PR #3 네이티브 CTest와 Emscripten WebAssembly 빌드가 GitHub Actions에서 성공했고, `main` 병합 및 GitHub Pages 배포까지 성공했다. `tests/browser_smoke.cjs`의 새 비율 검사는 CI에서 실행되지 않았다. iPhone Safari 실기기 화면·회전·노치 확인은 아직 필요하다.

# 검증 기록

## 2026-10-19 · 모바일 연속 스와이프

- 시작 기준: `main`의 `c37656187b3a0e445c1a9c0950f310ecb6acfdd5`(0.8.0). 기존 코드에서 이동 이벤트 직후 무공이 시작되어야 한다는 새 검사가 실패하는 것을 먼저 재현했다. 작업 종료 전 최신 `main`인 `b4af8ef4406c311864f1a0a5247db3b5a7e8166d`로 기준을 갱신했다. 그 사이 변경은 `AGENTS.md` 추가뿐이며 게임 소스·테스트는 동일했다.
- Windows, Zig 0.14.1 / Clang 19.1.7, C11, SDL 2.32.10으로 `-Wall -Wextra -Wpedantic` 빌드. 게임 실행 파일과 아래 테스트를 경고 없이 직접 빌드·실행했다. 이번 로컬 실행은 CMake/CTest나 웹 빌드 실행을 의미하지 않는다.
- 게임 검사 **455개 통과**: 기존 퀘스트·거래·회복·공격 회귀 및 연속 입력의 0.22초 예약 창, 마지막 유효 입력 한 개, 재사용 중 입력 거절, 기본 공격과 공유하는 예약, 예약 기술의 준비 시간·1회 타격, 메뉴 취소.
- 지형 검사 **142개 통과**: 기존 이동·경로·벽 충돌·파고들기·물러베기·갈라베기와 벽 너머 타격 차단.
- SDL 입력 검사 **245개 통과**: 손을 떼기 전 발동, 같은 터치의 위→아래→왼쪽 3연계, 버튼 중심을 지나지 않는 반전, 같은 방향 유지·손 떨림·모호한 대각선 차단, 완만한 방향 전환, 너무 이른 입력의 자동 재시도 방지, 예약 교체와 쿨타임 거절, 길게 누르기에서 스와이프로 전환, 해제 시 중복 발동 방지, 최종 이동이 해제에 합쳐진 경우, 기술 재배치.
- 실제 `SDL_FINGERDOWN/MOTION/UP` 이벤트로 이동·공격 손가락 분리 및 무관한 손가락 무시를 검사했다. 가방·대화·일시정지·무공·여정, 포커스 상실·백그라운드·세로 회전에서 누름/방향/예약 정리와 복귀 후 잔여 입력 미발동을 검사했다. 기존 Z/Q/R/G/T, 대화·줍기·가방 입력 검사도 유지했다.
- UI 검사 **1,088개**, 캐릭터 렌더링 검사 **108,396개** 통과. 844×390 상점의 네이티브 SDL 렌더링 스모크 테스트 통과. `git diff --check` 통과.
- 빌드 문제와 복구: 제한 실행 환경에서 Zig가 비정상 종료/파일 처리 오류를 냈다. 사용자 승인 후 동일 도구를 제한 밖에서 실행하고 로컬 빌드 명령의 `src` 헤더 경로를 보완해 위 검증을 완료했다. 이 문제로 제품 소스나 빌드 설정을 변경하지 않았다.
- 남은 검증: 실제 Android/iPhone의 터치 감각·다중 터치·회전, Android APK 및 브라우저/WebAssembly 실행은 미검증. 자동 테스트 통과를 실기기 검증으로 간주하지 않는다. 이번 작업에서 병합·배포는 수행하지 않는다.
- `CURRENT_STATE.md`와 `GAME_BIBLE.md`는 저장소에 없어 갱신하지 않았고 새로 재구성하지 않았다. 이번 결과는 이 문서와 PR에 기록하며 연속성 문서 생성은 별도 승인 후 진행한다.

## 2026-10-19 · 0.8.0 · 장비창과 스와이프 전투

- Windows에서 Clang 19.1.7 / Zig 0.14.1, C11, SDL 2.32.10으로 게임과 5개 검사 그룹을 빌드·실행했다. 게임 소스는 경고 없이 컴파일했다.
- 게임 검사: 근접 시 자동 공격 없음, 수동 공격·허공 공격, 기술 준비 후 1회 타격, 2/3연계 피해, 입력 예약과 메뉴 취소, 기술별 재사용, 배치 교환, 장비 소유·장착·해제와 거래 회귀를 통과했다.
- 지형 검사: 파고들기·물러베기·유수참의 벽 충돌을 포함해 142개 조건을 통과했다. 피해도 타격 시 시야를 확인한다.
- 실제 SDL 이벤트 및 포인터 검사: 탭, 네 방향 스와이프, 모호한 대각선 취소, 작은 손 떨림, 길게 누르기, 이동·공격 두 손가락 분리, 키보드, 포커스 상실·메뉴·세로 화면 입력 정리, 가방 탭·장비·소모품을 통과했다.
- UI 검사: 432×270, 480×306, 568×320, 844×390, 1100×650에서 장비 슬롯·탭·기술 배치 버튼이 화면 안에 있고 HUD 입력 영역이 겹치지 않는다. 기존 모든 서장·대사 높이와 텍스트 클립 회귀도 통과했다.
- 실제 SDL 렌더러에서 걷기·기본 공격·네 기술의 포즈·방향·메뉴 복귀를 1/2/3배로 검사했다. 432×270 장비창·무공 설정과 844×390 소비품·네 기술 장면을 직접 렌더링해 검토했다.
- 실제 휴대폰의 다중 터치나 Android APK 실행을 검증한 것은 아니다. 브라우저 빌드·배포는 GitHub Actions의 네이티브 검사 이후 진행한다.

## 2026-10-16 · 0.7.2 · 주인공 선명도 1단계

- 기존 디자인과 자세를 참조해 내장 ImageGen으로 투명 배경의 주인공 정지 원본 제작. 1254×1254 RGBA 원본과 전체 제작 프롬프트 보존. 실행용은 같은 캔버스를 138×138로 축소하며 원본은 변경하지 않음.
- 주인공과 기존 전령 그림을 분리. 전령은 원래 `warrior.png`, 주인공은 `warrior-v2.png`를 사용. 게임 안 표시 상자 46×46, 발 위치, 이동·충돌·피격·공격·깊이 정렬과 가림 판정은 유지.
- SDL 소프트웨어 렌더러에서 1/2/3배 픽셀 밀도와 46/92/138 해상도·선형/최근접 표시를 비교. 138픽셀과 기존 최근접 샘플링을 선택해 과도한 축소 흐림을 피함. 실제 브라우저·휴대폰 기기 검증을 뜻하지 않음.
- CMake Release 빌드 및 CTest 5개 전부 통과. 게임 145, 지형 136, 입력 22, UI 283으로 기존 586개 조건 검사 통과. 네이티브 상점 렌더링도 성공.
- 432×270, 844×390, 1100×650 마을과 844×390 지붕·나뭇가지 가림·보스·NPC 장면을 이전 실행 파일과 비교. 7개 장면 모두 바뀐 픽셀이 기존 주인공의 표시 상자 안에만 존재함을 확인.
- 생성 원본 SHA-256 일치, 실행 이미지 RGBA·투명 채널, 소스 배포 목록의 제작 원본·실행 이미지 포함 여부 확인. 실제 C 렌더링 비교는 `hero-clarity-comparison.png`, 게임 화면은 `hero-clarity-village.png`에 보존.
- 이번 단계에는 이동·기본 공격 애니메이션과 배경 수정이 포함되지 않음. 다음 단계는 사용자 확인 후 진행.

## 2026-10-16 · 0.7.1

- 사용자 첨부의 서장 05/05 넘침을 재현할 수 있는 짧은 가로 화면을 추가. 432×270, 480×306, 568×320, 599×382, 844×390, 1100×650에서 모든 서장·NPC·흔적 대사의 실제 줄바꿈 높이가 본문 영역 안에 들어가는지 검사.
- 새 SDL UI 검사 283개 통과. 1/1.25/2배 렌더링에서 긴 본문이 영역 밖에 픽셀을 그리지 않는지, 버튼 글자가 기존 클립 영역을 보존하는지 확인. 실제 이야기 문장은 잘라 숨기지 않고 16~17px로 전부 표시되는지 별도 높이 검사.
- 게임 145개, 지형 136개, SDL 입력 22개, UI 283개: 총 586개 조건 검사 통과. GCC C11 경고 옵션으로 네이티브 실행 파일과 검사 프로그램 빌드.
- 약재가 등장 직후 움직이지 않음, 0.7초 뒤 점진적인 흡수 시작, 발밑에서도 즉시 획득되지 않음, 메뉴에서 대기 시간 정지, 벽·중복 획득 차단 확인.
- 432×270부터 서장·대화·상점·가방·증강·여정·HUD를 SDL 소프트웨어 렌더링으로 확인. 특히 서장 05의 3줄 본문이 둥근 패널 안에 표시되는지 점검.
- 이번 검증은 네이티브 렌더링이며 실제 Android/iPhone 검증을 의미하지 않음. Android APK 빌드는 수행하지 않음.

## 2026-10-16 · 0.7

- GCC C11, `-Wall -Wextra -Wpedantic`로 공통 게임과 SDL 렌더러·입력 검사를 컴파일.
- 게임 흐름·자동 흡수 140개, 지형 136개, 실제 SDL 입력 22개: 총 298개 조건 검사 통과.
- 범위 안 물품의 점진적 이동, 범위 밖·벽 너머 차단, 도착 후 1회 획득, 동시 회수와 퀘스트 완료, 이동 지속, 대화·일시정지 중 정지를 확인. 기존 Space 대화·키 반복 차단·Q 스킬을 유지.
- 568×320, 640×360, 844×390, 1920×1080 SDL 소프트웨어 렌더링으로 둥근 버튼·대화창·상점·가방·여정·증강·서장·제목 화면을 점검. 좁은 증강 카드의 설명 겹침을 수정.
- 글자 영역으로 버튼의 가로·세로 중앙 정렬. 나눔명조 Bold와 나눔붓글씨 원본 TTF 및 각각의 OFL 라이선스를 포함.
- 이번 기록의 작은 화면 검증은 네이티브 렌더링이며 실제 Android/iPhone 실기기 검증을 의미하지 않음. 안전 영역 CSS는 포함했으나 노치가 있는 실기기, Android APK·Windows/macOS 실행은 미검증.

## 2026-10-14 · 0.6

- GNU C / C11, `-Wall -Wextra -Wpedantic`로 네이티브 게임과 입력 검사를 컴파일.
- 게임 흐름·줍기 126개, 지형 136개, 실제 SDL 입력 이벤트 22개: 총 284개 조건 검사 통과.
- Space 대화 진입·다음 대사·프롤로그, 키 자동 반복 무시, Q 스킬, F 및 터치 줍기, 패널이 열린 동안 잘못된 입력 차단 확인.
- 주변 NPC와 물품이 함께 있어도 줍기는 독립 동작. 같은 물품 중복 획득, 반경 밖·벽 너머·일시정지 중 회수 차단 확인.
- SDL 소프트웨어 렌더러로 640×360, 844×390, 1920×1080의 알림·대화·회수 화면 확인. 알림은 좌측 여정 아래 한 줄로 표시하며 중앙 전투 공간을 가리지 않음.
- `tests/browser_smoke.cjs`도 Space 대화·자동 반복·Q 스킬·빈 영역 F 검사를 수행하도록 갱신. 이 항목은 테스트 코드 변경 기록이며 브라우저 검사 실행 완료를 의미하지 않음.
- Windows 실행 파일·Android APK 및 실제 휴대폰 실행은 이번에도 검증하지 않음.

## 2026-10-13 · 0.5 · C 소스의 브라우저 실행

- 같은 `main.c`, `draw.c`, `game.c`, `world.c`를 Emscripten 3.1.74 / SDL2 2.30.9로 컴파일. 웹서버용 HTML·JS·Wasm·데이터와 모든 자산을 포함한 단일 HTML 생성 성공.
- Chromium 155 자동화에서 실제 키보드 입력으로 프롤로그 5개 대사, 이동, 가방·여정 열기/닫기, 이장 대화와 퀘스트 수락, 창 크기 변경, 전체화면 진입·종료 통과.
- 844×390 / 픽셀 밀도 2의 모바일 브라우저 에뮬레이션에서 터치로 시작·컷신 진행·드래그 이동 통과. 390×844 세로 전환 시 이동 정지와 가로 복귀 확인.
- 검사 중 플레이어 발 위치의 충돌 상태 확인. 기존 게임 흐름 110개·지형 및 길찾기 136개 검사도 통과.
- 배포용 웹 파일은 HTTP 서버에서 실행 확인. 단일 HTML은 `file://`로 직접 열어 실행 확인했으며 외부 HTTP 요청이 없었음. 두 배포본 모두 브라우저 오류와 테스트 전용 상태 조회 함수가 없음.
- 공통 프레임 처리로 변경한 네이티브 게임도 GCC C11로 경고 없이 빌드하고 844×390 프롤로그 렌더링 확인.
- 실제 Android/iPhone 기기의 성능·다중 터치·노치, Safari/Firefox/Edge, 회사 PC의 보안 정책은 미검증. 모바일 에뮬레이션 통과가 실제 기기 검증을 뜻하지는 않음. 진행 저장은 아직 없음.

## 2026-10-03 · 0.4

- GNU C 13.3 / C11로 네이티브 실행 파일 직접 컴파일. 새 `world.c`와 렌더러를 포함하고 경고 없이 완료.
- 기존 게임 흐름 110개 검사와 신규 지형·길찾기 136개 검사 통과. 모든 상점과 퀘스트 목표 왕복을 실제 이동으로 수행.
- 640×360, 844×390 컷신, 마을·상점 및 지붕·나뭇가지 가림 화면을 SDL 소프트웨어 렌더러로 확인.
- 신규 지형 검사와 컷신·가림 렌더링에 AddressSanitizer + UndefinedBehaviorSanitizer 적용 후 통과. `/proc` 제한 때문에 누수 검사는 제외.
- 프롤로그 PNG 세 장의 SHA-256이 사용자 첨부 원본과 일치.
- 현재 환경에서 로컬 CMake 실행 도구가 정상 작동하지 않아 이번 수정본은 GCC 직접 빌드로 검증. CMake 구성에는 `world.c`와 `world_navigation` 테스트를 추가함.
- Android APK·휴대폰·Windows/macOS 실행은 여전히 미검증.

## 2026-10-01 · 0.3

- Ubuntu 24.04 계열 환경, GNU C 13.3, C11, CMake로 전체 네이티브 게임 빌드 성공.
- SDL2 2.30 런타임과 SDL2 2.30.12 헤더 사용.
- 게임 로직 110개 검사 통과: 퀘스트 선행 조건, 조사·회수·전달·보고·보스·증강, 중복 보상 방지, 소지금, 장비 효과, 탕약·지혈산·주막, 패배 복구, 일시정지, 이동 범위.
- CTest의 `gameplay`, `native_render` 통과.
- SDL dummy 비디오/소프트웨어 렌더러에서 844×390의 타이틀·마을·상점·가방·대화·여정·증강·보스 화면을 생성·검토.
- 640×360 증강 선택 및 1100×650 마을 화면도 확인. 맵 비율과 인물 크기 고정.
- AddressSanitizer + UndefinedBehaviorSanitizer에서도 로직과 렌더링 테스트 통과. 이 실행 환경은 LeakSanitizer의 `/proc` 접근이 제한되어 누수 검사만 비활성화했으며, 메모리 누수 검사 완료를 의미하지 않음.
- Android 준비 스크립트의 SHA-256 검증·SDL 소스 준비·Gradle 실행기 복사 성공. Android XML과 Python 구문 확인.

미검증: 실제 휴대폰의 다중 터치·회전·노치·전체화면, Android APK 컴파일, Windows/macOS 실행. 테스트 화면은 C 실행 파일이 직접 렌더링한 화면이며 기존 HTML 화면 캡처가 아님.


## 0.7.4 native review (2026-10-19)

Verified with Clang 19.1.7 via official Zig 0.14.1, C11, and official SDL 2.32.10 on Windows. Compiler caches were scoped to the task temp directory. Third-party stb emits two existing unused-function warnings; application sources compile cleanly.

- Gameplay: 303 assertions, including six attack poses, windup before damage, a single impact, pause/resume, moving out of reach, recovery, equipment damage and augment regressions.
- Terrain: 136 assertions covering gates, routes, dash and walls. World simulation coordinates and collision polygons remain unchanged under the 1.20 render zoom.
- SDL controls: 26 assertions, including actual event dispatch and the inverse transform for a tapped destination after zoom.
- UI: 283 assertions across narrative lines, compact landscape screens and text clipping at 1/1.25/2x.
- Hero renderer: eight walking and six attack poses at 1/2/3x; source-facing regression, unique frames, bounds, left/right reflection, idle return and menu isolation. Pixel bounds were checked for all attack renders.
- Actual SDL game smoke scenes: village, attack3, attack6, roof and canopy at 844x390. See hero-scale-review.png and hero-attack-review.png.

The candidate is based on public main bb03a4c4a7592e0e344eefbbacaa0d485a021f45. Browser/WebAssembly build and public deployment remain pending; the workflow now runs all native test groups before compiling the web build.
