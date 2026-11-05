# DEVELOPMENT — 개발 기준

버전 3.0 · 2026-10-29. [STORY](../STORY/README.md)의 머더미스터리를 구현하는 문서다. 문서 정리는 게임 코드 구현 완료가 아니다.

| 문서 | 내용 |
|---|---|
| [01_SCOPE](01_SCOPE.md) | 전체 이야기와 첫 시제품 범위 |
| [02_REQUIRED_CAMERA](02_REQUIRED_CAMERA.md) | 확대·이동·자전·공전·부모 필수 |
| [03_ECHO_AND_INPUT](03_ECHO_AND_INPUT.md) | 과거 전환·가장자리 안개·흡입·입력 |
| [04_TECHNICAL_STRUCTURE](04_TECHNICAL_STRUCTURE.md) | 최소 소스 구조와 데이터 |
| [05_GAME_PATTERNS](05_GAME_PATTERNS.md) | 게임 디자인 패턴 학습과 적용 |
| [06_SCHEDULE](06_SCHEDULE.md) | 2명·각자 하루 2~3시간 계획 |
| [07_ACCEPTANCE](07_ACCEPTANCE.md) | 실제 실행 검증과 제출 |
| [08_PROTOTYPE_CASE](08_PROTOTYPE_CASE.md) | 작은 시야 퍼즐의 검증용 대사·증거 |

## 작은 구현 단계

0. [TASKS/00_BOOTSTRAP](TASKS/00_BOOTSTRAP.md)
1. [TASKS/01_CAMERA](TASKS/01_CAMERA.md)
2. [TASKS/02_INSPECTION](TASKS/02_INSPECTION.md)
3. [TASKS/03_GAMEPLAY](TASKS/03_GAMEPLAY.md)
4. [TASKS/04_ECHO_FOG](TASKS/04_ECHO_FOG.md)
5. [TASKS/05_PACKAGE](TASKS/05_PACKAGE.md)
6. [TASKS/06_OPTIONAL](TASKS/06_OPTIONAL.md)

기존 B/C/O/G/E/Q/X ID는 추적을 위해 유지한다. 작업 내용 중 옛 인물과 사건 연결은 교체하고 완료 체크는 임의로 바꾸지 않는다. 먼저 B001에서 현재 실행 상태를 확인하고 O015까지 카메라 5가지를 완성한다.

시야·책장·표식 퍼즐은 작은 기능 검증용 제안이며 STORY의 실제 살인 해답과 구분한다. 네 용의자의 상세 시간표와 유죄 단서는 아직 후속 설계다. 코드 구현에 들어가기 전에 해당 과제의 대사·배치를 확정한다.
