# UE5 API Usage for UE Team

## 1. 서버 실행

```powershell
uv run uvicorn app.main:app --reload
```

## 2. Health check

* `GET /health`

## 3. Legacy API: scenario generation

Endpoint:

* `POST /api/v1/scenarios/generate`

이 endpoint는 현재 `410 RUN_QUEUE_REMOVED` 안내만 반환한다. 이전 RunQueue 응답 설명은 legacy handoff 기록으로만 남긴다.

## 3.1 Removed legacy handoff endpoints

아래 legacy handoff endpoint들은 현재 FastAPI route와 OpenAPI에서 제거되었다. 이 URL들은 현재 UE 연동용 정상 API가 아니다.

```text
POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec
POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=setup_pair
```

이전 `responseFormat=episode_spec`, `responseFormat=setup_pair`, `responseFormat=both`, `responseFormat=world_config` 설명은 archive/tooling 참고용이다.

EpisodeSpec JSON 계약의 기준 문서는 `docs/archive/previous_episode_spec/UE_EPISODE_SPEC_JSON_GUIDE.md`이다.

## 4. Legacy EpisodeSetup + DeliveryBotSetup pair 기록

이 섹션은 이전 UE handoff 기준 기록이다. 현재 사용자 project 실행 계약은 `contracts/specs/user-project-data.md`를 따른다.

이전 pair 계약 문서는 `contracts/specs/` 아래 legacy 문서다.

setup pair 생성은 legacy RunQueue export tooling 기준으로만 남긴다.

UE가 직접 요청에서 EpisodeSetup / DeliveryBotSetup JSON을 작성하지 않는다. Legacy tooling이 EpisodeSetup + DeliveryBotSetup pair들을 만들고, RunQueue의 `runs` 배열에서 각 pair의 export 경로를 반환한다.

RunQueue에서 UE가 읽어야 하는 필드:

* `schema`
* `version`
* `runs[].pair_id`
* `runs[].episode_setup`
* `runs[].delivery_bot_setup`

Legacy RunQueue JSON에는 `success`, `diagnostics`, raw LLM response, rawContent, API key 값은 포함하지 않는다.

## 5. Export CLI 사용법

아래 CLI는 public HTTP API가 아니라 내부 handoff service를 직접 호출하는 archive/tooling 경로다.

```powershell
uv run python scripts/export_ue5_handoff_payload.py --prompt "..." --provider openai --format episode_spec
uv run python scripts/export_ue5_handoff_payload.py --prompt "..." --provider openai --format setup_pair
```

* `--out`을 명시한 경우에만 파일 저장
* `--out` 없이 실행하면 콘솔 출력만 수행
