# Next Actions

## Current Limitations

- OpenAI first / Ollama fallback provider chain is implemented.
- vector DB와 embedding index는 아직 구현하지 않았습니다.
- source document RAG는 아직 구현하지 않았습니다.
- 실제 UE actor spawn은 UE 팀 검증이 필요합니다.
- `obstacle.kickboard` prop ID는 UE 측 확인이 필요합니다.
- sample JSON과 fixture 파일은 의도적으로 자동 생성하지 않습니다.
- environment parameter sampler는 `WorldConfig`를 직접 생성하지 않고, 후속 generation constraints로 사용할 numeric parameter set만 생성합니다.
- `environmentSampling.enabled=true`이면 seed 기반 numeric parameter set을 `Numeric Environment Constraints`로 WorldConfig generation prompt와 deterministic post-processing에 연결합니다.
- environmentSampling 기반 단일 EpisodeSpec handoff smoke는 통과했으며, DOE matrix와 batch scenario generation은 후속 단계입니다.
- `responseFormat=setup_pair` live smoke는 통과했으며, fine-tuning candidate는 `data/fine_tuning_candidates/`에 로컬 저장하고 git commit 대상에서 제외합니다.
- EvaluationReport 기반 결과 분석과 Result Analysis Agent 구현은 별도 단계에서 진행합니다.

## Handoff Follow-Ups

1. UE 팀에 setup pair handoff package 전달
2. UE 팀에서 EpisodeSetup + DeliveryBotSetup 단일 pair compile과 실행 확인
3. UE 팀에서 parser integration, spawn mapping, route injection 확인
4. 최종 Kickboard prop ID 확정
5. UE 피드백에 따라 adapter 조정
6. Run Result API와 scoring 별도 단계 진행
7. DOE / scenario matrix / batch generation은 UE 단일 케이스 검증 후 진행
