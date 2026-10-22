# Blender 장군 복장 첫 구조·화풍 시험

2026-10-22. 최신 main `04cb6c5`에서 독립한 `art/blender-hero-style-study-20261022` 작업이다. 사용자 승인 범위는 기존 성인 비율의 장군 복장으로 정지와 짧은 걷기를 시험해 보는 단계다. 게임 원화·실행 코드·판정·입력·NPC·퀘스트·맵을 교체하지 않는다. 장비 조합 PR #40은 별도 검토 상태로 보존한다.

## 결과와 판단

편집 가능한 [hero-front-study.blend](../art/blender-hero-study/hero-front-study.blend)를 만들었다. 실제 성인 인체 메시, 대략적인 입체 갑옷/옷자락/소매/신발, 오른손 검, 붉은 천 허리띠, 19개 뼈와 16프레임 걷기 구조 시험이 들어 있다. 텍스처 5개는 파일에 포함하고 경로를 상대화했다. Blender 4.5.14 LTS에서 생성·저장·재열기를 확인했다. 타임라인 1–16을 재생하면 걷기 시험이며 첫 프레임을 정지 비교에 사용한다. 완성된 정지 애니메이션을 별도로 제작한 것은 아니다.

**현재 방식은 기존 그림을 앞면에 투사한 볼륨 초안이다.** 앞면의 색과 장식을 유지하는 가능성은 확인했지만, 이를 완성된 3D 캐릭터나 8방향 양산 공정으로 해석하면 안 된다. 얼굴/갑옷의 고유 조형과 균일한 UV 텍스처를 완성한 상태가 아니다. 옆면에서는 그림이 늘어나고, 원화에서 팔이 가렸던 옷에 손 그림이 중복된다. 뒤쪽은 단색 임시 재질이며 그림의 알파 실루엣에도 의존한다. 밑단·소매·손잡이·옷의 가림과 동작 품질 역시 추가 작업이 필요하다.

![원화와 실제 Blender 앞면](blender-style-comparison.png)

![구조와 옆면 한계](blender-volume-diagnostic.png)

![실제 Blender 걷기 시험](blender-walk-study.gif)

따라서 이 단계에서 장비 수나 방향을 늘리지 않는다. 다음 독립 단계는 실제 갑옷의 앞·옆·뒤 조형, 팔에 가려진 면의 재질 복원, 손의 검 쥠과 옷자락 변형을 보완한 뒤 다시 비교하는 것이다. 앞면 유사성이 모든 방향의 화풍 유지나 제작 효율을 증명하지 않는다. 단순 도형/회전만으로 기존 원화 품질을 재현했다고 주장하지 않는다.

## 원본과 권리

- 기존 승인 원화 `art/hero/warrior-clarity-master.png`는 변경하지 않았다.
- 앞면 장비 텍스처는 PR #40 코드 커밋 `a435327`의 장군 상의/바지/띠/검 RGBA를 catalog 위치에 그대로 놓아 복원했다. 그림을 다시 생성하거나 확대·재도색하지 않았다. 크롭 원본/출력 SHA-256과 처리 내용은 [texture-provenance.json](../art/blender-hero-study/texture-provenance.json)에 보존한다. 이 Blender 단계의 미리보기는 이미지 생성 AI로 만든 모형 사진이 아니라 실제 Blender 출력이다.
- 인체 기준: Blender 공식 [Human Base Meshes](https://www.blender.org/download/demo-files/)의 CC0 `human-base-meshes-bundle-v1.2.0.zip`. 실제 사용한 메시 이름은 `GEO-body_male_realistic`와 양쪽 눈이다. 몸은 원화 자세에 맞춰 변형한 초안이며 얼굴을 주인공의 고유 조형으로 완성하지 않았다. [배포 목록](https://download.blender.org/demo/asset-bundles/human-base-meshes/). ZIP SHA-256 `b7421350cbad8425dd1ba93c4248867a1c236cc8a3a3dc140d4c2a03144c4bad`, 입력 .blend SHA-256은 `study.json`에 기록한다. 배포 파일 자체는 저장소에 재복사하지 않는다.
- Blender 공식 포터블 4.5.14 LTS를 작업 도구 폴더에 준비했다. [공식 배포와 체크섬](https://download.blender.org/release/Blender4.5/). ZIP SHA-256 `b9533d2397ac1984db4466fb23a7a4649391cca93f6e84209f9bcc60d071c8b9`가 공식 `.sha256`과 일치했다. 실행 프로그램/ZIP은 커밋하지 않는다. 라이선스 구매·유료 플러그인·외부 3D 생성 서비스는 사용하지 않았다.

## 재현·검증

Blender 배경 실행에서는 반드시 `--disable-autoexec --python-exit-code 1`을 사용한다. 시스템에 설치하거나 레지스트리를 변경할 필요 없이 공식 포터블로 실행했다.

```text
blender --background --factory-startup --disable-autoexec --python-exit-code 1 --python scripts/build_blender_hero_study.py -- --human-bundle /absolute/path/human_base_meshes_bundle.blend --no-render
blender --background --disable-autoexec art/blender-hero-study/hero-front-study.blend --python-exit-code 1 --python scripts/verify_blender_hero_study.py
blender --background --disable-autoexec art/blender-hero-study/hero-front-study.blend --python-exit-code 1 --python scripts/render_blender_hero_study.py
python scripts/preview_blender_hero_study.py
```

- 재열기 검사: 메시 20개, 뼈 19개, 포함 이미지 5개, 16프레임 동작, 오른손 검 부위, 유한 좌표를 검사했다. 17개 시간 키의 양쪽 발 관절이 저작한 위치와 일치하는지 실제 평가된 뼈 좌표로 비교했고 최대 오차 약 `9.69e-8 m`였다. 이는 발바닥/옷 충돌·미끄러짐·자연스러운 보행 전체를 보증하지 않는다.
- 실제 저장 파일에서 CPU Cycles 12 samples로 정지/구조/옆면 768×768 RGBA 3장, 걷기 512×512 RGBA 8장을 출력했다. 빈 출력·4px 경계 잘림 없음, 걷기 8장 모두 다른 픽셀 해시. Blender의 File/Camera/Frame/RenderTime 메타데이터를 확인했다. 공개 PNG는 로컬 경로 메타데이터를 제외하고 동일 RGBA 픽셀로 저장했다. 원래 렌더는 무시되는 `build-blender-study/`에 보존한다.
- GIF는 24fps의 16프레임 동작에서 2프레임마다 추출한 8장의 660ms 리뷰다. 본게임의 이동/공격/스킬 타이밍이 아니다. `study.json`, `validation.json`, `render-validation.json`에 구조·검사·렌더 해시가 있다.
- 로컬 게임 CMake/CTest는 이번 원화 실험에서 새로 실행하지 않았다. `src/`, 본게임 `assets/`, 웹/Android 빌드와 워크플로를 변경하지 않았기 때문이다. 이전 작업의 테스트 숫자를 이번 결과로 쓰지 않는다.

초기 복구 기록: Blender 삼각 분할의 인덱스 반환 형식, 라이브러리 상대 경로 해석, 기존 인체 UV맵의 렌더 선택, 애니메이션 카메라의 옆면 위치 덮어쓰기, 저장 시 사용하지 않은 진단 재질 누락을 수정했다. 발 관절 재열기 검사에서 약 0.285m 오차를 검출해 시간 키 설정과 부모 관절 평가 순서를 바로잡았다. PNG에 Software 항목이 항상 있다고 가정했던 검사는 실제 Blender File/Camera/RenderTime 정보 검사로 교정했다. 이들을 수정한 뒤 재생성·재열기·실제 렌더와 경계 검사를 다시 실행했다.

현재는 미술 검토용 시제품이다. 완성 에셋 적용, main 병합, 자동 병합, Pages 배포, 버전 변경은 수행하지 않았다.
