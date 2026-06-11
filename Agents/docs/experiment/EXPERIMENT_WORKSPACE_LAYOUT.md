# Alpha Experiment Workspace Layout

이 문서는 알파 단계 실험 구성 방식과 실험 workspace 저장 규칙을 정리한다. 현재 기준의 planned layout이며, 아직 구현되지 않은 API나 자동 실행 기능을 완료된 기능으로 전제하지 않는다.

## 목적

알파 단계 실험은 시나리오 샘플을 고정한 상태에서 행동 정책을 반복 개선하고, 실행 결과를 run/episode 단위로 비교하기 위한 구조를 사용한다.

핵심 원칙:

* 시나리오 템플릿은 실험 구성 전에 작성하고, 실험 중에는 샘플링된 scenario sequence를 고정한다.
* 행동 정책은 실험마다 수정하고 개선하는 대상이다.
* 기존 `PolicySetup`을 별도 최상위 구조로 유지하기보다, 실험별 `policy/` 안에서 Python 정책 코드와 config를 함께 관리한다.
* 실행 결과는 run과 episode 단위로 저장한다.
* AI 분석 결과는 해당 run의 `review/` 아래에 저장한다.

## Experiment Flow

1. 시나리오 템플릿 작성
   * 시나리오 에디터나 agent가 scenario template을 생성하거나 수정한다.
   * template은 시뮬레이터에서 scenario sample을 만들 때 사용한다.

2. 실험 구성 생성
   * simulator setting, profile, scenario sample sequence를 실험 단위로 고정한다.
   * scenario sample은 template에서 생성되지만, 실험 시작 후에는 같은 실험 안에서 바뀌지 않아야 한다.
   * 최초 행동 정책은 단순한 기준 정책으로 시작하고, 실험 결과를 바탕으로 개선한다.

3. 실험 실행
   * 고정된 scenario sequence에서 현재 행동 정책을 실행한다.
   * 실행 시점의 policy snapshot과 episode별 결과를 run 폴더에 저장한다.

4. 실험 분석 및 개선
   * run 단위 summary와 episode 단위 trace/event/action log를 분석한다.
   * AI 분석 결과나 reviewer note는 run의 `review/` 아래에 둔다.
   * 행동 정책 개선 후 같은 실험에서 새 run을 만든다.
   * scenario template이나 sampling rule을 바꿔야 하면 기존 실험을 덮어쓰지 않고 별도 실험으로 분리한다.

## Planned Workspace Layout

```text
%APPDATA%/OdiroSim/
  templates/
    scenarios/
      <Scenario>.template.json
    profiles/
      <Profile>.json

  experiments/
    <Experiment>/
      setting.json
      profile.json

      policy/
        __init__.py
        <policy_module>.py
        <policy_config>.json

      scenarios/
        <ScenarioSample>.json

      runs/
        <Run>/
          policy/
          summary.json
          review/

          episodes/
            <Episode>/
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
| `templates/scenarios` | Scenario template 저장 위치. 에디터와 agent가 수정할 수 있는 입력 템플릿이다. |
| `templates/profiles` | 실험 생성에 사용할 profile template 저장 위치다. |
| `experiments/<Experiment>/setting.json` | FPS, global time limit 등 실험 설정을 둔다. |
| `experiments/<Experiment>/profile.json` | 실험에 고정된 profile snapshot을 둔다. |
| `experiments/<Experiment>/policy` | 해당 실험에서 수정/개선되는 행동 정책 코드와 config를 둔다. |
| `experiments/<Experiment>/scenarios` | template에서 샘플링된 scenario sequence를 둔다. 실험 중 고정되어야 한다. |
| `experiments/<Experiment>/runs` | 실험 실행 결과를 run 단위로 저장한다. |
| `runs/<Run>/policy` | 해당 run에서 사용한 policy snapshot을 복사해 둔다. |
| `runs/<Run>/summary.json` | run 전체 실행 시간, 통계, aggregate result를 둔다. |
| `runs/<Run>/review` | AI 분석 결과, reviewer note, 개선 후보를 둔다. |
| `runs/<Run>/episodes` | episode별 실행 로그와 결과를 둔다. |
| `episodes/<Episode>/actions.jsonl` | 로봇 입력, 센서 관측, 위치, 행동 변경 등 action log를 둔다. |
| `episodes/<Episode>/events.jsonl` | 충돌, 장애물 감지 등 event log를 둔다. |
| `episodes/<Episode>/trace.jsonl` | 분석과 replay에 필요한 environment trace를 둔다. |
| `episodes/<Episode>/result.json` | 성공/실패, 실행 시간, 충돌 횟수 등 episode result를 둔다. |
| `episodes/<Episode>/preview.png` | 대표 장면 preview image를 둔다. |
| `episodes/<Episode>/captures` | episode 중 생성된 sensor/capture image를 둔다. |

## Management Rules

* `templates/`는 실험 입력 후보를 관리하는 영역이고, 특정 실험의 고정 snapshot은 `experiments/<Experiment>/` 아래에 둔다.
* 하나의 실험 안에서 `scenarios/`는 고정한다. scenario sample을 바꾸면 새 실험으로 분리한다.
* 행동 정책 변경은 `policy/`에서 관리하고, run 생성 시점의 policy snapshot을 `runs/<Run>/policy`에 보존한다.
* run 결과와 episode 결과는 generated output이므로 repository에 직접 추가하지 않는다.
* AI 분석 결과는 run 결과의 일부로 보고 `runs/<Run>/review` 아래에 둔다.
* 이 문서는 알파 단계 workspace layout 기준이며, 실제 실행 폴더나 generated output을 생성하지 않는다.
