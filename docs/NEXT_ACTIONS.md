# 다음 액션

## 1. UE 팀에 전달

다음 문서를 UE 팀에 전달합니다.

* `README.md`
* `UE_TEAM_MESSAGE_DRAFT.md`
* `UE_TEAM_HANDOFF_PACKAGE.md`
* `UE_INTEGRATION_HANDOFF_INDEX.md`
* `UE_HANDOFF_DELIVERY_MANIFEST.md`
* `OPENAI_FIRST_HANDOFF_RESULT.md`

## 2. UE 팀 확인 항목

* `EpisodeSpec` JSON parsing
* map, robot, obstacle, pedestrian, path actor spawn
* `obstacle.kickboard` prop ID 존재 여부
* route injection과 pedestrian path linkage
* controlled UE integration smoke
* OpenAI-first EpisodeSpec handoff 결과 확인

## 3. AI 백엔드 후속 작업

* UE feedback 수신
* UE field name 또는 prop ID 변경 시 EpisodeSpec adapter 조정
* AI 내부 계약은 `WorldConfig`로 유지
* UE 실행 계약은 `EpisodeSpec`으로 유지
* OpenAI-first provider chain을 기본 경로로 유지하고 Ollama는 fallback provider로 유지

## 4. 이후 개발 후보

* `ENVIRONMENT_PARAMETER_SPEC` 기반 seed sampler를 WorldConfig generation constraints와 연결
* 수치 후보 기반 scenario matrix 설계
* UE controlled scenario batch test
* Run Result receive API
* Evaluation scoring
* 추가 scenario support
* DOE 또는 test matrix reduction 기반 scenario sampling

현재 단계에서는 sample JSON, fixture 파일, vector DB, embedding index, UE C++/Blueprint 코드를 생성하지 않습니다.
