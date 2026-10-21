# CURRENT_STATE.md — 팔도야담 개발 상태

최종 갱신: 2026-10-21

## 기준
- GitHub 저장소: `yousay0526-sudo/Paldoyadam`
- 현재 버전: `0.8.0` (C11/SDL2 기반 게임 및 Emscripten 웹 빌드)
- 정본: `src/*.c`, `src/*.h`; 주요 문서: `README.md`, `docs/CONTROLS.md`, `docs/VALIDATION.md`, `docs/WEB.md`

## 이번 작업: 감사 F3 — 세로 회전 안내 중 키보드 단축키 차단 · PR 검토 대기
- 기준 main `906619e` (F4 PR #17 병합, 사용자 승인으로 병합·Pages 배포 진행). 브랜치 `claude/project-thread-mm2qsg`.
- `src/main.c` `handle_events`: 세로 화면(폭 ≤ 높이)에서는 키 입력 중 F11(전체화면)·F3(충돌선 표시)만 처리하고 나머지 단축키는 무시한다. 회전 안내 뒤에서 탕약·지혈산 소모, 대사·서장 진행, 메뉴 열기, 수련장 진입, 공격·무공 요청이 일어나지 않는다. 창 종료·포커스 상실·크기 변경·백그라운드 이벤트와 키 해제 처리는 그대로이며, 가로로 돌아오면 단축키가 다시 동작한다. 터치·마우스는 기존 가로 검사를 유지한다.
- `tests/test_controls.c`: 세로 상태에서 Space/Enter/E/F/1/2/I/J/K/Z/Q/R/G/T/Esc/F8을 눌러도 단계·체력·물품·약재·공격·쿨타임·예약이 변하지 않는지, F3은 동작하는지, 가로 복귀 후 1 키가 다시 동작하는지 검사 추가. 수정 전 코드에서 실패를 확인했다.
- Linux / SDL 2.30.0 dummy: 전체 CTest **9/9 통과**(조작 832 조건), ASan+UBSan 전체 **9/9 통과**. 브라우저·외부 키보드를 연결한 실기기 미검증.
- 감사 입력 묶음(F2·F4·F3) 완료. 남은 감사 항목 F1·F5·F6·R은 미착수. 버전 변경 없음. GAME_BIBLE.md 없음.

## 이전 작업: 감사 F4 — 해제 이벤트에만 있는 드래그를 목적지 탭으로 오인하지 않게 수정 (PR #17, main `906619e`에 병합)
- 기준 main `c47ff0a` (F2 PR #16 병합, 사용자 승인으로 병합·Pages 배포 진행). 브랜치 `claude/project-thread-mm2qsg`.
- `src/main.c` `pointer_up`: 이동 포인터를 놓을 때 공격 포인터와 같이 `pointer_move()`로 마지막 좌표를 먼저 반영한다. 중간 motion 없이 놓은 지점까지 8픽셀을 넘게 움직였으면 끌기로 보고 목적지를 정하지 않는다. 8픽셀 기준, 끌기 이동, 일반 탭, 공격 포인터 처리는 변경하지 않았다.
- `tests/test_controls.c`: motion 없이 80px·9px 이동 후 해제(목적지 없음), 6px 이동 후 해제(탭 유지) 검사 추가. 수정 전 코드에서 실패를 확인했다.
- Linux / SDL 2.30.0 dummy: 전체 CTest **9/9 통과**(조작 764 조건), ASan+UBSan 전체 **9/9 통과**. 브라우저·실기기 미검증.
- 남은 감사 항목: F3(세로 화면 단축키 차단)는 승인 후 진행. F1·F5·F6·R 항목 미착수. 버전 변경 없음. GAME_BIBLE.md 없음.

## 이전 작업: 감사 F2 — Z 길게 누르기가 예약한 무공을 덮어쓰지 않게 수정 (PR #16, main `c47ff0a`에 병합)
- 기준 main `b3d6efa` (PR #15 8방향 정지·걷기까지 반영됨). 브랜치 `claude/project-thread-mm2qsg`. GPT 상세 감사(기준 `45d636f`)의 F1~F6은 현재 main에서도 모두 유효함을 확인했다.
- `src/main.c` `combat_tick`: Z 유지에 의한 자동 반복 요청은 `queued_move`에 무공이 예약돼 있으면 보내지 않는다. 새 Z 누름·Q/R/G/T·스와이프의 마지막 유효 입력 규칙, 단일 예약, 쿨타임, `game.c` 전투 로직, 공격 버튼 길게 누르기(스와이프 시 반복 중단)는 변경하지 않았다.
- `tests/test_controls.c`: Z 유지 → 횡베기 → 갈라베기 예약 → Z 계속 유지에서 예약이 유지되고 갈라베기가 실제 발동하는지, 예약이 없을 때 Z 반복이 계속 기본 공격을 이어가는지 검사 추가. 수정 전 코드에서는 이 검사가 실패함을 확인했다.
- Linux / SDL 2.30.0 dummy 드라이버: 전체 CMake/CTest **9/9 통과**(조작 761 조건), ASan+UBSan 전체 빌드 **9/9 통과**. 브라우저·실기기는 미검증.
- 남은 감사 항목: F4(이동 포인터 해제 판별), F3(세로 화면 단축키 차단)을 같은 입력 담당으로 순서대로 진행 예정, 각각 승인 후. F1·F5·F6 및 R 항목은 미착수. main 병합·배포·버전 변경 없음. GAME_BIBLE.md 없음.

## 이전 작업: 승인된 정지·걷기 8방향 구현 (main `b3d6efa`에 반영됨)
- 코드 커밋 `a2eb53a`의 [CI 39386598981](https://github.com/yousay0526-sudo/Paldoyadam/actions/runs/39386598981): Linux CMake/CTest **9/9**, Emscripten 3.1.74 일반 웹·단일 HTML 빌드 성공. Pages 준비와 deploy skipped. 아래 로컬 결과와 별도로 확인했으며 문서 결과 추가 후속 커밋의 최종 CI 상태는 PR에서 확인한다.
- 기준 main `07f39936cddc56c5091fa0b6a973b5e765789b5a`, 독립 브랜치 `art/hero-eight-direction-reference-20261020`. 사용자가 기준 원화와 정지·걷기 구현 단계를 승인해 기존 [PR #15](https://github.com/yousay0526-sudo/Paldoyadam/pull/15)를 갱신했다. main 병합·Pages 배포·버전 변경 없음.
- 승인 기준에서 정지 8포즈를 분리하고 내장 ImageGen으로 방향별 8프레임 걷기 8장을 신규 제작. `art/hero/directions/walk/`에 실제 원본·전체 프롬프트·북쪽 여백 교정 프롬프트를 보존한다. 오른손 환도/빈 왼손/왼쪽 칼집·붉은 천 허리띠 유지, 혈흔 없음. 반대 방향을 반전해 만들지 않았다.
- 실행 PNG는 `assets/warrior-idle-8dir-v1.png` 및 `warrior-walk-{n,ne,e,se,s,sw,w,nw}-v1.png` 9장, 각 768×384 RGBA/4×2/192px 셀. `scripts/prepare_direction_assets.py`와 `art/hero/directions/preparation.json`에 준비 절차·SHA-256·알파·선택 영역·발 기준점 `(96,176)`을 보존한다. 72포즈/실제 알파/셀 여백 4px 이상 검사 통과.
- `src/draw.c/h`: 기존 `Hero.dir`에서 N/NE/E/SE/S/SW/W/NW를 선택. 정지·걷기는 반전 없이 표시하고 마지막 이동 입력 방향을 유지한다. 상향은 등, 대각선은 해당 시점이다. `src/game.c/h`, `src/main.c`, 이동 거리 기반 주기·속도·충돌, NPC/퀘스트/맵과 공격/기술 원화·타이밍·판정·입력 버퍼·콤보·전진/후퇴 변경 없음.
- Windows Zig/SDL 전체 CMake/CTest **9/9 통과**, 조건 검사 합계 **444,287**. 렌더링 438,545, SDL 입력 747, UI 1,835 및 기존 게임/지형/수련/전투/피드백 통과. 실제 터치 드래그·목적지 이동 8방향과 정지/메뉴 후 방향 유지 추가 검사. 훈련 3개 크기 및 전후 8장면 성공; 시간 고정 평타·3개 기술 장면 픽셀 일치. 정지 장면의 변경은 캐릭터와 가림 표시 범위 안으로 제한된다. 북동쪽 걷기 검 끝을 포함하도록 가림 폭을 56으로 맞췄으며 기존 지붕·나무 폴리곤은 유지했다. 상세는 `docs/VALIDATION.md`.
- [SDL 정지](docs/hero-eight-direction-idle.png), [걷기](docs/hero-eight-direction-walk.gif), [게임 전후/가림](docs/hero-eight-direction-gameplay.png), [표시 계약](docs/HERO_DIRECTIONS.md). 실제 3D 전환이 아닌 방향별 2D 원화다.
- 남은 범위: 평타·4무공은 기존 좌우 반전 표시여서 공격 중 무기 손/시점이 전환될 수 있다. 공격 8방향은 다음 독립 단계. 이번 변경의 브라우저 실행·실제 iPhone/Android/APK·모바일 성능은 미검증. GAME_BIBLE.md 없음.

## 이전 작업: 오른손 고정·8방향 기준 원화 검토 (구현 전 기록)
- 기준 main `07f39936cddc56c5091fa0b6a973b5e765789b5a`, 독립 브랜치 `art/hero-eight-direction-reference-20261020`. PR #14는 사용자 승인으로 병합·배포됐으며 현재 공개 게임은 기술별 원화를 사용한다. 이 단계는 그 다음에 요청한 방향 표현의 기준 원화다.
- 실제 코드 확인: `Hero.dir` 및 이동/충돌은 이미 2D 벡터로 상하·대각선을 처리한다. 표시의 `face/attack_face`만 좌우 두 값이고 `gfx_hero`에서 수평 반전하므로 무기 든 손까지 바뀐다. 방향별 새 원화와 표시 선택이 필요하며 현재 맵을 3D로 재구성할 필요는 없다.
- 내장 ImageGen으로 승인 주인공을 참조해 8방향 정지 기준 원화 제작. 원본·초기 전체 프롬프트·북서 방향 오른손 교정 프롬프트는 `art/hero/directions/`에 보존한다. 해부학적 오른손 칼/빈 왼손/왼쪽 칼집, 앞·뒤·옆·대각선을 시각 검토했다. 붉은 허리띠 유지, 혈흔 없음.
- 실제 파일은 1254×1254 RGBA (1536×1536 요청과 구분), 알파 0..255, 완전 투명 픽셀 약 69.83%. 밝은 배경 합성 검토로 불투명 배경과 색 번짐을 확인했다. 균일 분할/발 기준점 보정 전의 참고 시트이며 실행용 아틀라스가 아니다.
- `docs/HERO_DIRECTIONS.md`에 기존 코드 근거와 단계별 적용 범위를 정리했다. 게임 코드·기존 실행 에셋·전투/입력/맵·배포 설정은 변경하지 않았다. 새 게임 기능/실행 검증은 미수행이며 기존 테스트 결과를 이번 결과로 재사용하지 않는다.
- 다음: 기준 원화 승인 후 정지·걷기 8방향 시트와 표시 연결, 해당 이동 단계 검토 후 평타·4무공의 8방향/오른손 확장. 현재 게임은 여전히 기존 좌우 반전 표시다. 새 단계의 병합/배포 없음. GAME_BIBLE.md 없음.

## 이전 작업: 기술별 6프레임 원화 제작·적용
- 시작 main `45d636f70ad6f0b6b43246a3f6842abcd753dc4a`, 독립 브랜치 `graphics/technique-sprites-20261020`. PR 생성 중 main에 들어온 PR #13 `81467764210357a27eaef25374a4fa46570922b5`를 반영해 피격·히트스톱을 보존하고 재검증했다. 참조 채팅 원화 접근 실패 후 이 채팅의 신규 제작 승인을 따랐다.
- 내장 ImageGen으로 승인 주인공 원화와 기존 평타를 참조해 갈라베기·횡베기·파고들기·물러베기 원화를 각각 제작. `art/hero/techniques/`에 생성 원본 4장·전체 프롬프트·분리/발 기준점 기록을 보존한다. 붉은 허리띠 유지, 새 원화에 혈흔 없음.
- `scripts/prepare_technique_assets.py`: 실제 RGBA 알파 확인, 셀 경계를 넘는 검/옷자락을 포함해 포즈 전체를 분리하고 3×2/576×384 실행 PNG로 재배치. 공통 발 기준점 `(116,176)`/192, 왼쪽 원화·오른쪽 반전. 물러베기의 반대 방향 준비 포즈 1장만 실행본에서 반전, master 보존.
- `src/draw.c/h`: `Technique`별 별도 아틀라스 로딩·검증·해제·표시. 이미 그려진 자세를 회전시키던 임시 기울기 제거. 평타 텍스처·발 기준점 유지. `src/game.c`는 파고들기 표시 키만 `0,0,1,2,3,4,5,5`로 수정해 여섯 포즈를 사용하고 타격 그림을 0.24초에 표시한다. 0.64초 동작·0.24초 피해·이동/후퇴·입력 버퍼·명중 콤보·장비·NPC·퀘스트·맵 로직 유지.
- 최종 로컬 검사: CMake/CTest 핵심 **5/5 통과**. Windows Zig/SDL 직접 빌드한 게임·지형·수련장·전투·히트스톱·조작·UI·영웅 렌더링 8종, 총 **441,441개 조건 통과**. 새 시트의 피격 표시·색/알파/블렌드 상태 복원도 확인. 훈련 3종 해상도와 전후 8장면 렌더링 통과. 시간 고정 비교에서 평타·상점·지붕/수목 가림은 픽셀 일치. 24포즈/알파/여백과 실제 게임 비교 이미지를 검토했다. 자세한 범위는 `docs/VALIDATION.md` 및 `docs/ART_DIRECTION.md`.
- 코드 커밋 `ada1d4a`의 [CI 39353122484](https://github.com/yousay0526-sudo/Paldoyadam/actions/runs/39353122484) 성공: Linux 전체 CMake/CTest **9종**, Emscripten 일반/단일 HTML 빌드 통과. Pages 준비·deploy skipped. 결과만 추가하는 문서 후속 커밋의 CI는 PR에서 확인한다.
- 남은 검증: 웹 브라우저 실행 및 실제 iPhone/Android/APK 표시·조작. 기존 `test_controls.c` signed/unsigned 경고 1건 유지. `GAME_BIBLE.md` 없음. main 병합·자동 병합·Pages 배포·버전 변경 없음.
- 리뷰 [PR #14](https://github.com/yousay0526-sudo/Paldoyadam/pull/14). 피격/히트스톱의 기능과 검사를 보존했으며 이번 PR의 최신 main 대비 변경은 기술 원화/표시/제작·검증 기록뿐이다.

## 이번 작업: 전투 고도화 2차 — 피격 시각 반응·히트스톱
- 기준 main `45d636f70ad6f0b6b43246a3f6842abcd753dc4a`. 브랜치 `feat/combat-hit-feedback-pass2`. 시작 및 구현 후 확인 시 main 동일, 열린 PR 없음.
- `src/game.c/h`: 실제 피해에만 짧은 정지(일반 35ms / 갈라베기 55ms / 바람만 20ms / 플레이어 피격 45ms)와 0.12초 피격 표시. 동시 타깃·바람 추가 피해는 최대 정지 하나를 공유한다. 정지 중 게임 시간은 함께 멈추고 부분 프레임 시간은 보존한다.
- `src/draw.c/h`, `src/main.c`: 원래 그림의 투명 영역을 따르는 밝은 피격 표시·접촉 불꽃, 훈련장 정지/피격 ms 진단. 입력과 UI는 계속 처리하고 마지막 유효 예약을 보존한다. 메뉴·포커스·회전·훈련 복귀·회복 시 표시와 정지를 초기화한다.
- 물리 좌표·이동/충돌·넉백·경직·공격 취소·원화·장비/증강 수치·정식 버전은 변경하지 않음. 히트스톱 동안 AI와 콤보/예약/무적/출혈/쿨타임도 함께 멈추므로 실제 시계상 진행은 그만큼 늦어진다.
- Windows Zig 0.14.1 / SDL 2.32.10: 8개 테스트 프로그램 총 **440,691개 조건 통과**. 실제 시간 피드백 159, 기존 게임 528/지형 142/수련장 1,578/방향 전투 753, SDL 입력 643, UI 1,835, 렌더링 435,053. 훈련/명중 3종 해상도와 피격·상점 화면 출력 및 시각 확인. 상세 검증·한계는 `docs/VALIDATION.md`.
- 리뷰 [PR #13](https://github.com/yousay0526-sudo/Paldoyadam/pull/13). 코드/문서 커밋 `676d7fe`의 [CI 39350445058](https://github.com/yousay0526-sudo/Paldoyadam/actions/runs/39350445058): native CTest 9종과 Emscripten 일반/단일 HTML 빌드 성공, Pages 준비·deploy skipped. 결과 기록만 추가한 문서 후속 커밋의 최종 CI는 PR에서 확인한다. 이번 2차의 main 병합·배포·버전 변경은 사용자 검토와 승인 대기. GAME_BIBLE.md 없음. 실제 모바일 체감과 브라우저 실행·APK, 경직/넉백은 후속 검증/설계 대상.

## 승인 후 배포 완료 — 전투 고도화 1차 (2026-10-20)
- [PR #12](https://github.com/yousay0526-sudo/Paldoyadam/pull/12)를 사용자 배포 요청에 따라 main에 병합했다. 커밋 `45d636f70ad6f0b6b43246a3f6842abcd753dc4a`.
- [실행 39340986967](https://github.com/yousay0526-sudo/Paldoyadam/actions/runs/39340986967): native 8종·웹/단일 HTML·Pages 게시 성공. [플레이](https://yousay0526-sudo.github.io/Paldoyadam/)의 HTML/JS/Wasm/data HTTP 200 확인. 파일 게시 2026-10-20 08:22:50 KST. 정식 버전 0.8.0 유지.
- 이전 채팅의 로컬 상태 기록과 PR의 배포 완료 기록을 이번 문서에 반영했다. 아래의 1차 승인/배포 대기 표현은 당시 구현 기록이다.

## 이번 작업: 전투 고도화 1차 — 공격 명중 판정 및 콤보 엔진
- 기준: 최신 main `e6ffd0f7b01ef56a12ca24b12a20836663d75508` (수련장 PR #11 병합 포함). 시작 시 열린 PR 없음. 브랜치 `feat/combat-hit-combo-pass1`.
- `src/game.c/h`: 기본 공격·4무공의 고정 공격 방향, 각도·거리·LOS 판정. 전진 검 궤적의 접촉과 후퇴 전 타격 위치 유지. 입력 연결과 명중 콤보를 분리해 미스는 보너스를 초기화하고, 명중 타격 사이 1.05초·직전 동작 중복·최대 3연계를 검사한다. 마지막 유효 입력 한 칸, 쿨타임·기존 피해·장비·증강·이동/충돌은 유지.
- `src/main.c`, `src/draw.c/h`: 수련장에 입력/명중 수를 구분 표시하고 기존 공격 안내에 예약·재사용·방향 불명·미스 표시. 기존 절차적 검 궤적을 실제 판정 방향·각도·사거리·타격 위치에 맞춤. 새로운 모션 원화나 조작법은 추가하지 않음.
- `tests/test_combat.c`와 기존 게임·수련장·입력·UI·렌더링 검사 보완. Windows Zig 0.14.1 / SDL 2.32.10에서 7개 검사 프로그램 총 **206,987개 조건 통과**. 실제 게임 훈련 3종 해상도, 기본 공격·4무공·상점 렌더링도 통과. 상세 범위와 환경 문제 복구는 `docs/VALIDATION.md` 참고.
- 리뷰 PR [#12](https://github.com/yousay0526-sudo/Paldoyadam/pull/12). 코드/문서 커밋 `36482d4`의 [CI 39340247387](https://github.com/yousay0526-sudo/Paldoyadam/actions/runs/39340247387) 성공: Linux CMake/CTest 8종 및 Emscripten 일반/단일 HTML 빌드 통과, Pages 준비·deploy skipped. 결과 기록만 추가한 후속 문서 커밋의 최종 CI는 PR에서 확인한다. 로컬 CMake·웹 브라우저 및 실제 iPhone/Android/APK 실행은 이번 단계에서 미검증.
- `GAME_BIBLE.md` 없음. 세계관·정식 버전 0.8.0·배포 워크플로·에셋 변경 없음. main 병합·배포·버전 변경은 수행하지 않음.
- 다음 단계(피격·히트스톱), 경직/넉백·새 스킬·회전 무공·활 차지는 미구현이며 사용자 승인 대기.

아래 항목은 각 이전 작업 당시의 기록이다. PR #10과 #11은 이번 작업 기준 main에 이미 병합되어 있다.

## 이번 작업: 숨겨진 허수아비 콤보 수련장
- 기준: 초기 main `3accab5`, PR 생성 중 추가된 마을 원화 리메이크 main `432defd`를 작업 브랜치에 동기화. 브랜치 `feat/hidden-combo-training`.
- `src/game.c/h`: 독립 훈련 상태와 원래 Game 스냅샷. 기존 빈터 지형·실제 공격/무공 판정 재사용, 고정 무적 허수아비·타격/미스/피해·예약/쿨타임 진단. 적대 AI·보상·드롭·퀘스트·소모품 사용 차단.
- `src/main.c`, `src/draw.c`: 플레이 중 HUD 제목 7회 빠른 탭 또는 F8로 진입. 일반 HUD에는 진입 표시 없음. 훈련에서만 종료 버튼·진단판 표시, 허수아비를 기본 도형으로 렌더링. 공격 버튼의 실제 연속 스와이프 및 무공 배치 유지.
- 종료 시 원래 월드/위치/퀘스트/체력/적/물품/배치/남은 쿨타임 복원. 잔여 공격·콤보·예약·누름·목적지는 취소. 입퇴장 시 이동 키 해제 대기. 메뉴·회전·크기 변경·포커스 상실에서 입력 초기화.
- `tests/test_training.c` 및 CMake 검사 추가, SDL 입력·UI·브라우저 검사 확장. 최신 main 동기화 후 Windows 네이티브 113,973개 조건 및 훈련 3종 해상도/상점 렌더링 통과. Emscripten 웹 빌드 및 Chrome 데스크톱·모바일 DPR2에서 숨은 진입/3연계/회전 취소/복귀 통과. 기존 전체화면 검사는 초기 main에서도 타임아웃이 재현돼 명시적으로 제외했으며 미해결로 기록. 리뷰 PR [#11](https://github.com/yousay0526-sudo/Paldoyadam/pull/11), CI 결과는 PR에 기록.
- 코드 커밋 `ad95923`의 [CI 39249545231](https://github.com/yousay0526-sudo/Paldoyadam/actions/runs/39249545231) 성공: Linux CMake/CTest 7종 및 웹/단일 HTML 빌드 통과, deploy skipped. 최신 main 동기화 후 로컬 Chrome 데스크톱/모바일 검사도 재통과(기존 전체화면 항목 명시적 제외).
- `GAME_BIBLE.md`는 저장소에 없어 생성하지 않음. 기존 세계관·스토리·조작법·정식 버전·배포 설정 변경 없음.
- 실제 iPhone/Android 기기에서의 조작감은 사용자 확인 필요. main 병합·Pages 배포·버전 변경은 별도 승인 대기.

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

## 이전 작업: 모바일 전방향 콤보 · 공격 버튼 배치 (기록 당시 PR 검증 대기)
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
