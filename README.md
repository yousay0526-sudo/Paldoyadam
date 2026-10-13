# 증강: 팔도야담 — C 네이티브 버전

**C11 + SDL2로 실행하는 픽셀 스토리 RPG 시제품입니다.** 전투·퀘스트·상점·대화·화면 렌더링을 C로 구현했습니다. HTML, JavaScript, 브라우저 또는 WebView를 실행하지 않습니다.

![C 게임 실행 화면](docs/native-village.png)

## 0.4 — 프롤로그와 지형

사용자가 제공한 세 장을 귀향길 → 허기와 갈증 → 물에 비친 늑대 순서로 연결하고, 마지막에 10년 뒤 마을로 전환합니다. 원본 비율을 유지하고 그림 전체를 대화창 위에 표시합니다.

![프롤로그](docs/native-prologue.png)

집·담장·울타리·우물·바위·숲에 충돌 영역을 추가했습니다. 발밑 기준으로 막히며 벽을 따라 이동합니다. 목적지를 탭하면 열린 길로 돌아갑니다. 적의 추격과 돌진 스킬에도 같은 지형 판정을 적용하고, 벽 너머 공격·상호작용을 차단합니다. 지붕과 나뭇가지 뒤에서는 캐릭터가 가려지고 플레이어의 위치는 희미하게 보입니다.

마을 입구, 상점, 흔적, 약재 회수 장소, 보스까지 이동 가능한지 자동 검사합니다. 충돌 좌표와 개발용 확인법은 [지형 안내](docs/TERRAIN.md)에 있습니다.

## 지금 플레이할 수 있는 내용

귀향길 프롤로그 → 마을 이장에게 의뢰 → 흔적 두 곳 조사 → 변이한 멧돼지 3마리 처치 → 약재 꾸러미 회수와 약방 전달 → 이장에게 보고 → 멧돼지 백정전 → 첫 증강 선택 → 윤겸의 서신.

- 대장간: 단련한 환도와 누비 갑옷. 구입 즉시 장착합니다.
- 약방: 탕약과 지혈산. 체력 회복·출혈 해제가 실제 전투에 적용됩니다.
- 주막: 국밥과 휴식으로 체력을 회복합니다.
- 가방·여정 기록·목적지 화살표·소지금·한글 대화·재도전이 포함됩니다.
- 하나의 비대칭 맵을 카메라로 이동합니다. 집·나무를 거울 반사하거나 반복하지 않습니다.
- 화면이 달라지면 시야가 달라집니다. 오브젝트 크기와 종횡비를 화면에 맞춰 늘리지 않습니다.
- 패배해도 같은 실행 중 조사·장비·물품 진행은 유지됩니다. **프로그램을 종료하거나 처음으로 돌아가면 새 여정이 시작됩니다. 영구 저장은 아직 없습니다.**

## Ubuntu / Linux에서 실행

Ubuntu 24.04 기준 설치 및 빌드:

```bash
sudo apt-get install build-essential cmake libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/paldo-yadam
```

명령은 압축을 풀어 생기는 `paldo-yadam-c/` 폴더에서 실행합니다.

PNG·한글 폰트 로더는 포함되어 있습니다. SDL2_image, SDL2_ttf를 별도로 설치할 필요가 없습니다. 실행 파일 옆의 `assets/` 폴더가 필요하며, CMake 빌드가 자동으로 복사합니다. 다른 위치의 파일을 쓰려면 `./build/paldo-yadam --assets /absolute/path/assets`를 사용합니다.

## Windows / macOS

CMake와 C11 컴파일러, SDL2 개발 패키지를 설치한 뒤 같은 CMake 프로젝트를 빌드합니다.

- Windows: Visual Studio 2022의 C++ 데스크톱 개발 도구(C 컴파일러 포함)와 vcpkg의 `sdl2:x64-windows`를 사용할 수 있습니다. `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`를 지정하세요. 실행 위치에 `SDL2.dll`도 필요합니다.
- macOS: `brew install cmake sdl2` 후 위 CMake 명령을 사용합니다.

Linux의 빌드와 렌더링을 확인했습니다. **Windows와 macOS에서는 아직 실행 검증하지 않았습니다.**

## Android

`android/`는 같은 C 게임을 Android NDK로 빌드하기 위한 프로젝트입니다. Java 파일 한 개는 SDL 앱 진입점이며 게임 로직은 모두 `src/*.c`에 있습니다.

```bash
python3 scripts/prepare_android.py
```

그다음 Android Studio에서 **`android/` 폴더**를 엽니다. 자세한 SDK·NDK 구성과 APK 생성 방법은 [Android 빌드 안내](docs/ANDROID.md)를 참고하세요.

**이번 작업에서는 Android SDK/NDK가 없어 APK 빌드·실기기 실행을 검증하지 못했습니다.** Android 프로젝트와 터치 입력은 포함되어 있으며, PC 실행 파일을 휴대폰에서 직접 실행할 수는 없습니다.

## 조작

| 동작 | 마우스·터치 | 키보드 |
| --- | --- | --- |
| 이동 | 화면을 끌거나 목적지 탭 | WASD / 방향키 |
| 기본 공격 | 가까운 적 자동 공격 | 동일 |
| 액티브 | 갈라베기 버튼 | Space |
| 대화·조사·줍기 | 가까이에서 상호작용 버튼 | E |
| 가방 | 우측 상단 가방 | I |
| 여정 기록 | 좌측 상단 퀘스트 | J |
| 탕약 / 지혈산 | 하단 빠른 사용 | 1 / 2 |
| 전체화면 | 화면 버튼 | F11 |
| 일시정지·메뉴 닫기 | 정지·닫기 버튼 | Escape |
| 대화 진행 | 계속·확인 버튼 | Enter |

기록 창에서는 위·아래 버튼 또는 마우스 휠로 목록을 이동합니다. 세로 화면에서는 회전 안내를 띄우고 진행을 멈춥니다.

## 소스 구조

| 파일 | 역할 |
| --- | --- |
| `src/game.h`, `src/game.c` | SDL에 의존하지 않는 게임 상태·전투·퀘스트·상점 |
| `src/draw.h`, `src/draw.c` | 이미지·한글 폰트·카메라·가림 효과·컷신 렌더링 |
| `src/world.h`, `src/world.c` | 지형 다각형·발밑 충돌·길찾기·공격 시야 판정 |
| `src/main.c` | 창, 터치·키보드, 대화, 메뉴, 게임 루프 |
| `tests/test_game.c`, `tests/test_world.c` | 퀘스트·거래·전투·이동·충돌·증강 검증 |
| `assets/` | 기존 PNG 그래픽과 한글 폰트 |
| `android/` | Gradle·NDK·SDL 앱 진입점 |
| `scripts/prepare_android.py` | 공식 SDL 소스 준비 및 체크섬 확인 |

로직만 검사할 때는 SDL 설치 없이 빌드할 수 있습니다.

```bash
cmake -S . -B build-core -DPALDO_BUILD_GAME=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

실제 C 렌더러를 디스플레이 없이 점검할 수도 있습니다.

```bash
./build/paldo-yadam --smoke frame.bmp --scene shop --size 844x390
```

장면: `title`, `village`, `shop`, `bag`, `dialogue`, `journal`, `augment`, `boss`, `recover`, `prologue1`~`prologue5`, `roof`, `canopy`, `collision`. 이 옵션은 테스트용으로 상태를 준비하고 화면을 저장한 뒤 종료합니다.

## 확인된 범위

- GNU C 13 / C11, SDL 2.30 런타임으로 네이티브 게임 컴파일 및 CMake 빌드.
- 246개 조건 검사: 기존 게임 흐름 110개 + 지형·진입로·퀘스트 장소 왕복·돌진·벽 너머 공격·적 우회 이동 136개.
- 640×360 및 844×390 프롤로그 화면과 지붕·나뭇가지 가림 렌더링 확인.
- 실제 SDL 소프트웨어 렌더러로 마을·보스·상점·가방·대화·여정·증강·타이틀 화면 생성.
- 한글 폰트와 PNG 그래픽이 외부 네트워크 없이 로드됨.

첫 장의 시제품입니다. 팔도 전체, 백랑·신악의 뱀 전투, 전설 무기, 방향별 걷기 애니메이션, 영구 저장은 다음 단계입니다. 0.4에서는 프롤로그 연출과 지형 이동을 보완했습니다.

라이브러리와 폰트의 출처·라이선스는 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 기록했습니다.
