# 다음 액션

## 1. UE 팀에 전달

다음 문서를 UE 팀에 전달합니다.

* `README.md`
* `UE_TEAM_MESSAGE_DRAFT.md`
* `UE_TEAM_HANDOFF_PACKAGE.md`
* `UE_INTEGRATION_HANDOFF_INDEX.md`
* `UE_HANDOFF_DELIVERY_MANIFEST.md`
* `OPENAI_FIRST_HANDOFF_RESULT.md`
* `ENVIRONMENT_SAMPLING_HANDOFF_RESULT.md`

## 2. UE 팀 확인 항목

* `EpisodeSpec` JSON parsing
* map, robot, obstacle, pedestrian, path actor spawn
* `obstacle.kickboard` prop ID 존재 여부
* route injection과 pedestrian path linkage
* controlled UE integration smoke
* OpenAI-first EpisodeSpec handoff 결과 확인
* environmentSampling EpisodeSpec handoff 결과 확인

## 3. AI 백엔드 후속 작업

* UE feedback 수신
* UE field name 또는 prop ID 변경 시 EpisodeSpec adapter 조정
* AI 내부 계약은 `WorldConfig`로 유지
* UE 실행 계약은 `EpisodeSpec`으로 유지
* OpenAI-first provider chain을 기본 경로로 유지하고 Ollama는 fallback provider로 유지

## 4. 이후 개발 후보

1. UE 팀에 EpisodeSpec handoff 문서와 최신 결과 전달
2. UE에서 EpisodeSpec parser / actor spawn / route injection 확인
3. UE feedback 기반 adapter 조정
4. Run Result 수신 API 설계
5. Evaluation scoring 설계
6. DOE / scenario matrix / batch generation은 UE 단일 케이스 검증 후 진행
7. 추가 scenario support 검토

## 5. 미팅 이후 우선순위

* UE actual integration check
* Run Result API design
* Evaluation scoring design
* Run Result analysis design
* DOE / scenario matrix / batch generation은 UE 단일 케이스 검증 이후 진행

현재 단계에서는 sample JSON, fixture 파일, vector DB, embedding index, UE C++/Blueprint 코드를 생성하지 않습니다.
