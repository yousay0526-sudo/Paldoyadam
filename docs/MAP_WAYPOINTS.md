# 승인된 지역별 길목 표식 · 2026-10-28

동쪽 길이 닫힌 문처럼 보이는 배경과 원형 이동 표식을 수정한 PR #47에서 사용자 검토를 반영했다. 사용자는 나무와 바위에 맞는 글씨 질감, 투박한 음각, 화살표/채색 제거, 주변 동굴과 같은 돌결을 검토한 뒤 동일한 처리를 다른 이정표에도 적용하도록 승인했다. 승인 후 추가 확인 없이 반영하고 별도 인게임 테스트를 생략하는 요청을 따른다.

## 최종 적용

| 위치 | 최종 표시 |
| --- | --- |
| 마을 북쪽 → 솔바람재 | 나뭇결에 스며든 먹 붓글씨 ‘솔바람재’와 기존 나무 기둥 |
| 마을 동쪽 → 탐식굴 | 기존 배경 등주에 부착한 먹 붓글씨 ‘탐식굴’ 방향판 |
| 솔바람재 → 마을 | 같은 재질·붓맛·여백의 ‘마을’ 나무 방향판 |
| 탐식굴 → 마을 | 주변 암벽과 같은 큰 면의 청회색 바위, 정으로 쪼아낸 투박한 ‘마을’ 음각. 채색과 화살표 없음 |

`src/draw.c/h`는 이름을 폰트로 덧그리던 함수와 바위 화살표를 제거하고 `assets/maps/waypost-boards.png`의 세 셀 및 `cave-waystone.png`를 표시한다. 글씨가 재질과 함께 그려져 별도로 튀어나오지 않는다. 나무 기둥/기존 동쪽 등주 배치, 전환 지점·판정·도착 위치, 밑돌 충돌, NPC/몬스터/의뢰/전투/조작 규칙은 이번 승인 적용에서 변경하지 않았다.

마을 PNG의 동쪽 길 부분 수정은 이 PR의 앞선 범위다. 변경 35,172픽셀, 지정 사각형 밖 0픽셀이라는 기존 기록을 유지하며 이번 재질 적용에서 배경 원본을 더 수정하지 않았다.

## 원화와 준비

[승인 이미지 시안](../art/maps/waypoints-20261028/approved-material-review.png)은 이미지 편집 검토용으로, 최종 게임 실행 화면이 아니다. [투명 에셋 원본](../art/maps/waypoints-20261028/approved-material-atlas.png), [준비 기록](../art/maps/waypoints-20261028/approved-preparation.json), [출처/해시](../art/maps/provenance.json)에 근거를 남겼다. 세 이름이 정확한지 시각 확인했고 알파 경계·셀 크기를 검사했다. 준비 스크립트는 `scripts/prepare_approved_waypoints.ps1`이다.

## 이번 검증과 승인

- Windows / Zig 0.14.1 / SDL 2.32.10: `cmake --build build-maps-host --target paldo-yadam --parallel 2` 성공. [빌드 기록](waypoint-material-build.txt).
- 사용자 요청에 따라 이번에는 로컬 게임 실행·SDL 스모크 출력·CTest·브라우저/실기기 플레이 테스트를 추가로 수행하지 않았다. 자동 CI의 기존 네이티브 검사/웹/단일 HTML 빌드·배포 워크플로는 변경하지 않는다.
- 이전 커밋 `9253a68`의 12/12 및 CI #123 성공은 이전 명조/화살표 단계의 결과이며 이번 승인 적용 결과로 대체 표시하지 않는다. 기존 [검토 중간 화면](waypoint-review.png)과 [이전 로그](waypoint-validation.txt)도 과거 단계다. 최신 원격 결과는 PR #47 및 해당 커밋의 Actions 기록으로 확인한다.
- 사용자는 이미지 승인 후 바로 적용하도록 요청했고, 최신 시안 및 다른 이정표의 동일 처리를 승인했다. 이에 따라 PR #47 main 병합과 자동 Pages 반영을 진행한다. 실제 병합 SHA/배포 결과는 [PR #47](https://github.com/yousay0526-sudo/Paldoyadam/pull/47)과 [main 배포 기록](https://github.com/yousay0526-sudo/Paldoyadam/actions/workflows/web.yml?query=branch%3Amain)에 기록한다.

실제 휴대폰 조작·브라우저 플레이·장시간 성능은 미검증이다. GAME_BIBLE.md는 없어 생성하지 않았다. 정식 버전/워크플로/새 독립 기능 변경 없음.
