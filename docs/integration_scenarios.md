# 통합 테스트 시나리오 (프로토타입 v0.1)

본 문서는 골격형 프로토타입에서 사람이 따라 실행해 볼 수 있는 통합 시나리오와, 자동화된 단위 테스트 매핑을 정리한다. CLAUDE.md의 "통합 테스트 시나리오" 요구를 충족한다.

## 자동화 단위 테스트

| 테스트 | 대상 | 통과 기준 |
|---|---|---|
| `test_messagebus` | `MessageBus` 발행/구독, 멀티스레드 정확성, header 자동 기입 | 4 publisher × 4 subscriber × 10 000 msg 누락 0, event_id 단조 |
| `test_coord` | EPSG:5179 ↔ WGS84 왕복, 원점 일치, 시청 위치 추정 | 왕복 lat/lon ≤ 1e-7°, 시청 EPSG:5179 ≈ (953 900, 1 952 030) ± 50 m |
| `test_picking` | AABB-광선 교차, 폴리라인 거리 | 정상/실패 분기, 거리 0 검증 |

```
ctest --preset msvc-x64-debug --output-on-failure
```

## 사람이 실행하는 시나리오

### S1. 시동/셧다운
1. `mapviewer.exe` 실행.
2. 검은 배경 + 흰색 그리드 + 시청 인근 6개 빌딩(파랑) + 3개 도로(노랑) + 좌상단 카메라 패널 + 하단 CLI 표시.
3. `Esc` 누르면 즉시 종료, 콘솔에 GL/leak 경고 없어야 함.

### S2. 카메라 + 화각
1. `WASD` 평면 이동, `Q/E` 고도, 화살표 yaw/pitch.
2. `[` / `]` 또는 ImGui FoV 슬라이더로 −60..+60 한 칸씩 변동. effective FoV는 60+delta(°).
3. CLI에서 `fov 30` 입력 후 Enter, 즉시 슬라이더가 30으로 갱신.

### S3. 충돌 안내
1. CLI `teleport 953905 1952030 5` (city_hall AABB 내부, 높이 5 m).
2. 자동으로 가장 가까운 면 외측으로 0.5 m 밀려나오고 stderr에 `[collision]` 한 줄 출력.
3. 우상단에 한국어 토스트 "[충돌 안내] 건물(city_hall)에서 외부로 자동 이동" 표시.
4. CLI `teleport 953900 1952030 -1` → 지면 보정 (z=0.5).

### S4. 클릭 피킹 + UNDO
1. 임의 빌딩에 좌클릭 → 우하단 *Pick info* 패널에 `kind: building`, BUL_MAN_NO/주소/용도 표시.
2. 카메라 5회 이상 이동.
3. CLI `undo` 5회 → 카메라가 단계별로 직전 위치로 복원, 좌상단 패널 값 동기화. 토스트 "상태 적용: camera_undo".
4. CLI `redo` → 다시 앞으로 이동.

### S5. OBJ 추가
1. CLI `load_obj balloon` Enter.
2. 카메라 시야 위쪽(약 130 m 고도)에 8면체 풍선 인스턴스 표시. 토스트 "오브젝트 생성: balloon".
3. `undo` → 풍선이 아직 그대로(정식 제거는 추후 작업), 토스트 "object_undo:obj-N" 발행 확인. (현재 RenderModule은 StateAppliedMsg의 object_undo를 명시적으로 처리하지 않으므로, JSONL 로그로만 확인 가능)

### S6. 위치 로그 트레일
1. 첫 실행 시 `assets/samples/location_log_sample.csv`가 자동 생성되어 있어야 한다 (시청 → 광화문, 1 Hz, 300 sec).
2. settings.json의 `location_log.enabled`가 `true`이면 LocationLogModule이 시작 시 트레일을 한 점씩 추가 — 시간 진행에 따라 초록 폴리라인이 점차 길어짐.
3. CLI `trail off` → 새 점 추가 중단. (이미 그려진 트레일은 유지)

### S7. 로그 파일 검증
1. 종료 후 `%LOCALAPPDATA%\MapViewer\logs\session-<id>.jsonl` 열기.
2. 줄마다 JSON 객체. `event_id` 단조 증가, `session_id` 동일, `kind`에 bootstrap/map_tile/collision/pick/object_added/state_applied/ui_command/log/shutdown 등 포함.
3. UNDO 시 발행된 메시지는 `parent_id != 0`로 표시되어 *replay*임이 식별 가능.

## 시나리오 vs 모듈 매핑

| 시나리오 | 관여 모듈 |
|---|---|
| S1 | Render, Config, Object, Data, Ui, Input, LocationLog, State, Log, Tts, Sound (전 모듈 시동) |
| S2 | Render, Input, State, Log |
| S3 | Render(충돌 검사), Ui(토스트), State, Log |
| S4 | Render(피킹), Input(CLI), Ui(패널), State, Log |
| S5 | Input, Object, Render, State, Log |
| S6 | LocationLog, Render(트레일), Log |
| S7 | Log (모든 메시지) |

## CLI 명령 레퍼런스

```
goto <lat> <lon>            # WGS84 좌표로 카메라 이동
teleport <x> <y> [z]        # UTM-K 미터 좌표로 카메라 이동
fov <int>                   # FoV delta(-60..+60)
load_obj <name>             # 풍선 객체 추가 요청
undo / redo                 # State 명령 스택 조작
trail on / trail off        # 위치 로그 트레일 토글
```
