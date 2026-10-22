# C 소스로 개발하고 브라우저에서 테스트하기

원본은 `src/*.c`, `src/*.h`입니다. 웹 게임은 이 C 코드를 Emscripten으로 컴파일한 결과이며, 전투와 퀘스트를 JavaScript로 별도 구현하지 않습니다. PC용 CMake 빌드와 Android NDK 프로젝트도 같은 게임 코드를 사용합니다.

## 회사에서 한 번만 설정

1. GitHub 게임 저장소에서 **Settings → Pages**를 엽니다.
2. **Build and deployment → Source**를 **GitHub Actions**로 선택합니다.
3. **Actions → Build and publish browser game → Run workflow → main → Run workflow**를 누릅니다.
4. `build`, `deploy`가 완료되면 실행 결과의 `github-pages` 링크를 엽니다. Settings → Pages에도 게임 주소가 표시됩니다.

`yousay0526-sudo/Paldoyadam` 저장소의 기본 Pages 주소는 `https://yousay0526-sudo.github.io/Paldoyadam/`입니다. Pages 설정과 첫 배포가 성공한 뒤에만 열립니다.

Pages를 설정하지 않았을 때는 배포를 건너뛰고 Actions 요약에 안내를 남깁니다. 게임 빌드와 다운로드 파일 생성은 진행됩니다. 회사 PC에서 GitHub에 접속되더라도 `github.io` 주소나 WebAssembly가 별도로 허용되어야 합니다.

## 매번 수정할 때

1. GitHub에서 수정할 C 파일을 열고 연필 아이콘(Edit this file)을 누릅니다. 여러 파일을 편집하려면 저장소 화면에서 `.` 키로 웹 편집기를 열 수 있습니다.
2. 파일을 수정하고 **Commit changes**로 저장소에 반영합니다. 웹 편집기에서는 **Commit & Push**를 사용합니다.
3. Actions에서 새 실행이 녹색으로 완료될 때까지 기다립니다.
4. 게임 페이지를 새로고침합니다. 예전 화면이 남아 있으면 `Ctrl + Shift + R`로 다시 불러옵니다.

웹 편집기에는 컴파일러가 없습니다. 컴파일은 GitHub Actions의 Linux 서버가 수행합니다. `main`에 반영된 변경은 자동 배포하고, Pull Request의 변경은 빌드만 검사합니다.

| 바꾸고 싶은 것 | 주로 수정할 파일 |
| --- | --- |
| 전투·퀘스트·보상·상점 규칙 | `src/game.c`, `src/game.h` |
| 컷신 대사·대화·버튼 | `src/main.c` |
| 벽·집·나무 충돌과 길찾기 | `src/world.c` |
| 게임 화면을 그리는 방식 | `src/draw.c` |
| 이미지·폰트 | `assets/` |

## 링크 대신 HTML 파일로 실행

Actions에서 완료된 실행을 열고 **Artifacts → paldo-browser**를 다운로드합니다. 압축을 풀면:

- `offline/index.html`: 코드·이미지·폰트를 포함한 단일 HTML. Chrome/Edge에서 파일로 열 수 있습니다.
- `web/index.html`, `web/index.js`, `web/index.wasm`, `web/index.data`: 웹서버에 함께 올리는 파일입니다. 일부 파일만 옮기지 마세요.

단일 HTML도 브라우저의 WebAssembly 실행 허용이 필요합니다. 용량은 수십 MB이며 새 버전을 쓰려면 새 빌드 결과를 다운로드해야 합니다. 파일을 편집해도 C 원본이 바뀌지는 않습니다.

## 집 PC에서 직접 웹 빌드

공식 [Emscripten SDK 설치 안내](https://emscripten.org/docs/getting_started/downloads.html)에 따라 SDK **3.1.74**를 설치하고 활성화합니다. GitHub Actions도 이 버전을 사용합니다.

Linux/macOS SDK 폴더:

```bash
./emsdk install 3.1.74
./emsdk activate 3.1.74
source ./emsdk_env.sh
```

Windows는 SDK의 `emsdk.bat install 3.1.74`, `emsdk.bat activate 3.1.74`를 실행한 뒤 `emsdk_env.bat` 환경을 불러온 명령 프롬프트를 사용합니다. 아래 명령은 게임 프로젝트 폴더에서 실행합니다.

```bash
python scripts/build_web.py
python scripts/build_web.py --single-file --output dist/offline
```

서버용 결과를 로컬에서 실행하려면:

```bash
python -m http.server 8000 --directory dist/web
```

`http://localhost:8000`을 엽니다. 단일 파일은 `dist/offline/index.html`을 브라우저로 바로 엽니다. Emscripten의 SDL2 포트를 사용하므로 웹 빌드에 Windows용 SDL2.dll 또는 vcpkg는 필요하지 않습니다.

## 테스트 범위와 제한

- 공통 C 프레임을 브라우저의 화면 갱신 콜백에서 실행하며, C의 동기식 무한 반복으로 브라우저를 점유하지 않습니다.
- 창 크기와 픽셀 밀도를 반영합니다. 화면 비율에 맞춰 인물·건물의 종횡비를 늘리지 않습니다.
- 화면을 세로로 돌리면 기존 회전 안내가 표시되고 게임 진행이 멈춥니다.
- 진행 저장·기기간 동기화는 아직 없습니다. 웹 페이지 새로고침 또는 앱 종료 시 처음부터 시작합니다.
- 웹에서 동작해도 Windows 실행 파일·Android APK의 기기별 성능과 입력은 별도로 검사해야 합니다.
- `--test-hooks`는 자동 입력 검사에 쓰는 읽기 전용 상태 조회 함수를 추가합니다. 기본 배포에는 포함되지 않습니다.

자세한 실제 실행 기록은 [검증 기록](VALIDATION.md)을 참고하세요.


## 정지 장비 조합 개발자 도구

웹 빌드의 `equipment-lab/`에서 독립적으로 연다. 게임의 훈련장/인벤토리·저장 상태에는 연결하지 않았다. 단일 HTML 게임도 기존처럼 독립 파일이며 이 도구 자체는 별도 폴더의 모듈/PNG를 사용하므로 HTTP 서버가 필요하다. [사용법·범위](EQUIPMENT_LAYER_LAB.md).
