# MapViewer 프로토타입

도로명주소 전자지도를 OpenGL 4.6으로 표출하는 3D 지도 뷰어의 골격형 프로토타입.
설계는 [`CLAUDE.md`](CLAUDE.md), 단계별 계획은 `C:\Users\colum\.claude\plans\claude-md-reactive-cat.md` 참조.

## 빌드 환경 (Windows 11)

- Visual Studio 18 Community (도구 셋 14.50 이상)
- CMake 3.25 이상 (개발 환경: 4.2.3, generator `Visual Studio 18 2026`)
- vcpkg manifest 모드, `VCPKG_ROOT` 환경 변수 (예: `D:\vcpkg`)

## 빌드 / 실행 / 테스트

```pwsh
$env:VCPKG_ROOT = "D:\vcpkg"
cmake --preset msvc-x64-debug
cmake --build --preset msvc-x64-debug
ctest --preset msvc-x64-debug
.\build\debug\Debug\mapviewer.exe   # 작업 디렉터리 = 저장소 루트
```

## 조작

| 키/마우스 | 동작 |
|---|---|
| `W A S D` | 카메라 평면 이동(전·좌·후·우) |
| `Q / E` | 카메라 하강 / 상승 |
| 화살표 | 카메라 yaw / pitch |
| 우클릭 + 드래그 | 마우스 룩 |
| `[` / `]` | 화각 1°씩 −60..+60 조정 |
| 좌클릭 | 건물 피킹 (정보 패널 갱신) |
| `Shift` (이동 키와 동시) | 4× 빠른 이동 |
| `Esc` | 종료 |
| ImGui CLI | `goto LAT LON`, `teleport X Y [Z]`, `fov N`, `load_obj NAME`, `undo`, `redo`, `trail on|off` |

## 디렉터리 구조

```
260502_Map_Viewer/
├── CMakeLists.txt / CMakePresets.json / vcpkg.json   # 빌드
├── assets/
│   ├── config/{settings.json, object_palette.json}    # 환경설정 + 오브젝트 팔레트
│   └── samples/                                       # 자동 생성 자산 (LocationLog CSV 등)
├── docs/                                              # 향후 다이어그램/노트
├── src/
│   ├── main.cpp                                       # 진입점, 모든 모듈 등록
│   ├── core/                                          # MessageBus + ModuleBase + MainModule
│   ├── geo/                                           # UTM-K 변환, AABB, 광선 피킹
│   ├── render/                                        # OpenGL 추상화 + Camera
│   └── modules/{render,data,object,config,input,
│                state,log,tts,ui,sound,locationlog}/
└── tests/{test_messagebus,test_coord,test_picking}.cpp
```

## 모듈 구성과 메시지 버스

`MessageBus`는 타입 키 기반 pub/sub. 발행 시 등록된 모듈의 락프리 큐(`moodycamel::BlockingConcurrentQueue<AnyMessage>`)에 enqueue. 각 모듈은 자체 워커 스레드(렌더 모듈은 메인 스레드)에서 큐를 폴링.

주요 시그널은 `src/core/Messages.h`에 정의. `MessageHeader`의 `parent_event_id`가 0이 아닌 메시지는 StateModule이 발행한 *replay* 메시지로 간주되어 다시 기록되지 않는다.

## 좌표계

- 공간 단위: GRS80 UTM-K (EPSG:5179) 미터.  Central Meridian 127.5°, Lat Of Origin 38°, False (E,N) = (1 000 000, 2 000 000), Scale 0.9996.
- WGS84 lat/lon ↔ EPSG:5179 변환은 GeographicLib `TransverseMercator`로 처리(`src/geo/CoordTransform.*`). ITRF2000 ↔ WGS84 datum 차이는 cm 수준이라 무시.

## 로그

`%LOCALAPPDATA%\MapViewer\logs\session-<id>.jsonl`에 모든 주요 메시지가 JSONL로 기록된다 (`LogModule`). OTel OTLP exporter는 후속 작업.

## 구현 진행 상황 (계획 vs 코드)

| 단계 | 항목 | 상태 |
|---|---|---|
| 1 | CMake/vcpkg + GLFW 윈도우 | 완료 |
| 2 | MessageBus + ModuleBase | 완료 (테스트 3 통과) |
| 3 | UTM-K 좌표 변환 | 완료 (테스트 5 통과) |
| 4 | GL 4.6 + 카메라 + 그리드 + 입력 | 완료 |
| 5 | ConfigModule + ObjectModule(팔레트) | 완료 |
| 6 | DataModule (샘플 타일 hard-coded) | 완료 — PostGIS 연결은 추후 |
| 7 | 건물(10m extrude) + 도로(quad strip) | 완료 |
| 8 | 카메라 충돌 감지 + 자동 보정 | 완료 |
| 9 | ImGui CLI + UiModule + 토스트 | 완료 |
| 10 | 좌클릭 피킹 → 정보 패널 | 완료 (테스트 3 통과) |
| 11 | OBJ 로드 + 풍선 인스턴스 | 골격 (octahedron mesh, tinyobjloader 추후) |
| 12 | 위치 로그 CSV 재생 + 트레일 | 완료 (자동 샘플 생성) |
| 13 | UNDO/REDO 명령 스택 | 완료 (Camera, FoV, Object) |
| 14 | LogModule JSONL 출력 | 완료 |
| 15 | TTS / Sound NoOp 스텁 | 완료 |
| 16 | 통합 시나리오 + README | 본 문서 |

## 알려진 한계 / 후속 작업

- **PostgreSQL/PostGIS 라이브 연결**: `DataModule`에 인터페이스만 있고 sample tile만 발행. `libpqxx` 추가 후 bbox 기반 PostGIS 쿼리로 전환 예정.
- **OBJ 파서**: 현재는 octahedron 풍선. `tinyobjloader` + Win32 파일 다이얼로그 통합 필요.
- **TTS / 사운드**: 인터페이스만. SAPI(`<sapi.h>`) + miniaudio 통합은 후속 단계.
- **OTel OTLP exporter**: LogModule이 JSONL로만 출력. opentelemetry-cpp의 OTLP exporter로 확장 가능.
- **DirectX 12 백엔드**: `IRenderer` 추상화는 아직 OpenGL 단일 구현. `render/dx12/` 추가로 확장 가능.
- **카메라 화각 −60..+60° 해석**: 본 프로토타입에서는 `effective_fov = clamp(60 + delta, 1, 119)`로 해석 (`src/render/Camera.h`).
