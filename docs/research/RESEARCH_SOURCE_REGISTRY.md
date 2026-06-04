# RESEARCH_SOURCE_REGISTRY

## 목적

이 문서는 배달 로봇 시뮬레이션, 시나리오 테스트, 실험 조합 축소, LLM 자동화 구조의 근거로 사용할 공개 연구·방법론 source를 관리한다.

RSR source는 아직 RAG에 반영하지 않는다. 각 source는 검토 후 `reviewed` 처리되고, 필요한 경우에만 정책 또는 실험 설계 knowledge card 후보로 전환한다.

## RSR Source 목록

| Source ID | 제목 | Source Type | URL | 프로젝트 활용 목적 | Status |
| --- | --- | --- | --- | --- | --- |
| RSR-001 | Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists | report | https://www.metrans.org/assets/research/psr_finalreport_21-16.pdf | 보행자/자전거 상호작용, near miss, PET, 배달 로봇 위험 시나리오 검토 | to_review |
| RSR-002 | Composable and executable scenarios for simulation-based testing of mobile robots | paper | https://www.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2024.1363281/full | 실행 가능한 모바일 로봇 시나리오와 시나리오 변동성 검토 | to_review |
| RSR-003 | NIST Combinatorial Methods for Trust and Assurance | guideline | https://csrc.nist.gov/projects/automated-combinatorial-testing-for-software | 조합 테스트, DOE, 입력 공간 커버리지와 테스트 매트릭스 축소 검토 | to_review |
| RSR-004 | The Scenic Programming Language | guideline | https://scenic-lang.org/ | 시나리오 명세, 확률적 scene 생성, simulator integration 검토 | to_review |
| RSR-005 | Eureka: Human-Level Reward Design via Coding Large Language Models | paper | https://eureka-research.github.io/ | LLM 기반 평가 함수와 점수화 기준 생성 구조 검토 | to_review |
| RSR-006 | DrEureka: Language Model Guided Sim-To-Real Transfer | paper | https://eureka-research.github.io/dr-eureka/ | 월드 파라미터 샘플링, domain randomization, LLM 자동화 구조 검토 | to_review |

## KOR 문서와 RSR 문서의 차이

- KOR: 한국 법·인증·운행 기준 문서이며, 실외이동로봇 운행과 인증 관련 법적·제도적 근거를 관리한다.
- RSR: 시나리오 테스트, 실험 설계, 조합 축소, LLM 자동화 구조의 연구·방법론 근거를 관리한다.

## 처리 원칙

- HTML source는 원문 전체를 저장하지 않고 URL만 source registry에 등록한다.
- PDF로 직접 다운로드 가능한 source만 `data/sources/raw/research/`에 저장한다.
- RSR-001은 local PDF로 processed Markdown을 생성한다.
- RSR-002~RSR-006은 URL-only source로 관리하며, 원문을 확보하기 전에는 processed Markdown을 만들지 않는다.
- RSR 문서도 `reviewed` 처리 전에는 policy knowledge card로 변환하지 않는다.
- RSR 문서는 정책 직접 근거가 아니라 시나리오 설계, 실험 조합 축소, LLM 자동화 구조 근거로 사용한다.
