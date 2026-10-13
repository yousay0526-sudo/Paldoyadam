# Android 네이티브 빌드

게임의 화면·입력·전투·대화는 C로 실행됩니다. Android에서 SDL 라이브러리와 `libmain.so`를 불러오는 작은 Java 진입점만 사용합니다. 브라우저·WebView를 사용하지 않습니다.

## 준비

- JDK 17 및 Android Studio
- Android SDK Platform 34
- Android NDK `26.3.11579264`
- Android SDK CMake `3.22.1`
- Python 3.9 이상과 첫 준비 시 인터넷 연결

프로젝트 루트에서 실행합니다.

```bash
python3 scripts/prepare_android.py
```

공식 SDL `2.30.12` 릴리스 아카이브를 다운로드하고 고정 SHA-256을 확인한 뒤 `third_party/SDL/`에 준비합니다. SDL이 제공하는 Gradle 실행 스크립트와 wrapper JAR도 `android/`에 복사합니다. 생성된 의존성과 실행 스크립트는 Git 추적에서 제외되어 있습니다. `.c`·`.h`·PNG·폰트·앱 설정은 프로젝트에 포함됩니다.

## Android Studio

1. `android/` 디렉터리를 프로젝트로 엽니다.
2. SDK Manager에서 위 SDK·NDK·CMake 버전을 설치합니다. SDK 약관은 사용자가 확인합니다.
3. Gradle Sync를 진행합니다. Gradle 8.1.1 / Android Gradle Plugin 8.1.1을 사용합니다.
4. USB 디버깅을 켠 기기를 연결하거나 x86_64 에뮬레이터를 준비합니다.
5. `app`을 선택해 Run 또는 Build APK(s)를 실행합니다.

명령줄에서는 Android SDK 경로가 설정된 상태에서:

```bash
cd android
./gradlew assembleDebug
```

Windows에서는 `gradlew.bat assembleDebug`를 사용합니다. 출력은 `android/app/build/outputs/apk/debug/app-debug.apk`입니다. 이 경로는 **빌드가 성공한 뒤 생성되는 경로**이며, 이번 배포에 APK 파일이 포함된 것은 아닙니다.

## 구성

- `arm64-v8a`: 대부분의 Android 휴대폰
- `x86_64`: 에뮬레이터
- 최소 SDK 23, 대상 SDK 34
- 가로 화면, 다중 터치(이동 중 스킬 버튼), 외부 연결 없이 에셋 로드
- CMake에서 네이티브 라이브러리의 16 KiB 페이지 정렬 옵션을 지정함
- 앱 게시용 서명 키·스토어 배포 설정은 포함하지 않음

Android SDK/NDK가 없는 제작 환경에서는 프로젝트 준비와 소스 구성까지만 확인했습니다. **APK 컴파일, 실제 휴대폰에서의 회전·노치·터치·기기별 호환성은 다음 검증 단계입니다.** 현재 구성은 개발용 시제품이며 스토어 최신 게시 정책을 충족한다고 보장하지 않습니다.
