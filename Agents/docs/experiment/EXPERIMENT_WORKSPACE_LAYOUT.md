# Alpha Project Workspace Layout

이 문서는 알파 단계 사용자 project 저장 규칙을 정리한다.
현재 기준의 planned layout이며, 아직 구현되지 않은 API나 자동 실행 기능을 완료된 기능으로 전제하지 않는다.

## 목적

한 project는 하나의 시뮬레이션 구성, 시나리오, 행동 정책, 실험 결과 로그를 포함한다.

핵심 원칙:

* 사용자는 project root를 직접 생성한다.
* 한 project에는 사용자가 편집하는 `scenario.json` 하나만 둔다.
* 랜덤성이 필요한 값은 `scenario.json`에 range/choices로 남긴다.
* episode 수와 seed는 `setting.json`이 소유하고, run 시작 시 deterministic하게 episode scenario를 만든다.
* 행동 정책은 project마다 수정하고 개선하는 대상이다.
* 실행 결과는 run과 episode 단위로 저장한다.
* AI 분석 결과는 해당 run의 `review/` 아래에 저장한다.

## Project Flow

1. Project 작성
   * 사용자가 `<UserProject>/setting.json`, `profile.json`, `scenario.json`, `policy/`를 만든다.

2. Scenario 작성
   * 시나리오 에디터나 agent가 `<UserProject>/scenario.json`을 생성하거나 수정한다.
   * scenario는 고정값과 range/choices를 함께 포함할 수 있다.

3. Project 실행
   * run 시작 시 setting/profile/scenario/policy snapshot을 저장한다.
   * snapshot scenario와 고정 seed로 episode별 `scenario.json`을 생성한다.
   * 실행 시점의 policy snapshot과 episode별 결과를 run 폴더에 저장한다.

4. 결과 분석 및 개선
   * run 단위 summary와 episode 단위 trace/event/action log를 분석한다.
   * AI 분석 결과나 reviewer note는 run의 `review/` 아래에 둔다.
   * 행동 정책 개선 후 같은 project에서 새 run을 만든다.

## Workspace Layout

```text
<UserProject>/
  setting.json
  profile.json
  scenario.json

  policy/
    __init__.py
    <policy_module>.py
    <policy_config>.json

  runs/
    <Run>/
      snapshot/
        setting.json
        profile.json
        scenario.json
        policy/
      summary.json
      review/

      episodes/
        <Episode>/
          scenario.json
          actions.jsonl
          events.jsonl
          trace.jsonl
          result.json
          preview.png
          captures/
```

## Folder Roles

| Path | Role |
| --- | --- |
| `<UserProject>/setting.json` | FPS, base seed, episode count, evaluation 기준 등 실행 설정을 둔다. |
| `<UserProject>/profile.json` | project에 고정된 profile을 둔다. |
| `<UserProject>/scenario.json` | 사용자가 편집하는 단일 scenario source를 둔다. |
| `<UserProject>/policy` | 해당 project에서 수정/개선되는 행동 정책 코드와 config를 둔다. |
| `<UserProject>/runs` | 실행 결과를 run 단위로 저장한다. |
| `runs/<Run>/snapshot` | 해당 run의 setting/profile/scenario/policy 입력 snapshot을 둔다. |
| `runs/<Run>/snapshot/policy` | 해당 run에서 사용한 policy snapshot을 복사해 둔다. |
| `runs/<Run>/summary.json` | run 전체 실행 시간, 통계, aggregate result를 둔다. |
| `runs/<Run>/review` | AI 분석 결과, reviewer note, 개선 후보를 둔다. |
| `runs/<Run>/episodes` | episode별 실행 입력, 로그, 결과를 둔다. |
| `episodes/<Episode>/scenario.json` | snapshot scenario와 seed로 확정한 episode scenario를 둔다. |
| `episodes/<Episode>/actions.jsonl` | 로봇 입력, 센서 관측, 위치, 행동 변경 등 action log를 둔다. |
| `episodes/<Episode>/events.jsonl` | 충돌, 장애물 감지 등 event log를 둔다. |
| `episodes/<Episode>/trace.jsonl` | 분석과 replay에 필요한 environment trace를 둔다. |
| `episodes/<Episode>/result.json` | 성공/실패, 실행 시간, 충돌 횟수 등 episode result를 둔다. |
| `episodes/<Episode>/preview.png` | 대표 장면 preview image를 둔다. |
| `episodes/<Episode>/captures` | episode 중 생성된 sensor/capture image를 둔다. |

## Management Rules

* project root는 사용자 데이터의 소유 단위다.
* 하나의 project 안에서 사용자가 편집하는 scenario source는 `scenario.json` 하나다.
* episode scenario는 run/episode artifact이며 사용자가 직접 수정하지 않는다.
* 행동 정책 변경은 `policy/`에서 관리하고, run 생성 시점의 policy snapshot을 `runs/<Run>/snapshot/policy`에 보존한다.
* run 결과와 episode 결과는 generated output이므로 repository에 직접 추가하지 않는다.
* AI 분석 결과는 run 결과의 일부로 보고 `runs/<Run>/review` 아래에 둔다.
