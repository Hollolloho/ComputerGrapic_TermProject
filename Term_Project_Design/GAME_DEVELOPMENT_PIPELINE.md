# 게임 제작 파이프라인 개발서 v2.1

갱신일: 2026-10-07. 이야기 기준은 마리와 에드윈의 과거 구호 기록 은폐를 다룬 v2.1 초안이다. 기본 구현 순서와 완료 체크는 유지한다. 실행 기준은 [전체 문서 시작점](docs/game/README.md)이다.

## 구현 순서

1. [실행·도형·한글](docs/game/tasks/00_BOOTSTRAP.md)
2. [이동·자전·부모·확대](docs/game/tasks/01_CAMERA.md)
3. [필수 공전 상세보기](docs/game/tasks/02_INSPECTION.md)
4. [방·조사·대화·수첩](docs/game/tasks/03_GAMEPLAY.md)
5. [잔향·안개·흡입·추론](docs/game/tasks/04_ECHO_FOG.md)
6. [검증·제출](docs/game/tasks/05_PACKAGE.md)
7. 기본 완료 후 [선택 확장](docs/game/tasks/06_OPTIONAL.md)

**공전·부모를 게임 전체 완성 뒤로 미루지 않는다.** 작은 큐브로 카메라 5가지를 먼저 검증하고, 이후 같은 기능으로 과거 공간을 탐색한다.

## 기능별 설계

| 문서 | 용도 |
|---|---|
| [재검토 보고서](docs/game/00_REVIEW.md) | 과도한 범위·문서 간 충돌 수정 |
| [구현 목표](docs/game/01_SCOPE.md) | 2명·2~3시간·2~3주에 맞춘 상한 |
| [카메라 명세](docs/game/02_REQUIRED_CAMERA.md) | 5가지 필수 정의와 합격 확인 |
| [첫 퍼즐](docs/game/03_STORY_AND_PUZZLE.md) | 실제 사실·증언·증거·정답 |
| [잔향과 입력](docs/game/04_ECHO_AND_INPUT.md) | 상태와 복귀·연출 계약 |
| [최소 기술 구조](docs/game/05_TECHNICAL_STRUCTURE.md) | 적은 파일·고정 데이터·리소스 |
| [일정과 축소 규칙](docs/game/06_SCHEDULE_AND_CUTS.md) | 3주 기준과 2주 대응 |
| [검증·발표](docs/game/07_ACCEPTANCE.md) | 실제 실행과 패키징 기준 |

이전 긴 원문은 [재검토 전 개발서](docs/archive/2026-10-02/GAME_DEVELOPMENT_PIPELINE.md)에 보존했다. 문서는 세분화하지만 제품 코드는 필요할 때만 파일을 나눈다. 빈 모듈·엔진·편집기를 먼저 만들지 않는다.
