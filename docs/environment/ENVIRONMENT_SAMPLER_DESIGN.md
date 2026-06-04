# Environment Sampler Design

## 1. 목적

수치 후보값을 기반으로 seed deterministic environment parameter set을 생성한다.

## 2. 원칙

* low/middle/high를 JSON 값으로 사용하지 않는다.
* 같은 seed와 scenarioType이면 같은 결과를 낸다.
* sampler는 WorldConfig나 EpisodeSpec을 만들지 않는다.
* sampler 결과는 후속 WorldConfig generation constraints로 사용할 수 있다.

## 3. 입력

* seed
* scenarioType
* fixedParameters

## 4. 출력

* EnvironmentParameterSet

## 5. 후속 단계

* WorldConfig generation constraints와 sampler 연결
* DOE / Latin Hypercube / Sobol sampling
* scenario matrix 생성
* 반복 run configuration

## 6. Generation integration

`generationRequest.constraints.environmentSampling.enabled=true`이면 sampler 결과가 `Numeric Environment Constraints`로 prompt에 포함된다.
후처리는 sampled numeric values를 LLM 출력보다 우선 적용한다.
이 연결은 단일 요청용이며 DOE matrix나 batch generation은 만들지 않는다.
