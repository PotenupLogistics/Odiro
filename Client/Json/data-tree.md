```json
<UserProject>/
  setting.json                         # sampling, runtime, evaluation
                                       # episode_count/base_seed/generator_version 소유

  profile.json                         # robot body/drive/lidar capability
                                       # project 고정 입력

  scenario.json                        # 사용자가 편집하는 단일 scenario
                                       # corridor/obstacles/pedestrians/robot
                                       # random range/choices 포함

  policy/
    __init__.py                        # create_policy() entrypoint
    ...                                # policy가 쓰는 Python module/config

  runs/
    <RunId>/
      status.json                      # run process/status lifecycle

      snapshot/
        setting.json                   # run 시작 시 project 입력 복사
        profile.json
        scenario.json
        policy/
          __init__.py
          ...

      summary.json                     # schema: run_summary
                                       # run-level 집계. 원본 아님

      review/
        analysis_run_response_v2.json  # AI run 분석 응답 snapshot

      episodes/
        <EpisodeId>/                   # 1-based 6자리. 예: 000001
          scenario.json                # seed로 확정된 실행 입력
                                       # 사용자가 직접 편집하지 않음

          result.json                  # episode 최종 결과
          actions.jsonl                # robot policy request/response log
          events.jsonl                 # episode event log
          trace.jsonl                  # runtime trace
          preview.png                  # 대표 이미지
          captures/                    # sensor/image/data artifacts
            ...
```